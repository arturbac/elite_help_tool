# elite_help_tool
Elite Dangerous companion: journal tracking and in-game overlay for exploration, exobiology and BGS

EHT (Elite Help Tool) is a companion for Elite Dangerous that follows the game's journal as you play and draws what matters straight into the game through a Vulkan overlay. It started as a way to plan exploration, deciding which planets are worth mapping and in what order. Since then it has grown to cover exobiology with species prediction and a codex, BGS work with tick tracking and influence history, missions, trade and in-game screenshots. Written in C++23, out of my own love of exploring the galaxy.

![at_work](at_work.png)

> **Linux only, for now.** EHT and its in-game overlay are built and tested only on Linux, with
> Elite Dangerous running under Proton.

## What it does

- **[In-game overlay](doc/overlay.md)** drawn straight into the frame by a Vulkan layer: side bands, head-up readouts in a dogfight, the system map, GPU/CPU temperatures, the frame rate in the corner of the middle screen, trouble with Frontier's servers told while it happens, the other players in the instance, a reminder of which graphics set fits the place.
- **[BGS](doc/bgs.md)** tick tracking, influence history, wars and settlement ownership, and a territory table of your own factions' systems.
- **[Space missions](doc/missions_space.md)** and **[on-foot missions](doc/missions_on_foot.md)**: what is open, where to go, what a hand-in really pays, who probably holds a bounty on you, settlement job boards.
- **[Trade](doc/trade.md)**: best known trades against every market you have opened, fleet carrier bars, bartender sales, which missions pay best at your own bar.
- **[Credits](doc/credits.md)**: the balance now, what this session earned and spent per hour, and the history by day, week, month, quarter or year in one column per kind of income and expense, with the squadron bank and colonisation payouts told from the game's own balance readings.
- **[Community goals](doc/community_goals.md)**: your contribution to each goal, your percentile band and whether it reaches the top 50% or top 75% bracket, with the time left.
- **[Colonisation](doc/colonisation.md)**: what a construction site still needs and who can supply it, tracked from every docking and delivery.
- **[Fleet carriers](doc/fleet_carriers.md)** and **[your ships](doc/ships.md)**: where each one is, where it is going, what is on board.
- **[Exploration and exobiology](doc/exploration.md)**: what is worth mapping, species prediction from your own sampling history, notable stellar phenomena flagged on arrival, an automatic photo codex and sky album. And a curiosity from the game's own sales: [the bodies you never scanned are paid for too](doc/unscanned_bodies.md).
- **[Surface navigation](doc/surface_navigation.md)** to a place given as coordinates, or to a codex entry.
- **[Neutron routes](doc/neutron_routes.md)**: following a route plotted by Spansh.
- **[Hot drop](doc/hot_drop.md)**: an experiment that reads the distance and the time to a port off the HUD in supercruise and learns, port by port and ship by ship, how far out overspeed still lets Supercruise Assist drop at the port.
- **[Vision](doc/vision.md)**: an optional recorder of small pictures of the screen, each with the game's own state as its label, as a dataset for teaching a model to read the screen.
  `tools/ml` trains a first model on it (a scene classifier) and converts it to ncnn.
- **[Backup](doc/backup.md)**: the journals, the codex and the market readings, and the settings a verification of the game's files would take - the game's bindings and graphics, `AppConfigLocal.xml`, the mods' `.ini` files - and the tool's own, and edworld's logs of the game sessions gone by.
- **[Privacy](doc/privacy.md)**: nothing is downloaded from public databases; EDDN uploads are opt-in, exploration-only and only for empty, undiscovered systems.

## Documentation

The [documentation index](doc/index.md) lists everything above in more detail, plus building and
running EHT, the on-disk database layout, backups and known issues.

## In-game overlay screenshots

Each page above has a real screenshot of the feature it describes, cropped from actual play. System
and faction names that would identify the author's own BGS systems are blurred out; nothing else is.
