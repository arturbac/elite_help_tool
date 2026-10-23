# Where you were at a place in a journal: `journal_context`

`journal_context` reads one journal file and tells where the commander was at a given byte of it, and what
happened in the seconds after. It reads the journal only: no database, nothing written.

```
build/eht-release/journal_context <journal file> <byte> [seconds]
```

- `<byte>` is a place in the file: everything before it is the past, the line starting at it or later the future.
  edworld_eht writes the journal's size at the moment it samples a hologram model (its `_context.json` beside the
  sample), so a sample can be placed in the journal afterwards.
- `[seconds]` is how far after the last event before the place the steps go on (60 when not given), on the
  journal's own clock.

It prints one JSON object:

| field | what it holds |
|---|---|
| `last_before` | the time of the last event before the place |
| `before` | the place up to the byte: `system`, `body`, `station`, `station_type`, `pad` (given and not yet left, 0 when none), `settlement`, `ship`, `where` |
| `after` | the same at the end of the window |
| `steps` | the events within the window: `seconds` after `last_before`, `event`, `what` |

`where` is one of `docked`, `normal space`, `supercruise`, `landed`, `on foot`, `SRV`, `taxi`, `unknown`.

The events are read the way the tool and `journal_tailer` read them: line by line, each line one event, the state
kept apart from them (`generic_state_t`). Besides the events the tool reads, it follows `LoadGame`, `Touchdown`,
`Liftoff`, `DockingRequested`, `ApproachBody` and `LeaveBody` for the place alone. A last line the game is still
writing is not read. The place before is only as good as the journal: a file begun mid-session starts with
`LoadGame` and `Location`, which set it.

Its first user is edworld's `tools/holo_db.py`, which describes the hologram models edworld samples by what was
happening around each sample.
