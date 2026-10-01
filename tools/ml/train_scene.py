"""Scene classifier probe: MobileNetV3-small trained on the manifest scene_labels.py writes.

The probe proves the chain frames -> labels -> model -> report -> ONNX -> ncnn without any hand
work; the scene itself is already known from Status.json. Validation holds out whole
days (a frame of the same minute looks like its neighbour, so a random split would
flatter the model). With one day only it falls back to holding out ten minute blocks
and says so in the report.

Runs on the CPU at the idle scheduling class. The game is not pinned and spreads its
threads over every core, and a sibling hyperthread or a shared L3 slows it whatever the
priority, so train with the game off; beside it, only on few threads of the second CCD:
    taskset -c 8-15,24-31 python train_scene.py manifest.csv -o run/ --threads 4
"""

import argparse
import collections
import csv
import json
import os
import pathlib
import subprocess
import sys
import time

import numpy as np
import torch
import torchvision
from PIL import Image
from torchvision.transforms import v2

from scene_labels import CLASSES

BLOCK_MS = 10 * 60 * 1000


def split_rows(rows, val_every):
    """Train and validation rows, and how they were split."""
    days = sorted({r["day"] for r in rows})
    if len(days) >= 2:
        # the latest days are held out, about one in val_every of them, at least one
        held = set(days[-max(1, len(days) // val_every):])
        how = "by day, held out " + ", ".join(sorted(held))
        return [r for r in rows if r["day"] not in held], [r for r in rows if r["day"] in held], how
    # blocks of every scene apart, so each scene with two blocks or more has one held out
    blocks = {}
    for r in rows:
        blocks.setdefault((r["label"], r["commander"], int(r["taken_ms"]) // BLOCK_MS), []).append(r)
    by_label = collections.defaultdict(list)
    for key in sorted(blocks):
        by_label[key[0]].append(key)
    train, val = [], []
    for keys in by_label.values():
        held = {k for i, k in enumerate(keys) if i % val_every == val_every - 1}
        if not held and len(keys) >= 2:
            held = {keys[-1]}
        for k in keys:
            (val if k in held else train).extend(blocks[k])
    return train, val, f"one day only - ten minute blocks, every {val_every}th of each scene held out (optimistic)"


def load_images(rows, size):
    w, h = size
    out = torch.empty((len(rows), 3, h, w), dtype=torch.uint8)
    for i, r in enumerate(rows):
        with Image.open(r["path"]) as im:
            im = im.convert("RGB").resize((w, h), Image.BILINEAR)
        out[i] = torch.from_numpy(np.array(im)).permute(2, 0, 1)
    return out


def make_model(n_classes, pretrained):
    weights = torchvision.models.MobileNet_V3_Small_Weights.IMAGENET1K_V1 if pretrained else None
    model = torchvision.models.mobilenet_v3_small(weights=weights)
    model.classifier[-1] = torch.nn.Linear(model.classifier[-1].in_features, n_classes)
    return model


def evaluate(model, images, targets, normalize, batch):
    model.eval()
    preds = []
    with torch.no_grad():
        for i in range(0, len(images), batch):
            x = normalize(images[i:i + batch].float() / 255)
            preds.append(model(x).argmax(1))
    return torch.cat(preds) if preds else torch.empty(0, dtype=torch.long)


def report(names, targets, preds):
    n = len(names)
    matrix = torch.zeros((n, n), dtype=torch.long)
    for t, p in zip(targets.tolist(), preds.tolist()):
        matrix[t, p] += 1
    per_class = {}
    for i, name in enumerate(names):
        total = int(matrix[i].sum())
        said = int(matrix[:, i].sum())
        per_class[name] = {
            "frames": total,
            "recall": float(matrix[i, i]) / total if total else None,
            "precision": float(matrix[i, i]) / said if said else None,
        }
    accuracy = float(matrix.trace()) / max(1, int(matrix.sum()))
    return accuracy, per_class, matrix


def format_report(how, names, skipped, train_counts, accuracy, per_class, matrix, seconds):
    lines = [f"split: {how}", f"accuracy: {accuracy:.3f}  ({int(matrix.sum())} validation frames, {seconds:.0f} s)"]
    if skipped:
        lines.append("left out, too few frames: " + ", ".join(f"{k} {v}" for k, v in skipped.items()))
    lines.append("")
    lines.append(f"{'class':17} {'train':>6} {'val':>5} {'recall':>7} {'precision':>9}")
    for name in names:
        c = per_class[name]
        fmt = lambda v: "-" if v is None else f"{v:.2f}"
        lines.append(f"{name:17} {train_counts[name]:6} {c['frames']:5} {fmt(c['recall']):>7} {fmt(c['precision']):>9}")
    lines.append("")
    lines.append("confusion (rows = true, columns = said):")
    short = [n[:6] for n in names]
    lines.append(" " * 17 + "".join(f"{s:>7}" for s in short))
    for i, name in enumerate(names):
        lines.append(f"{name:17}" + "".join(f"{int(v):7}" for v in matrix[i]))
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("manifest", type=pathlib.Path)
    ap.add_argument("-o", "--output", type=pathlib.Path, default=pathlib.Path("run"))
    ap.add_argument("--width", type=int, default=384)
    ap.add_argument("--height", type=int, default=216)
    ap.add_argument("--epochs", type=int, default=8)
    ap.add_argument("--batch", type=int, default=32)
    ap.add_argument("--min-frames", type=int, default=20, help="a class with fewer training frames is left out")
    ap.add_argument("--val-every", type=int, default=5)
    ap.add_argument("--threads", type=int, default=4)
    ap.add_argument("--no-pretrained", action="store_true")
    args = ap.parse_args()

    # below nice 19: runs only when a core has nothing else to do
    os.sched_setscheduler(0, os.SCHED_IDLE, os.sched_param(0))
    torch.set_num_threads(args.threads)
    torch.manual_seed(1)
    args.output.mkdir(parents=True, exist_ok=True)

    with open(args.manifest) as f:
        rows = list(csv.DictReader(f))
    train_rows, val_rows, how = split_rows(rows, args.val_every)
    counts = collections.Counter(r["label"] for r in train_rows)
    names = [c for c in CLASSES if counts[c] >= args.min_frames]
    skipped = {c: counts[c] for c in CLASSES if 0 < counts[c] < args.min_frames}
    index = {n: i for i, n in enumerate(names)}
    train_rows = [r for r in train_rows if r["label"] in index]
    val_rows = [r for r in val_rows if r["label"] in index]
    print(f"split {how}: {len(train_rows)} train, {len(val_rows)} validation, {len(names)} classes")

    size = (args.width, args.height)
    started = time.monotonic()
    train_x = load_images(train_rows, size)
    val_x = load_images(val_rows, size)
    train_y = torch.tensor([index[r["label"]] for r in train_rows])
    val_y = torch.tensor([index[r["label"]] for r in val_rows])
    print(f"images loaded in {time.monotonic() - started:.0f} s")

    normalize = v2.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
    # players recolour the HUD, so the hue must not tell the scene; no flips, the HUD is not symmetric
    augment = v2.Compose([
        v2.RandomResizedCrop((args.height, args.width), scale=(0.8, 1.0), ratio=(1.6, 1.95), antialias=True),
        v2.ColorJitter(brightness=0.3, contrast=0.3, saturation=0.4, hue=0.5),
    ])

    model = make_model(len(names), not args.no_pretrained)
    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    steps = args.epochs * ((len(train_rows) + args.batch - 1) // args.batch)
    scheduler = torch.optim.lr_scheduler.OneCycleLR(optimizer, max_lr=1e-3, total_steps=steps)
    # every class equally often, the rare scenes are the interesting ones
    weights = 1.0 / torch.tensor([counts[names[y]] for y in train_y.tolist()], dtype=torch.float)
    sampler = torch.utils.data.WeightedRandomSampler(weights, len(train_rows), replacement=True)

    for epoch in range(args.epochs):
        model.train()
        order = list(sampler)
        total = 0.0
        for i in range(0, len(order), args.batch):
            idx = torch.tensor(order[i:i + args.batch])
            x = torch.stack([augment(img) for img in train_x[idx]]).float() / 255
            loss = torch.nn.functional.cross_entropy(model(normalize(x)), train_y[idx], label_smoothing=0.05)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            scheduler.step()
            total += loss.item() * len(idx)
        preds = evaluate(model, val_x, val_y, normalize, args.batch)
        accuracy = float((preds == val_y).float().mean()) if len(val_y) else 0.0
        print(f"epoch {epoch + 1}/{args.epochs}: loss {total / len(order):.3f}, validation {accuracy:.3f}, "
              f"{time.monotonic() - started:.0f} s")

    preds = evaluate(model, val_x, val_y, normalize, args.batch)
    accuracy, per_class, matrix = report(names, val_y, preds)
    text = format_report(how, names, skipped, counts, accuracy, per_class, matrix, time.monotonic() - started)
    print(text)
    (args.output / "report.txt").write_text(text)
    (args.output / "report.json").write_text(json.dumps({
        "split": how, "accuracy": accuracy, "classes": names, "per_class": per_class,
        "confusion": matrix.tolist(), "input": [3, args.height, args.width]}, indent=1))

    torch.save({"classes": names, "size": size, "state": model.state_dict()}, args.output / "scene.pt")
    # normalisation goes into the graph, the runtime feeds RGB scaled to 0..1
    exported = torch.nn.Sequential(normalize, model).eval()
    torch.onnx.export(exported, (torch.zeros(1, 3, args.height, args.width),), str(args.output / "scene.onnx"),
                      input_names=["rgb"], output_names=["scene"], dynamo=False)
    (args.output / "classes.txt").write_text("\n".join(names) + "\n")
    print(f"model in {args.output}/scene.onnx, classes in {args.output}/classes.txt")
    to_ncnn(args.output, exported, val_x[:16].float() / 255, args.width, args.height)


def to_ncnn(output, model, sample, width, height):
    """ONNX -> ncnn by pnnx, then the same frames through both to show the conversion kept the model."""
    pnnx = pathlib.Path(sys.executable).parent / "pnnx"
    if not pnnx.exists():
        print("pnnx is not installed, no ncnn model")
        return
    subprocess.run([str(pnnx), "scene.onnx", f"inputshape=[1,3,{height},{width}]"], cwd=output, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    import ncnn
    net = ncnn.Net()
    net.load_param(str(output / "scene.ncnn.param"))
    net.load_model(str(output / "scene.ncnn.bin"))
    with torch.no_grad():
        expected = model(sample).numpy()
    worst, same = 0.0, 0
    for i in range(len(sample)):
        ex = net.create_extractor()
        # clone, the Mat only points at the numpy buffer
        ex.input("in0", ncnn.Mat(sample[i].numpy()).clone())
        _, out = ex.extract("out0")
        got = np.array(out).ravel()
        worst = max(worst, float(abs(got - expected[i]).max()))
        same += int(got.argmax() == expected[i].argmax())
    print(f"ncnn model in {output}/scene.ncnn.param/.bin: same answer for {same} of {len(sample)} frames, "
          f"largest logit difference {worst:.4f}")


if __name__ == "__main__":
    main()
