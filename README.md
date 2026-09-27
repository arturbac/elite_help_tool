# elite_help_tool
Elite Dangerous exploration planet visiting optimisation tool

This is a realtime tool that helps explore a system, map valuable planets and optimise the visiting route. I wrote it in a few hours in C++23, for my own pleasure of exploration in Elite Dangerous.

![at_work](at_work.png)

## Exploration

The bottom right corner of the overlay follows the order of work in a new system:

1. **arrival** — whether the arrival star was discovered before ("discovered before - jump on") or is
   `UNDISCOVERED`, and how many bodies from the honk have been scanned already,
2. **to map** — bodies worth the probes, most valuable first, green when it is a first discovery,
3. **life** — bodies with biological signals and, for each genus, the species it most likely is on
   this world, with its price; green when it is worth landing for (`bio_worth`, 5M).

The species is guessed from your own history. Every species sampled is stored together with its
world (planet class, atmosphere, temperature, gravity, the star above it), and a genus on a new world
is compared with what has already been found under the same atmosphere. On 374 species from the
archive, guessing each one from the rest, the first answer is right in 302 cases and one of the first
two in 349. The more samples, the narrower the guess.

On the surface, in the place of the target panel left of centre, the sample in progress is shown:
species, price, 1/3–3/3 and the distance from the nearest earlier sample against the colony range,
counted live from `Status.json` — a green "sample" when you may take the next one. Below it, what else
grows on this body.

## Codex

With every sample the layer takes a picture of the centre of the screen (see `overlay/README.md`), and
the tool puts it in `codex/pictures/` next to itself, described in `codex/pictures.json`.
`codex/index.html` is written from the database at start and after every sample: family by family,
species by species — price, number of finds, temperature and gravity range, atmospheres, worlds and
stars, pictures, a table of systems and bodies, and what of the family has not been found yet. The
*Codex* button opens it. The pictures live in the directory, not in the database: the database can be
rebuilt from the journals, the pictures cannot.

Settings are in the `exploration` section of `eht_settings.json`: `bio_worth`, `bio_bodies`,
`candidates`, `capture`, `capture_size`, `codex_dir`, `jpeg_quality`.

## BGS tick

How EHT works out from the journals when the daily influence and war ticks happened, even though
systems are visited rarely: [doc/bgs_tick.md](doc/bgs_tick.md).

## Three databases

The data is split into three files lying side by side, because they differ in origin and owner:

| file | holds | rebuilt from journals | shared between accounts |
|---|---|---|---|
| `ehtdb.sqlite` | missions, loot, reputation, scanning progress | yes | **no** — belongs to one commander |
| `galaxy.sqlite` | systems, bodies, stations, factions, influence, conflicts | yes | yes |
| `live.sqlite` | markets, prices, the bartender's shelf | **no** | yes |

`ehtdb` is split from `galaxy` for one concrete reason: the world is the same for every commander,
but "what I have scanned and mapped" is not. Showing one commander that they mapped a planet which was
really mapped by the other leads straight to a wrong decision when planning a flight. That is why
`system_progress`, `body_progress`, `genus_progress` and `faction_reputation` stay in the personal
database and are keyed **naturally** — by system address, body id, faction name — and not by `oid`s,
which change with every rebuild of `galaxy.sqlite`.

Thanks to this two accounts can point by symlink at the same `galaxy.sqlite` and `live.sqlite`,
sharing knowledge of the galaxy and prices, while keeping their own missions, reputation and
exploration progress.

`journal_tailer` deletes and rebuilds `ehtdb.sqlite` and `galaxy.sqlite`; it leaves `live.sqlite`
untouched, because its contents cannot be rebuilt from the journals.
