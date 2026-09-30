# Building, installing and running EHT

> **Linux only, for now.** EHT and its in-game overlay are built and tested only on Linux, with
> Elite Dangerous running under Proton. The overlay is a Vulkan layer that hooks into
> DX11 → DXVK → Vulkan, and the screenshot key needs the X server (XWayland). None of this has
> been tried on Windows.

EHT is made of three parts:

| part | what it is |
|---|---|
| `journal_tailer` | command-line importer: reads **all** your journals into the databases |
| `ui/elite_help_tool` | the tool itself (Qt6): follows the newest journal live and feeds the overlay |
| `libeht_overlay.so` | Vulkan layer loaded **into the game's process**, hands the game's swapchains to the plugin |
| `libeht_overlay_plugin.so` | the overlay itself, loaded by the layer and reloaded when replaced |

## 1. Build

### Requirements

- CMake ≥ 3.28, Ninja
- **clang 19** or newer. The presets expect `clang++-19`, and the warning flags assume clang, so
  gcc will not build it. Plain `clang++` works too, see below
- Qt6: Widgets, Charts
- Boost ≥ 1.70: thread, program_options
- SQLite3, OpenSSL, zlib, zstd (the backup packs the journals with it), pkg-config
- libsystemd (reads net-monitor's log for the Network incidents window, no root needed)
- Vulkan headers (for the overlay)

- libxcb headers (the overlay loads libxcb at run time, from the game's process)
- **libstdc++ from GCC 14** or newer, since the code uses `std::println`, `std::ranges::to` and
  `std::chrono::current_zone`. clang uses the newest GCC installed, so installing `g++-14` next
  to an older default is enough

Everything else (glaze, spdlog, simple_enum, small_vectors, stralgo, imgui) is downloaded by CPM
at configure time, so `git` has to be installed too.

### Installing the dependencies

Only the Gentoo line is what the author builds on. The others follow each distribution's package
names and have not been tried.

**Debian 13 (trixie)** already has clang 19, GCC 14 and a new enough CMake:

```sh
sudo apt install git cmake ninja-build clang-19 \
  qt6-base-dev libqt6charts6-dev \
  libboost-thread-dev libboost-program-options-dev \
  libsqlite3-dev libssl-dev zlib1g-dev libzstd-dev libsystemd-dev pkg-config libvulkan-dev libxcb1-dev
```

Debian 12 is too old: its CMake is 3.25 and it has no Qt6 Charts package.

**Ubuntu 24.04 and derivatives:** Linux Mint 22.x (the usual pick for people coming from
Windows), Pop!_OS 24.04, Zorin OS 18. They share Ubuntu 24.04's packages. The default GCC there is
13, so install `g++-14` as well:

```sh
sudo apt install git cmake ninja-build clang-19 g++-14 \
  qt6-base-dev libqt6charts6-dev \
  libboost-thread-dev libboost-program-options-dev \
  libsqlite3-dev libssl-dev zlib1g-dev libzstd-dev libsystemd-dev pkg-config libvulkan-dev libxcb1-dev
```

If `clang-19` is not found, enable the *universe* repository, or take it from
[apt.llvm.org](https://apt.llvm.org). Linux Mint 21 and Ubuntu 22.04 are too old (no Qt6 Charts,
CMake 3.22).

**Arch Linux** (and CachyOS, EndeavourOS, Manjaro):

```sh
sudo pacman -S --needed base-devel git cmake ninja clang \
  qt6-base qt6-charts boost sqlite openssl zlib zstd systemd vulkan-headers vulkan-icd-loader libxcb
```

Arch names its compiler plain `clang++`, so configure with
`cmake --preset eht-release -DCMAKE_CXX_COMPILER=clang++`.

**Gentoo** (`dev-qt/qtbase` needs `USE="gui widgets"`):

```sh
emerge --ask --noreplace dev-vcs/git dev-build/cmake dev-build/ninja llvm-core/clang:19 \
  dev-qt/qtbase:6 dev-qt/qtcharts:6 dev-libs/boost dev-db/sqlite app-arch/zstd dev-libs/openssl \
  virtual/zlib sys-apps/systemd dev-util/vulkan-headers media-libs/vulkan-loader x11-libs/libxcb
```

**Immutable gaming systems** (Bazzite, SteamOS) have no package manager for development
libraries. Build inside a [distrobox](https://distrobox.it) with Arch or Debian 13 and the matching
line above, and run EHT inside it as well, since it needs that box's Qt and Boost. distrobox shares
`$HOME` and the display. The overlay layer carries its own C++ runtime, so after `overlay-install`
the game on the host loads it from `$HOME` as usual.

### Configure and build

```sh
git clone https://github.com/arturbac/elite_help_tool.git
cd elite_help_tool
cmake --preset eht-release        # configures into build/eht-release
cmake --build build/eht-release
```

If your clang has a different name, override it:
`cmake --preset eht-release -DCMAKE_CXX_COMPILER=clang++`.

Other presets: `eht-debug`, `eht-release-asan`, `eht-release-tsan`, `eht-release-unit-tests`.

## 2. Install the overlay layer

```sh
ninja -C build/eht-release overlay-install
```

This copies:

| file | to |
|---|---|
| `libeht_overlay.so`, `libeht_overlay_plugin.so` | `~/.local/lib/` |
| `eht-overlay-run` (optional wrapper) | `~/.local/bin/` |
| `eht_overlay.json` (layer manifest) | `~/.local/share/vulkan/implicit_layer.d/` |

It has to go under your home directory: the game runs inside the Steam Linux Runtime container,
which sees `$HOME` but not your build directory. The layer does nothing until `ENABLE_EHT_OVERLAY=1`
is set, so installing it does not affect other games.

**After every change in `overlay/`, run `overlay-install` again.** A running game picks up the new
plugin within a second; only a change of the layer itself (`vk_layer.cc`, `plugin_api.h`) needs the
game restarted. Changes only in the tool need only a restart of the tool.

To check that the game's container can see the layer, without starting the game:

```sh
ENABLE_EHT_OVERLAY=1 ~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/run -- \
  vulkaninfo --summary | grep EHT
```

## 3. Prepare a working directory

EHT keeps its databases and settings in the directory **it is started from**, and it looks for the
journals through a link named `journal-dir` there. Pick one directory per game account, **outside
the build tree**: a test binary or a fresh build run with the build directory as its working
directory writes its own `galaxy.sqlite`/`live.sqlite` there on start, silently wiping the live
ones — not recoverable from journals.

```sh
mkdir -p ~/eht/steam && cd ~/eht/steam
ln -s "$HOME/.local/share/Steam/steamapps/compatdata/359320/pfx/drive_c/users/steamuser/Saved Games/Frontier Developments/Elite Dangerous" journal-dir
```

That is where Proton keeps the journals for the Steam version (appid 359320). If your Steam
library is elsewhere, the path starts from that library's `steamapps/compatdata/359320`.

## 4. First scan of the journals

The tool itself follows only the **newest** journal. Your history (systems visited, bodies scanned,
factions, missions, BGS influence) goes in once, with `journal_tailer`:

```sh
cd ~/eht/steam
/path/to/elite_help_tool/build/eht-release/journal_tailer --dir journal-dir
```

> **`journal_tailer` deletes and rebuilds `ehtdb.sqlite` and `galaxy.sqlite` in the current
> directory.** Run it only where you want the databases rebuilt. `live.sqlite` (market prices) is
> left alone, because it cannot be rebuilt from journals.

It takes a while for months of journals. Options:

| option | meaning |
|---|---|
| `--dir <path>` | journal folder (default: current directory) |
| `--commander <FID>` | whose career goes into `ehtdb.sqlite`; empty = the commander of the newest journal, `all` = no distinction |
| `--ticks [days]` | print the observed BGS tick waves from the existing database, without importing (see [bgs_tick.md](bgs_tick.md)) |
| `--bgs [days]` | print mission effort against the influence it moved, day by day |
| `--wars` | print how long after being announced the wars actually started |

What goes where is described in [data_model.md](data_model.md), under *Three databases*.

## 5. Run EHT

```sh
cd ~/eht/steam
/path/to/elite_help_tool/build/eht-release/ui/elite_help_tool
```

Start it from the working directory: `journal-dir`, the databases and `eht_settings.json` are all
relative to it. On the first start it writes `eht_settings.json` with every setting at its default.
The file is reloaded live whenever you save it.

EHT and the game can be started in either order. The overlay connects whenever both are up, and
reconnects after either one restarts.

For a launcher or a desktop entry, the tool's icon is `ui/icons/eht.svg` (a 256 px `eht.png` beside
it). The desktop entry has to start EHT in its working directory: `Path=` set to it, or a user
service with `WorkingDirectory=`.

## 6. Steam launch options

In Steam: *Elite Dangerous → Properties → General → Launch Options*:

```
ENABLE_EHT_OVERLAY=1 EHT_OVERLAY_CAPTURE=1 %command%
```

| variable | meaning |
|---|---|
| `ENABLE_EHT_OVERLAY=1` | turns the layer on (required) |
| `EHT_OVERLAY_CAPTURE=1` | lets the layer copy the game's image: pictures for the codex and the screenshot key. Leave it out if you want neither |
| `EHT_OVERLAY_DEBUG=1` | the layer's log on the game's stderr |
| `DISABLE_EHT_OVERLAY=1` | emergency off switch, even with the layer installed |

Do **not** add `PROTON_ENABLE_WAYLAND=1`. The screenshot key needs the game to run through
XWayland, and the overlay has only been tested that way.

If you use the wrapper instead, give its **full path**, because Steam runs launch options through
`/bin/sh` without `~/.local/bin` in `PATH`:

```
/home/<you>/.local/bin/eht-overlay-run %command%
```

## 7. Screenshots with the overlay

Press **F11** in the game. The whole screen, overlay included, is saved as
`screenshots/ED <date> <time>.png` in EHT's working directory. You can change the key, directory
and format in the `screenshots` section of `eht_settings.json`. This needs `EHT_OVERLAY_CAPTURE=1`.
Details are in [overlay/README.md](../overlay/README.md).

## Two accounts at once

Each game + EHT pair needs its **own socket** and its **own working directory**:

```sh
# second account's EHT, started from its own directory with its own journal-dir link
EHT_OVERLAY_SOCKET=$HOME/.local/share/elite_help_tool/overlay-alt.sock /path/to/elite_help_tool

# and the same variable for that account's game
EHT_OVERLAY_SOCKET=$HOME/.local/share/elite_help_tool/overlay-alt.sock ENABLE_EHT_OVERLAY=1 ...
```

`live.sqlite` can be symlinked between the two directories, to share market prices. Keep
`ehtdb.sqlite` separate, because missions, reputation and exploration progress belong to one
commander.

To run the second account without Steam, see [elite_without_steam.md](elite_without_steam.md).

To spread the game over several monitors, borderless from the start under KWin, see
[wine_virtual_desktop.md](wine_virtual_desktop.md).
