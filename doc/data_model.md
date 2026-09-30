# Three databases

The data is split into three files lying side by side, because they differ in origin and owner:

| file | holds | rebuilt from journals | shared between accounts |
|---|---|---|---|
| `ehtdb.sqlite` | missions, loot, reputation, scanning progress, your ships | yes | **no** — belongs to one commander |
| `galaxy.sqlite` | systems, bodies, stations, factions, influence, conflicts | yes | yes |
| `live.sqlite` | markets, prices, the bartender's shelf | **no** | yes |

`ehtdb` is split from `galaxy` for one concrete reason: the world is the same for every commander,
but "what I have scanned, mapped or set foot on" is not. Showing one commander that they mapped a planet which was
really mapped by the other leads straight to a wrong decision when planning a flight. `body_progress`
tracks `mapped` from `SAAScanComplete` and `footfalled` from `Touchdown`, each column set independently
so mapping and landing do not overwrite one another. That is why
`system_progress`, `body_progress`, `genus_progress` and `faction_reputation` stay in the personal
database and are keyed **naturally** — by system address, body id, faction name — and not by `oid`s,
which change with every rebuild of `galaxy.sqlite`.

Thanks to this two accounts can point by symlink at the same `galaxy.sqlite` and `live.sqlite`,
sharing knowledge of the galaxy and prices, while keeping their own missions, reputation and
exploration progress.

`journal_tailer` deletes and rebuilds `ehtdb.sqlite` and `galaxy.sqlite`; it leaves `live.sqlite`
untouched, because its contents cannot be rebuilt from the journals.

Each of the three files carries its own schema version (SQLite's own `PRAGMA user_version`), bumped
whenever a table's shape changes in a way an older build could misread - a plain column added in
place does not count, every build already tolerates those. Opening a file stamped by a newer EHT than
the one running stops with a message rather than risk rewriting it: back it up, then rebuild
`ehtdb.sqlite`/`galaxy.sqlite` with `journal_tailer --dir <journals> --commander <FID>` (`live.sqlite`
has no such rebuild - keep the newer build's copy).
