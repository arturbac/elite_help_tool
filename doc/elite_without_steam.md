# Running Elite Dangerous on Linux without Steam, with EHT

> **Linux only.** This describes how the author runs a second game account under Proton without
> the Steam client, with the EHT overlay. It is a working recipe, not a supported product. Paths
> and versions will differ on your machine.

## Why

- A second commander on a **Frontier account** (not linked to Steam) can play next to the Steam
  one, each with its own EHT.
- The Steam client does not have to be running, and does not mark the game as being played.

## What you need

- The game files. The simplest way is the Steam installation (`steamapps/common/Elite Dangerous`),
  which can be **shared** with the Steam account. Don't start both instances at the very same moment:
  the launchers race for files in the shared directory. Start the second one about ten seconds later.
- A Proton build (e.g. GE-Proton from `compatibilitytools.d`) and the Steam Linux Runtime
  (`steamapps/common/SteamLinuxRuntime_4`). These are just files; the Steam client itself is not
  used.
- A Frontier account to log in with in the game's launcher.
- EHT built, with the layer installed (see [building_and_running.md](building_and_running.md)).

## A separate prefix

The second account needs its own Wine prefix. The prefix holds the account's login, settings,
key bindings and **journals**. Its shader cache is separate too:

```
/games/ed-frontier/compatdata/359320     # the prefix (Proton creates pfx/ inside)
/games/ed-frontier/shadercache/359320
```

## The launcher script

A minimal script, following the one the author uses. Adjust the four paths at the top:

```bash
#!/usr/bin/env bash
set -euo pipefail

STEAM_ROOT=$HOME/.local/share/Steam
GAME_DIR="$STEAM_ROOT/steamapps/common/Elite Dangerous"
PROTON=$STEAM_ROOT/compatibilitytools.d/GE-Proton11-6-x86_64
ED_ROOT=/games/ed-frontier

mkdir -p "$ED_ROOT/compatdata/359320" "$ED_ROOT/shadercache/359320"

export STEAM_COMPAT_DATA_PATH=$ED_ROOT/compatdata/359320
export STEAM_COMPAT_SHADER_PATH=$ED_ROOT/shadercache/359320
export STEAM_COMPAT_CLIENT_INSTALL_PATH=$STEAM_ROOT
export STEAM_COMPAT_INSTALL_PATH=$GAME_DIR
export STEAM_COMPAT_APP_ID=359320
# no Steam client to talk to
export WINEDLLOVERRIDES=lsteamclient=d

# --- EHT overlay ---
export ENABLE_EHT_OVERLAY=1
export EHT_OVERLAY_CAPTURE=1
# a socket of its own, so it does not steal the Steam account's overlay
export EHT_OVERLAY_SOCKET=$HOME/.local/share/elite_help_tool/overlay-alt.sock

cd "$GAME_DIR"
exec "$STEAM_ROOT/steamapps/common/SteamLinuxRuntime_4/_v2-entry-point" \
  --verb=waitforexitandrun -- \
  "$PROTON/proton" waitforexitandrun \
  "$GAME_DIR/EDLaunch.exe" /novr
```

Notes:

- `/novr` skips the VR question. Add `/Steam` only if you want the Steam login back.
- Don't export `SteamAppId` or `SteamGameId`: those make the Steam client think the game is
  running. A side effect is that `PROTON_LOG` will not write a file, because Proton names the file
  after `SteamGameId`. To see the overlay's log, pipe stderr instead:
  `EHT_OVERLAY_DEBUG=1 ./ed-frontier.sh 2>&1 | tee /tmp/alted.log`.
- `HOME` stays your real home, so the Vulkan loader inside the container finds the layer manifest
  in `~/.local/share/vulkan/implicit_layer.d` by itself. No `VK_ADD_IMPLICIT_LAYER_PATH` and no bind
  mounts are needed.
- For several monitors, give this prefix a Wine virtual desktop too, and a KWin rule matching
  the class `steam_proton`: [wine_virtual_desktop.md](wine_virtual_desktop.md).
- Keep the game on X11 (XWayland), the Proton default. The F11 screenshot key does not work with
  `PROTON_ENABLE_WAYLAND=1`.

## EHT for this account

Give the second account its own working directory, with `journal-dir` pointing into **its**
prefix:

```sh
mkdir -p /games/ed-frontier/eht && cd /games/ed-frontier/eht
ln -s "/games/ed-frontier/compatdata/359320/pfx/drive_c/users/steamuser/Saved Games/Frontier Developments/Elite Dangerous" journal-dir

# first scan of this account's journals - deletes and rebuilds the databases here
/path/to/elite_help_tool/build/eht-release/journal_tailer --dir journal-dir

# optional: share market prices with the Steam account
ln -s ~/eht/steam/live.sqlite live.sqlite
```

Then start EHT from that directory, with the **same socket** as the game script:

```sh
cd /games/ed-frontier/eht
EHT_OVERLAY_SOCKET=$HOME/.local/share/elite_help_tool/overlay-alt.sock \
  /path/to/elite_help_tool/build/eht-release/ui/elite_help_tool
```

EHT resolves `journal-dir`, the databases, `eht_settings.json`, `codex/` and `screenshots/` from
the current directory, so start it from the account's directory, not from the build directory.

## Sharing species knowledge between accounts

The species EHT guesses on a new world come from your own sampling history. To let one account
use the other's finds, point `exploration.shared_galaxies` in each account's `eht_settings.json` at
the other account's `galaxy.sqlite`. It is read-only, and the codex stays private to each
commander.
