# Vision: recording what the screen shows

`eht_vision` is a separate process beside EHT. For now it only **records**: small pictures of the
game's screen, each saved with what `Status.json` and the journal said at that moment. The aim is
a dataset for teaching a small model to read what the game never writes into its files. The game
already supplies the labels (docked or not, which panel is open, on foot, in supercruise), so
nobody has to label a single picture by hand.

Nothing of it runs inside the game. The overlay layer already keeps a small copy of the middle
screen in a shared file (the *sample*, shrunk on the graphics card, at most 512 px). The recorder
only reads that file. It does no drawing and sends no input to the game.

## Turning it on

In `eht_settings.json`, in the directory EHT runs in:

```json
"vision": {
   "record": true,
   "dataset_dir": "vision",
   "every_ms": 1000,
   "size": 1,
   "width": 256,
   "min_difference": 2,
   "keep_every_s": 10,
   "limit_gb": 100,
   "shot_every_s": 60,
   "shot_after_change_ms": 1500,
   "shot_min_gap_s": 10,
   "shot_jpeg_quality": 90
}
```

While `record` is on, EHT asks the layer for the sample all the time, not only while flying at a
planet. Start `eht_vision record` in the **same directory** as EHT: it reads the same settings
file, and the journals and `Status.json` through the same `journal-dir` link. It finds the sample
through `EHT_OVERLAY_SOCKET`, as EHT does, so a second account with a socket of its own records its
own screen. `record` is read live, and switching it off stops the recording without restarting
anything.

The overlay has to be able to copy the game's image: `EHT_OVERLAY_CAPTURE=1` in the launch options
(see [Building and running](building_and_running.md)).

As a user service started and stopped together with EHT's own:

```ini
[Unit]
Description=eht_vision recorder
PartOf=eht.service
After=eht.service

[Service]
WorkingDirectory=/path/to/the/eht/directory
ExecStart=/path/to/elite_help_tool/build/eht-release/vision/eht_vision record
Restart=on-failure

[Install]
WantedBy=eht.service
```

| setting | meaning |
|---|---|
| `record` | on/off |
| `dataset_dir` | where the dataset goes, relative to the working directory, `~` for home |
| `every_ms` | at most one picture this often |
| `size` | the part of the middle screen: its height as a share of the screen's, in the 16:9 shape |
| `width` | about how wide; 256 gives the whole middle of a 4K screen as 480 × 270 |
| `min_difference` | a picture that differs less than this from the last one kept is skipped. The measure is the mean brightness difference of a 32 × 18 thumbnail, 0-255 |
| `keep_every_s` | but one is kept at least this often, even when nothing changes |
| `limit_gb` | above this the oldest days are deleted. The current day never is: when it alone is over the limit, recording stops until the next day |
| `shot_every_s` | a whole picture of the middle screen in full resolution at least this often, 0 for none |
| `shot_after_change_ms` | and one after `Flags`, `Flags2` or `GuiFocus` changed and then stayed the same this long |
| `shot_min_gap_s` | never two whole pictures closer than this |
| `shot_jpeg_quality` | the JPEG quality of the whole pictures |

A picture is also kept whenever the part of the screen changes (EHT asks for a different
rectangle while flying at a planet), and none is kept while the sample is older than 5 s (the game
paused, minimised, or not running).
None is kept either while `Status.json` has not been written for over 10 minutes and has neither
`Flags` nor `Flags2` set: that is the file left from an earlier session while the game sits in its
menus, and a picture labelled with it would be labelled wrong. The log says when this begins and when
`Status.json` is written again.

## Whole pictures

The small pictures are enough to tell which screen is open, but too small to read any text on
it. So now and then EHT also asks the layer for the whole middle screen in full resolution, the
way it asks for the codex photos: once a minute, and once after each change of the state, when
the state has settled and the screen has caught up with it. The layer takes the picture before
the overlay is drawn, so it shows the game alone. It writes the picture as a PPM into its spool
directory, beside the sample, named `<socket>_shot_<ms>_<why>.ppm`. The recorder turns it into a
JPEG in the day's directory and deletes the PPM. If no recorder takes it, EHT deletes it before
asking for the next one and says so in its log once. Any other picture EHT asks for at the same
moment (a codex photo, a scanner view) takes the turn, and that whole picture is skipped.

## What it writes

One directory per UTC day:

```
vision/2026-10-01/1790852680590.png   the picture, named after the moment it was drawn (ms)
vision/2026-10-01/frames.jsonl        a line per picture
vision/2026-10-01/status.jsonl        every new content of Status.json
vision/2026-10-01/events.jsonl        every journal event
vision/2026-10-01/1790870563480_change.jpg  a whole picture: when it was asked for, and why (every/change)
vision/2026-10-01/shots.jsonl         a line per whole picture
```

