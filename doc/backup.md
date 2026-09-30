# Backup

EHT keeps a copy of what cannot be rebuilt in `~/.backups/eht/<commander>/`, in the background, once a
month (`backup.every_days`, 30) or once 100 new pictures came into the codex and the sky album
(`backup.every_pictures`), whichever comes first.

- The journals go one `journals-2026-09.tar.zst` a month, some seventy times smaller than the text
  (zstd level `backup.level`, 9). A month is packed again only when a journal of it changed, so a
  month gone by is packed once. `tar -xaf journals-2026-09.tar.zst` unpacks it. `ehtdb.sqlite` and
  `galaxy.sqlite` are left out of the backup: both are rebuilt from these same journals.
- The `codex/` directory is copied as it is - JPGs do not pack - file by file, when missing or newer.
  Nothing is ever deleted from the backup.
- `live.sqlite` (markets, carrier cargo) holds readings the journals never carry - a market is gone
  the moment the game overwrites `Market.json` at the next docking, and a carrier's hold corrected by
  hand in the Data window is nowhere else. So it is copied whole, hot (SQLite's own backup API, safe
  to take while EHT is running and writing it), every time this backup runs, and again right after
  every market is read, rather than waiting for the monthly schedule.
- `last_backup.json` beside them says when the last one was and how many pictures there were then.
- `backup.dir` moves it, `backup.enabled` turns it off. Copying `~/.backups/eht` to another disk
  is then the whole backup.
