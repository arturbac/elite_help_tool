# Hot drop: how far out overspeed still works

An experiment, off by default.

## What a hot drop is

Flying into a port with Supercruise Assist, the time to the target on the HUD tells how fast the
ship flies:

| time to target | what it means |
|---|---|
| 0:07 | how the assist flies by itself |
| 0:06 | the least without overspeed, whoever flies the ship |
| 0:05 or less | overspeed, every time, unless corrected |

A **hot drop** is overspeed on purpose. The ship goes in at 0:05 or less, the throttle goes back to
75%, and the assist still drops out at the port, 1 Mm from it, even though the ship keeps speeding
up. What decides it is how fast the ship is at that point. Go into overspeed too early, or too deep
(0:04, 0:03), and the assist flies past the port without dropping. The ship then loses the extra
speed a while later and has to turn back.

How early is too early depends on two things:

- **the gravity around the port**: the body it circles, whether a planet, a moon or the star itself;
- **the ship**: how it turns matters more than its mass. A ship with large inertia, such as the
  Panther Clipper, takes a hot drop from far out. A very agile one, such as the Mandalay, only from
  close in.

None of this is written down anywhere, and neither the journal nor `Status.json` says the distance
or the time to the target. So EHT reads them off the screen and learns from every approach.

## What EHT records

While in supercruise towards a **port in space** (a station, an outpost, a fleet carrier), EHT asks
the overlay layer for a picture of the middle of the screen. That is twice a second while the port
is near, and once every two seconds further out. The picture is read with
[tesseract](https://github.com/tesseract-ocr/tesseract), and what EHT looks for is the label the
HUD writes beside the target's marker:

```
ANTONIADI CITY
238Ls
1:45
```

The text reader now and then takes a leading 0 for a 6 or an 8, or a 1 for a 4. So the time read is
checked against the distances: the time to the target is the distance divided by the speed at which
the distance fell over the last seconds. The minutes come from that estimate and the seconds as
read. A time still far from the estimate is replaced by it. The picture is deleted once read.

Nothing is recorded for planets, stars or ports on the ground, in a taxi, or in another
commander's ship.

Every approach ends up as one line in `hot_drop.jsonl`, in the directory EHT runs in, holding:

- the readings: distance and time to the target over time, plus the time exactly as read;
- the port, its system, and the body it circles with that body's kind, mass, radius and surface
  gravity. The body is attached the way the system map attaches it, by the same distance from the
  star, so it is known only for ports docked at before;
- the ship: its type, its id, and its mass with fuel and cargo;
- how the approach ended:
  - `dropped`: the journal's `SupercruiseDestinationDrop` at this port;
  - `overshot`: the distance grew again right by the port. The way back to it then counts as a new
    approach;
  - `broken_off`: anything else, such as another destination, a drop elsewhere, or turning away;
- a summary: whether there was overspeed, the distance at the start of the last way into it, the
  least time to the target on the way, and the speed at the last readings. Going in too early and
  turning hard to lose speed, then going in again nearer, is common, and only the last way in
  decides the outcome. It takes two readings in a row above 0:05 to end a way in, because a single
  one is more often a misread than the ship slowing down. The distance of the first way in and the
  number of ways in are written too.

Readings taken within 0.05 Ls of the port are the drop itself and say nothing about overspeed.

## On the overlay

The block stands on the middle screen, upper left, where the readout of an enemy goes. That spot
is empty in supercruise. During the approach it shows:

- the port and the body it circles, with that body's surface gravity;
- the furthest overspeed that still dropped at this port in this ship, as the suggestion. It is
  shown only while no nearer overspeed has passed the port;
- the nearest overspeed that passed the port;
- the last distance read, so you can see the reading works.

For half a minute after an approach ends, the block says how it ended.

For now the suggestion comes only from earlier approaches at the same port in the same ship type.
An estimate for a port never tried, from the gravity of its body and the ship's handling, needs
these records first.

## Turning it on

In `eht_settings.json`:

```json
"hot_drop": {
   "record": true,
   "near_every_ms": 500,
   "far_every_ms": 2000,
   "near_ls": 100,
   "size": 0.45
}
```

- `near_every_ms` / `far_every_ms`: how often the label is read, nearer and further than
  `near_ls` from the port.
- `size`: how much of the middle screen is read, as its height in a share of the screen's height,
  in the 16:9 shape. 0.45 of a 4K screen is 1728 x 972 pixels, some 5 MB a picture, written to the
  layer's spool and deleted right after.

EHT has to be built with tesseract (its development files and the English data, `eng.traineddata`).
Without it EHT still builds, and the hot drop records nothing.
