"""Scene labels for the frames eht_vision records, taken from Status.json alone.

Walks one or more dataset directories (each holds day directories with frames.jsonl
and status.jsonl, see doc/vision.md), gives every frame a scene label and turns down
the frames whose label cannot be trusted. Writes a manifest the trainer reads.

    python scene_labels.py /ext/.../eht-sjona/vision /ext/.../eht/vision -o manifest.csv
"""

import argparse
import bisect
import collections
import csv
import json
import pathlib
import sys

# GuiFocus values from the journal manual
GUI_FOCUS = {
    1: "panel", 2: "panel", 3: "panel", 4: "panel",
    5: "station_services",
    6: "galaxy_map",
    7: "system_map", 8: "system_map",
    9: "fss",
    10: "dss",
    11: "codex",
}

FLAG_DOCKED = 1 << 0
FLAG_LANDED = 1 << 1
FLAG_SUPERCRUISE = 1 << 4
FLAG_IN_SRV = 1 << 26
FLAG_FSD_JUMP = 1 << 30
FLAG2_ON_FOOT = 1 << 0

CLASSES = [
    "normal", "supercruise", "hyperspace", "docked", "landed", "srv", "on_foot",
    "panel", "station_services", "galaxy_map", "system_map", "fss", "dss", "codex",
]

# Status.json lags behind the picture; a frame this close to a change may show either side
CHANGE_GUARD_MS = 1000


def scene_label(status):
    """Scene of a Status.json object, or None when it says nothing about the screen."""
    flags = status.get("Flags", 0)
    flags2 = status.get("Flags2", 0)
    focus = status.get("GuiFocus", 0)
    # Flags 0 is the main menu or a status left over from an earlier session
    if flags == 0 and flags2 == 0:
        return None
    if focus in GUI_FOCUS:
        return GUI_FOCUS[focus]
    if flags2 & FLAG2_ON_FOOT:
        return "on_foot"
    if flags & FLAG_IN_SRV:
        return "srv"
    if flags & FLAG_FSD_JUMP:
        return "hyperspace"
    if flags & FLAG_DOCKED:
        return "docked"
    if flags & FLAG_LANDED:
        return "landed"
    if flags & FLAG_SUPERCRUISE:
        return "supercruise"
    return "normal"


def change_times(status_lines):
    """Times of the status changes that moved Flags, Flags2 or GuiFocus, sorted."""
    return sorted(s["ms"] for s in status_lines if s.get("flags_changed"))


def near_change(changes, ms, guard=CHANGE_GUARD_MS):
    i = bisect.bisect_left(changes, ms - guard)
    return i < len(changes) and changes[i] <= ms + guard


def read_jsonl(path):
    if not path.exists():
        return []
    out = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                try:
                    out.append(json.loads(line))
                except json.JSONDecodeError:
                    pass  # a line cut short by a stop of the recorder
    return out


def region_key(frame):
    return (round(frame["left"], 3), round(frame["top"], 3),
            round(frame["region_width"], 3), round(frame["region_height"], 3))


def label_day(day_dir):
    """Rows for one day directory and a counter of why frames were turned down."""
    frames = read_jsonl(day_dir / "frames.jsonl")
    changes = change_times(read_jsonl(day_dir / "status.jsonl"))
    rows = []
    dropped = collections.Counter()
    for frame in frames:
        path = day_dir / frame["file"]
        if not path.exists():
            dropped["missing file"] += 1
            continue
        label = scene_label(frame.get("status", {}))
        if label is None:
            dropped["no status"] += 1
            continue
        if near_change(changes, frame["taken_ms"]):
            dropped["near a change"] += 1
            continue
        rows.append({
            "path": str(path),
            "label": label,
            "day": day_dir.name,
            "commander": frame.get("commander", ""),
            "taken_ms": frame["taken_ms"],
            "region": "%g,%g,%g,%g" % region_key(frame),
        })
    return rows, dropped


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("datasets", nargs="+", type=pathlib.Path, help="vision dataset directories")
    ap.add_argument("-o", "--output", type=pathlib.Path, default=pathlib.Path("manifest.csv"))
    args = ap.parse_args()

    rows = []
    dropped = collections.Counter()
    for dataset in args.datasets:
        for day_dir in sorted(p for p in dataset.iterdir() if p.is_dir()):
            day_rows, day_dropped = label_day(day_dir)
            rows += day_rows
            dropped += day_dropped
    if not rows:
        sys.exit("no labelled frames")

    # the trainer keeps one region only; a frame of another region is a different picture
    regions = collections.Counter(r["region"] for r in rows)
    main_region = regions.most_common(1)[0][0]
    for r in rows:
        if r["region"] != main_region:
            dropped["other region"] += 1
    rows = [r for r in rows if r["region"] == main_region]

    with open(args.output, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    per_class = collections.Counter(r["label"] for r in rows)
    days = sorted({(r["day"], r["commander"]) for r in rows})
    print(f"{len(rows)} frames labelled into {args.output}, region {main_region}")
    print("turned down: " + (", ".join(f"{k} {v}" for k, v in dropped.most_common()) or "none"))
    print("days: " + ", ".join(f"{d} {c}" for d, c in days))
    for name in CLASSES:
        print(f"  {name:17} {per_class.get(name, 0)}")


if __name__ == "__main__":
    main()