`frames.jsonl`:

```json
{"file":"1790852680590.png","taken_ms":1790852680590,"seq":2,"width":480,"height":270,
 "left":0.2866,"top":0,"region_width":0.4266,"region_height":1,"surface_width":9000,"surface_height":2160,
 "difference":-1,"commander":"...","socket":"overlay","status_ms":1790852680678,
 "status":{"timestamp":"...","event":"Status","Flags":16842765,"Flags2":0,"GuiFocus":0,...}}
```

- `taken_ms` is the system clock in milliseconds when the layer drew the frame.
- `left`, `top`, `region_width`, `region_height` give where the picture lies on the whole surface,
  as shares of `surface_width` and `surface_height`. They change over time, so every line has its
  own.
- `difference` is the brightness difference from the last picture kept (-1 for the first).
- `commander` comes from the journal and `socket` is the layer's socket. Both tell accounts apart.
- `status` is the whole `Status.json` as it was last read, and `status_ms` is when it was read.

`shots.jsonl` has `file`, `asked_ms`, `reason`, `width`, `height`, `commander`, `socket`, and
`status` with `status_ms` as in `frames.jsonl`. They are as the recorder knew them when it took the
picture, a fraction of a second after the layer wrote it.

`status.jsonl` has a line for every new content of `Status.json`: `ms` (when the recorder saw it),
`flags_changed` (`Flags`, `Flags2` or `GuiFocus` differ from the previous one) and the whole
`status`. The game writes `Status.json` a little after the screen has already changed, so a
training set should leave out pictures within about a second of a change of the flags. That is
what this file is for.

`events.jsonl` has one line per journal event: `ms` (when the recorder saw it), the game's
`timestamp` (whole seconds only) and the `event` name. The journal written before the recorder
started is not copied, because it is not what was on the screen. The full lines are still in the
journals themselves.

## How much

A 480 × 270 PNG of the game's screen is 150-190 kB. At one picture a second that is at most about
0.6 GB per hour of play, and less once the duplicates are skipped. A whole picture of a 4K middle
screen is roughly 1-2 MB as a JPEG, so a minute's picture adds about 0.1 GB per hour, plus one per
change of the state. `eht_vision` writes how much it
recorded to its log every 10 minutes.

## Training a first model

`tools/ml/` holds the offline side: Python with PyTorch in its own virtual environment, never
needed to run EHT. The first model is a probe. It tells the scene (supercruise, docked, on foot,
the galaxy map, the DSS and so on) from the small picture alone. The scene is already known from
`Status.json`, so the model adds nothing to EHT yet. It shows that the whole chain, from the
recorded pictures to a model a C++ program can load, works with no hand work.

```sh
cd tools/ml
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt --extra-index-url https://download.pytorch.org/whl/cpu
.venv/bin/python scene_labels.py ~/ed/eht/vision ~/ed/eht-other/vision -o manifest.csv
.venv/bin/python train_scene.py manifest.csv -o run/
```

`scene_labels.py` gives every picture its scene from the `status` beside it. `GuiFocus` comes first
(panels, station services, the maps, the FSS, the DSS, the codex), then on foot, the SRV, the jump,
docked, landed, supercruise, and normal space for the rest. It leaves out:

- pictures with `Flags` 0, which is the main menu or a status left from an earlier session,
- pictures within a second of a change in `status.jsonl`, which may show either side of it,
- pictures of any region other than the most common one (the approach to a planet asks for a
  different part of the screen).

It prints how many pictures each scene has. A scene needs a couple of hundred to be learnt well.

`train_scene.py` trains MobileNetV3-small, starting from the ImageNet weights, on the CPU. Each
scene is drawn equally often, and the colours are shifted at random, because players recolour
their HUD. Pictures are not mirrored, because the HUD is not symmetric. Validation holds out whole
days. Pictures a few seconds apart look alike, so a random split would flatter the model. With a
single day it holds out ten-minute blocks of each scene instead, and the report says the result is
optimistic. A scene with fewer than 20 training pictures is left out and named in the report.

In `run/` it writes `report.txt` (recall and precision per scene and a confusion matrix),
`report.json`, `scene.pt`, `scene.onnx` and `classes.txt`. It then converts the model to ncnn
(`scene.ncnn.param`, `scene.ncnn.bin`) with pnnx, runs the same pictures through both, and prints
how far apart their answers are. The ncnn model takes RGB scaled to 0..1, 384 × 216; the
normalisation is part of the graph.

On a 9950X a training of about 2,000 pictures takes three minutes on four cores. It runs at the
idle scheduling class, but that is not enough beside the game. The game spreads its threads over
every core, and training on a sibling hyperthread or beside it in the same L3 makes the game
stutter even with half the cores free. So train with the game off.
