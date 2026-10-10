# Backup

EHT keeps a copy of what cannot be rebuilt in `~/.backups/eht/<commander>/`. The journals, the codex
and `live.sqlite` go in the background, once a month (`backup.every_days`, 30) or once 100 new
pictures came into the codex and the sky album (`backup.every_pictures`), whichever comes first.

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
- The settings go to `settings/`, looked at every ten minutes rather than monthly - a few small files,
  and a verification of the game's files (Steam or the launcher) may take them any day: it deletes
  what the game did not ship from the game's directory and puts its own files back as shipped.
  - `settings/options/`: the game's `Options` directory (Bindings, Graphics, Player, Audio), found
    beside the journals in the same Wine user's home.
  - `settings/game/`: from the top of the game's own directory (`Products/<product>`)
    `AppConfigLocal.xml`, `GraphicsConfiguration.xml` and every `.ini` file - the mods keep theirs
    there. Never a DLL: a mod's binary comes again from where it was built or downloaded. The
    directory is found beside a Steam library's journals, or from the running game; a game started
    by another launcher is found only while it runs, or set it with `backup.game_dir`.
  - `settings/eht_settings.json`: the tool's own settings.

  A file is copied when missing or newer. When a newer one differs, the copy it replaces is kept
  beside it with its time appended (`edworld.ini.20261004-183000`), so a setting broken later never
  overwrites the last good one; a file the game wrote again unchanged sets nothing aside.
- The closed session logs of edworld go to `mod-logs/`, looked at with the settings every ten minutes.
  edworld writes `edworld.log` in the game's directory (`edworld_eht.log` from its build that works with
  EHT) and, when the next game session starts, renames the last one `edworld.<UTC>.log`
  (`edworld_eht.<UTC>.log`). Each of those is compressed with zstd (level 3) into
  `mod-logs/<the same name>.zst`, read back, and only then deleted from the game's directory - a
  verification of the game's files would delete it anyway. The running session's log itself is never
  touched. `zstd -d` unpacks one.
- `last_backup.json` beside them says when the last one was and how many pictures there were then.
  When it cannot be read, or written after a backup, the log says so, since the backup then counts
  as due at every look. A directory that cannot be gone through to its end is named in the log with
  the reason; what was copied before stays, the rest waits for the next run.
- `backup.dir` moves it, `backup.enabled` turns it off. Copying `~/.backups/eht` to another disk
  is then the whole backup.
