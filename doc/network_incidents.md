# Network incidents: disconnects and technical failures, found automatically

Elite Dangerous shows its own disconnect dialogs when it loses touch with a server - a colour and a
ship name, "Orange Sidewinder", "Yellow Adder" and the like. None of it reaches any log: checked
against the whole of the game's netLog history, that label is never written down anywhere - it is
drawn by the client from an internal code. The **Network** window does not try to catch that label.
Instead, a background scanner reads the technical detail underneath it, automatically, from two
sources that already sit on disk:

- **netLog** (the game's own connection log, separate from the journal, kept beside the install in
  its `Logs` folder): bursts of checksum failures - several within a couple of seconds count as one
  incident - and structured `Disconnect: type=N&reason=...` lines naming things like `SaveFailed`,
  `DockingSaveFailed` or a mission timeout. Each disconnect also keeps what followed it: how long the
  game had heard nothing from its server when it gave up (`LastRx` on "Releasing server on
  disconnection" - the network went quiet that long before the disconnect), and the seconds until the
  next `ConnectToServerActivity: state=Init` (mostly the player clicking on) and the first `Connected:`
  (the server reached again). A database from before these were kept gets them from netLog read again.
- **the journal**: a completed session file with no `Shutdown` event in it. This is *not* read as a
  crash - on one commander's history checked while building this, about a fifth of all sessions ended
  this way, far more than real crashes would explain. Alt+F4, closing the window, or stopping the
  process any way other than the game's own exit menu leaves the same trace, and there is no way to
  tell those apart from the file alone. The row says "ended without Shutdown", nothing stronger.

Nothing here is typed in by hand. The first time the window runs, the scanner works back through
everything already on disk - which can take a little while on a long history - and every tick after
that only looks at what is new. What it found stays through restarts: a small progress marker (the
newest netLog and journal file names already gone through) means old history is not reread from
scratch every time EHT starts.

## The local-network verdict

The moment an incident is found, EHT reads the systemd journal for a service logging under the
syslog identifier `net-monitor` (an independent watch of the local network - ping to the LAN gateway
and to a public address, DNS through the local resolver and, failing that, straight through a public
one) for the ten minutes around it, and keeps that window with the incident, since journald will not
keep it open forever:

- **network clean** - nothing but successful checks in the window;
- **Unbound only** - the local DNS resolver alone failed while the network path itself tested fine;
- **local/upstream network down** - the gateway or the public ping failed, or DNS failed through
  both resolvers - a real problem on this side, whether it is the LAN, the router or the ISP;
- **no net-monitor log** - the service was not running, is not set up on this machine, or journald
  has since aged the window out.

This settles only one question - was the local network in trouble at that moment - not whether the
failure itself was Frontier's fault or this side's. The net-monitor lines themselves are kept with
the row and shown when it is picked, for whoever reads the report to judge for themselves.

## The community's glossary

Shown in the window as a legend only, to read by eye beside the game's own dialog if one happens to
be on screen at the time - no row is ever matched to one of these, since no log carries the code:

| Code | Meaning | Usually points to |
|---|---|---|
| Orange Sidewinder | Generic connection error to the Elite servers; the CMDR cannot load | local |
| Yellow/Black Adder | Unrecoverable transaction server error (CMDR/module data) | remote |
| Mauve Adder | Matchmaking server connection error | remote |
| Scarlet/Magenta Krait | Transaction server connection error | remote |
| Purple Python / Blue, Gold, Teal, Taupe Cobra | Mission/adjudication server errors | remote |
| Silver Fer-de-Lance | Multicrew connection timeout between players | local/P2P |

Not official Frontier documentation - gathered from the game's own forums and Steam Community, and
worth correcting or extending as more codes turn up.

## Setting up net-monitor.service yourself

EHT does not install or run this watch - it only reads its log if one is already there. Any script
that pings a gateway and a public address every few seconds, tries a DNS lookup through the local
resolver and, on failure, through a public one, and logs state changes through `logger -t
net-monitor` gives EHT everything this feature needs; no root is required to read your own user's
journal entries.

To plot the history as charts, see [Charts of your own connection incidents](incident_charts.md).
