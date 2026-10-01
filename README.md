# elite_help_tool
Elite Dangerous companion: journal tracking and in-game overlay for exploration, exobiology and BGS

EHT (Elite Help Tool) is a companion for Elite Dangerous that follows the game's journal as you play and draws what matters straight into the game through a Vulkan overlay. It started as a way to plan exploration, deciding which planets are worth mapping and in what order. Since then it has grown to cover exobiology with species prediction and a codex, BGS work with tick tracking and influence history, missions, trade and in-game screenshots. Written in C++23, out of my own love of exploring the galaxy.

![at_work](at_work.png)

> **Linux only, for now.** EHT and its in-game overlay are built and tested only on Linux, with
> Elite Dangerous running under Proton.

## What it does

- **[In-game overlay](doc/overlay.md)** drawn straight into the frame by a Vulkan layer: side bands, head-up readouts in a dogfight, the system map, GPU/CPU temperatures, a corrected jump panel.
- **[BGS](doc/bgs.md)** tick tracking, influence history, wars and settlement ownership, and a territory table of your own factions' systems.
- **[Space missions](doc/missions_space.md)** and **[on-foot missions](doc/missions_on_foot.md)**: what is open, where to go, what a hand-in really pays, who probably holds a bounty on you, settlement job boards.
- **[Trade](doc/trade.md)**: best known trades against every market you have opened, fleet carrier bars, bartender sales, which missions pay best at your own bar.
- **[Colonisation](doc/colonisation.md)**: what a construction site still needs and who can supply it, tracked from every docking and delivery.
- **[Fleet carriers](doc/fleet_carriers.md)** and **[your ships](doc/ships.md)**: where each one is, where it is going, what is on board.
- **[Exploration and exobiology](doc/exploration.md)**: what is worth mapping, species prediction from your own sampling history, notable stellar phenomena flagged on arrival, an automatic photo codex and sky album. And a curiosity from the game's own sales: [the bodies you never scanned are paid for too](doc/unscanned_bodies.md).
- **[Surface navigation](doc/surface_navigation.md)** to a place given as coordinates, or to a codex entry.
- **[Neutron routes](doc/neutron_routes.md)**: following a route plotted by Spansh.
- **[Privacy](doc/privacy.md)**: nothing is downloaded from public databases; EDDN uploads are opt-in, exploration-only and only for empty, undiscovered systems.

## Documentation

The [documentation index](doc/index.md) lists everything above in more detail, plus building and
running EHT, the on-disk database layout, backups and known issues.

## In-game overlay screenshots

Each page above has a real screenshot of the feature it describes, cropped from actual play. System
and faction names that would identify the author's own BGS systems are blurred out; nothing else is.
