# Evidence of the settlement glare

Since an update of the game some rooms of Odyssey settlements are now and then lit so brightly that
nearly the whole screen is white. To report it with what the network did at that moment, EHT can
write the moments down for another tool to pick up. With `evidence.dir` set (empty, the default,
turns it off), while you are on foot inside a settlement's buildings with no menu open, a patch of
the middle screen (`evidence.measured_size`, 0.3 of its height) is taken quietly every 3 s
(`evidence.interval_ms`) and measured. When at least 60 % of it (`evidence.overexposed_pct`) is
burnt out - brighter than `evidence.burnt_out`, 245 of 255 - the whole middle screen is taken at
once and kept:

- `<dir>/screenshots/<moment>_eht.png` - the picture;
- `<dir>/markers/<moment>_eht.json` - the marker: `ts_utc` (the machine's clock, UTC with
  milliseconds), `kind` `auto-luma`, the brightness (`luma_mean`, `luma_p99`, `overexposed_pct`),
  the picture's path and size, and where it was - system, body, settlement and the journal's
  newest moment (`journal_ts`, the game's clock, which is not always the machine's).

Both files are written whole under a temporary name and renamed, so a tool watching the directory
never reads half of one. One white room makes one marker: the next waits for the screen to darken
again, or 5 minutes (`evidence.again_after_s`).
While the game runs, the state of its network connections goes every 2 s
(`evidence.netstate_interval_ms`) to the day's `<dir>/netstate-YYYY-MM-DD.jsonl`, again from what
any user may read - no packet capture, nothing as root. The game's sockets come from its
`/proc/<pid>/fd` (the process whose program is `EliteDangerous64.exe` itself - not the watchdog,
which names the game only among its arguments - and whose Wine prefix holds the journals, so two
accounts playing at once are told apart) and their state from the kernel's socket diagnostics: for TCP the round trip, its
variance, retransmissions and losses; for UDP the queues and the datagrams dropped. Beside them go
the system's UDP and TCP counters from `/proc/net/snmp` as deltas, and the number of default
routes. The lines hold the addresses the game talks to, other players' among them: keep them on
the machine and give them to nobody but Frontier. The daily logs are kept 2 days
(`evidence.keep_days`); an older day's file is deleted when a new day's starts.

While the game runs, the machine's traffic also goes every 10 s (`evidence.traffic_interval_s`, 0
writes none) to the day's `<dir>/traffic-YYYY-MM-DD.jsonl`, kept 30 days
(`evidence.traffic_keep_days`): `pid` (the game's, so two accounts are told apart), `seconds`
since the last line, `in_session` (Status.json says more than the main menu's nothing), the UDP
datagrams received and sent from `/proc/net/snmp` (`udp_in`, `udp_out` and per second), the bytes
of all interfaces but the loopback from `/proc/net/dev` (`rx_bytes`, `tx_bytes` and KiB per
second), `players` (the other players linked in the instance, see [the overlay](overlay.md)) and
the game's own figures of its last ten minutes from netLog when it has written them
(`netlog_machines`, `netlog_act1`, `netlog_act2`). The counts are the whole machine's - both
accounts' games and any other program's. It is written to learn how much traffic another player
brings when the game reaches them through its server alone, with no link of its own to them: once
a player showed in the contacts while netLog named no machine but Frontier's servers. No addresses
go into it.

Five minutes after a marker (`evidence.report_after_s`), once what followed it is in the logs
too, EHT writes a report into `<dir>/reports/<moment>/`: the picture, `marker.json`, the lines of
`netstate.jsonl` and `sensors.jsonl` from 10 minutes before (`evidence.report_before_s`) to 5
after, the game's journal and its network log (`netLog`, found beside a Steam library's game or
by the running game's directory; `evidence.netlog_dir` names it otherwise) of the same minutes,
and `report.md` to read first - the longest TCP round trip, retransmissions, UDP errors and drops,
the netLog's cancelled requests and zero hashes, the minute round the moment against the minutes
before, and the temperatures at the moment. `report.md` is written last, so a report with one is
complete.

A report still to come is promised by an empty file named after its marker in
`glare_reports_due/`, in the commander's EHT working directory, and the file goes once the report
is written. When EHT stops before then, it writes the report at its next start - at once if its
minutes have passed and the game's network log is found (for a launcher's game, once the game runs
again), with the logs still kept - so a restart loses no report. The promise lies by
each commander's own EHT, so the report takes the network log of the game that saw the glare.

For two minutes after a glare the overlay says so in red at the top of the right band -
`lighting defect detected 08:47:21, evidence kept` - the sign that the watch works.
