# Overlay in the game window

A Vulkan layer that draws information from `elite_help_tool` straight into Elite Dangerous' frames.

## Why a layer and not a window

A Wayland session does not let another program put a window over the game, and in fullscreen that
works nowhere. A Vulkan layer draws into the swapchain image just before it is presented, so it
ends up in the game's picture whatever the window mode. Elite goes DX11 → Proton → DXVK → native
Vulkan, so the layer catches it.

## What it is made of

| file | role |
|---|---|
| `libeht_overlay.so` | the Vulkan layer; the loader puts it **into the game's process**. It only tracks the game's devices, queues and swapchains and hands them to the plugin |
| `libeht_overlay_plugin.so` | the overlay itself - drawing, pictures, the sample, the faces, the screenshot key - loaded by the layer from beside itself |
| `eht-overlay-run` | a wrapper for `%command%` in Steam: sets the variables and starts the game |
| `eht-overlay-feed` | a test source, sends frames without the main application |
| `eht-overlay-headless-check` | renders without a screen into a PPM file, for checking the layer |

The server lives in `elite_help_tool`, and it decides what is to appear. The layer receives finished
lines of text and knows nothing of the database, so there is neither SQLite nor any state of ours
in the game's process.

## Installation

```
ninja overlay-install
```

Copies the layer and the plugin to `~/.local/lib`, the wrapper to `~/.local/bin` and the manifest to
`~/.local/share/vulkan/implicit_layer.d`. It has to be the home directory, because the Steam Linux
Runtime container does not see the build directory.

In Elite Dangerous' Steam launch options it is simplest without the wrapper, because the manifest
lies in the standard path the loader searches anyway:

```
ENABLE_EHT_OVERLAY=1 %command%
```

With the wrapper, **the full path is required**. Steam runs launch options through `/bin/sh`,
which has no `~/.local/bin` in its `PATH`. The bare name ends in `command not found`, and the game
does not start at all:

```
/home/<you>/.local/bin/eht-overlay-run %command%
```

Elite starts through its own launcher, and child processes inherit environment variables, so the
game gets them just as the launcher does. The launcher has no swapchain, so the layer does nothing
in it.

## Environment variables

| variable | effect |
|---|---|
| `ENABLE_EHT_OVERLAY=1` | turns the layer on |
| `DISABLE_EHT_OVERLAY=1` | turns it off even with the manifest installed: the emergency switch |
| `EHT_OVERLAY_DEBUG=1` | the layer's log on the game process' stderr. A part of the overlay that gives up (drawing, sampling, faces, captures, the screenshot key) says so there once even without it |
| `EHT_OVERLAY_SOCKET` | the socket's path, by default `~/.local/share/elite_help_tool/overlay.sock` |
| `EHT_OVERLAY_CAPTURE=1` | lets the layer copy the game's image. Without it there are no codex pictures and no screenshots at the key |

These are the only ones, needed before the layer connects to the tool. The whole layout (text
scale, the widths of the side bands and the middle screen, where the head-up readouts stand,
margins, opacity, the layer's own diagnostics, the frame rate in the top left corner of the middle
screen - `centre_fps`, on by default) comes from `elite_help_tool` with every frame, from
the `overlay.layout` section of `eht_settings.json` in the directory the tool runs from. A saved
change shows in the game at once, and a change of text scale rebuilds the layer's fonts without
restarting the game. The old `EHT_OVERLAY_SCALE`, `_SIDE_WIDTH`, `_CENTRE_WIDTH`, `_HUD_GAP`,
`_HUD_BOTTOM`, `_SMALL_TEXT` and `_STATS` are no longer read.

## Two game accounts at once

Nothing stops two tool + game pairs from running side by side, for example a Steam account and
another from a separate launcher. There is one condition: **each pair needs a socket of its own**.
At start the server deletes any socket file it finds, so that a file left by a killed process does
not block it. Two instances on the default path would take the connection from each other.

```
# main account - unchanged, the defaults
elite_help_tool

# second account - the tool and the game get the same path of their own
EHT_OVERLAY_SOCKET=~/.local/share/elite_help_tool/overlay-alt.sock elite_help_tool
EHT_OVERLAY_SOCKET=~/.local/share/elite_help_tool/overlay-alt.sock ENABLE_EHT_OVERLAY=1 <launcher>
```

The socket's path cannot exceed 107 characters, which is all `sockaddr_un` holds. A longer one ends
with nothing listening, which the tool's log shows together with the path's length.

Each account also needs its own working directory for the tool, because `journal-dir` and
`ehtdb.sqlite` are relative to the current directory, and missions and loot belong to one
commander. `live.sqlite` is the opposite: it holds only facts about the galaxy (markets, prices,
bartenders' shelves), so it is worth symlinking, letting both accounts share what they know of
prices.

The side band matters on panoramic screens: at 8000 px the middle belongs to the game, and the
overlay fits into ~1600 px on each side. Longer text wraps within the band instead of running into
the middle.

## Checking visibility inside the Steam container

The game runs in a container that does not see the build directory. That the loader inside finds
the layer can be checked without starting the game:

```
ENABLE_EHT_OVERLAY=1 ~/.local/share/Steam/steamapps/common/SteamLinuxRuntime_4/run --   vulkaninfo --summary | grep EHT
```

A `VK_LAYER_EHT_overlay` line among the layers means the path under `$HOME` works, and the library
is compatible with the C standard library in the container.

## A new overlay without restarting the game

The layer looks at the plugin's file once a second. When another file stands under its name - the
install renames the new one over the old - the layer waits for the presenting queue, detaches every
device and swapchain from the running plugin, has it stop and join its threads and free what it holds,
unloads it, and attaches everything to the new one, which builds its resources at the next frame. A
file that cannot be loaded, or is of another interface (`plugin_api.h`, `EHT_OVERLAY_PLUGIN_ABI`),
leaves the running plugin in place. The plugin is copied into memory and loaded from there: the
dynamic loader knows a library by its name, and would only hand the old one back.

So a change in the overlay needs only `overlay-install`. A change in the layer itself (`vk_layer.cc`,
`plugin_api.h`) still needs the game restarted. When the game destroys its last Vulkan instance, or
the process ends without doing so, the plugin is shut down and unloaded the same way.

`EHT_OVERLAY_PLUGIN` names another file for the plugin, for a test.

## Checking without the game

```
eht-overlay-feed &
ENABLE_EHT_OVERLAY=1 eht-overlay-headless-check /tmp/check.ppm 8000 1440
```

`VK_EXT_headless_surface` gives a swapchain nobody looks at. The layer draws exactly as in a real
window, and the picture comes back into the file. Neither a desktop nor the game is needed.

`EHT_CHECK_FRAME_MS=50` paces the frames, so the plugin's file can be replaced during the run and its
reloading watched in the log (`EHT_OVERLAY_DEBUG=1`). Configured with `-DEHT_OVERLAY_SANITIZE=ON`,
the layer, the plugin and the check are built with AddressSanitizer and LeakSanitizer; a run through
loads, reloads and the end reports no leak of ours - the one of 256 bytes left is the driver's, and is
there without the layer too.

## Measured

On an RX 7900 XTX, 3000 frames, RADV:

| measurement | result |
|---|---|
| cost per frame | 0.018 ms (0.040 → 0.058), about 0.1% of the budget at 60 fps |
| creating the layer's resources | 0.2 ms |
| releasing the resources | 0.2 ms |
| 2000 swapchain re-creations | no crash, memory growth 0.3 MB |

The last row stands for alt-tabbing and changes of resolution in the game.

```
ENABLE_EHT_OVERLAY=1 eht-overlay-headless-check /tmp/x.ppm 1920 1080 10 200
```

## A picture of the middle of the screen

At every sample of an organism the tool asks the layer for a picture, because the commander is
looking straight at the plant then. The request rides in the frame (`frame_t.capture`: a number, a
path, and the side of the square as a share of the screen's height) and repeats in every frame
after, so it is **the change of the number** that asks, not its presence. The first number a
freshly started layer sees counts as already served: it is the tool's last frame handed to a new
client, and the moment it was meant for is long gone.

The layer copies a square from the middle of the game's image **before** drawing the overlay over
it - or a wider rectangle of the same height, when `capture_t.aspect` asks for one (16/9 takes the
middle screen's own shape; an older layer skips the field and takes the square). It takes the pixels out only once that frame's fence has signalled, and writes the PPM in a
thread of its own, under its final name only when the file is complete. The files go beside the
socket (`~/.local/share/elite_help_tool/captures/`), the one place visible on both sides of the
container. The tool turns them into JPGs in its `codex/` directory.

The swapchain additionally gets `TRANSFER_SRC`. A driver that does not accept it leaves the overlay
without pictures, not without the overlay. A picture that fails once is not tried again. An older
layer skips the new field and simply takes no pictures.

## Screenshot at a key

In the game, **F11** takes a screenshot of the whole screen **with the overlay**, as the player
sees it. The key and the format are set in the `screenshots` section of `eht_settings.json`:

| key | default | meaning |
|---|---|---|
| `key` | `F11` | the X key name (as in `xev`): `F1`–`F35`, `Print`, `Pause`, `Scroll_Lock`, `KP_Multiply`…, a letter, a digit or a keysym such as `0xffc8`; empty turns it off |
| `dir` | `screenshots` | the directory for screenshots, relative to the tool's directory |
| `format` | `png` | `png` keeps the overlay's text sharp, `jpg` is several times smaller |
| `jpeg_quality` | `92` | the quality for `jpg` |

The layer hears the key itself, not the tool. A thread of its own in the game's process asks the X
server for the keyboard's state every 30 ms, through `libxcb` loaded with `dlopen` from the game's
process. Under XWayland the X server knows the keys only while one of its windows has the focus.
With two games at once only the one whose window is active counts (`_NET_ACTIVE_WINDOW` →
`_NET_WM_PID` → the same process or the same `WINEPREFIX`). The game gets the key just as without
the overlay; the layer only watches.

The copy of the whole image is made **after** the overlay is drawn. It goes into the spool as
`screenshot_<ms>.ppm`, and the tool, in a thread of its own, saves it as `ED <date time>.png` and
deletes the PPM. Screenshots taken while the tool was not running are saved at its next start. It
needs `EHT_OVERLAY_CAPTURE=1`, like the codex pictures.

It works only with the game in an X11 window (XWayland), which is how Proton starts it by default.
With `PROTON_ENABLE_WAYLAND=1` the layer will not hear the key.

## Patches over the game's interface and blocks in the middle

`frame_t.covers` are patches painted fully opaque over the game's own interface, with an emblem in
the middle if one is named. The tool sends none: the superpower emblem the game gets wrong in the panel
of a jump being charged is put right by edworld, on the panel itself, and edworld lists the destination's
factions under it as well. A block with `middle` set stands in the middle screen instead of its corner,
centred across it; the tool sends none at present. `line_t.emblem_column` keeps the emblems' column for
lines without markers, so the names still start in one place. An older layer skips the field and draws
the lines without emblems.

Both are placed on the middle screen, from its centre, in shares of its height (`x`, `y`, `width`,
`height`, `emblem_height`, `middle_y`, `middle_width`). The game draws its interface on the middle
screen at 16:9 whatever the resolution and however wide the whole surface, so the same numbers land
on the same spot of its interface at 1080p, at 4k and on a triple screen. A patch goes when the tool falls
silent for 10 s, so it never outlives the tool. An older layer skips both fields: no patches, and
the block joins its corner.

The cockpit camera is not fixed to the ship: it lags and swings when the ship turns, and the panel moves
on the screen with it while a patch at a fixed spot stays. A patch can follow a panel instead
(`cover_t.follow_width`, `follow_height`, `follow_rest_x`, `follow_rest_y`), from what edworld reports:
edworld is a separate, read-only d3d11 proxy in the game process that publishes, for every cockpit panel
the game draws, the size of its interface surface and where its origin lands on the screen, in
`/dev/shm/eht/panels` (`EHT_WORLD_SHARE` names another file; layout in `include/edworld_share.h`). The
layer maps that file, picks the panel by its surface's size (the one nearest its rest place when several
share it) and moves the patch by as much as the panel's origin moved from `follow_rest_x`, `follow_rest_y`
(normalised device coordinates of the whole surface, x right, y up). Without the file, with a record older
than 250 ms, or with `follow_width` 0, the patch stays where `x`, `y` put it. An older layer skips the
fields and its patch stays put.

The other way round, the tool tells edworld what its database knows of a jump's destination: once per new
FSDTarget it writes `target` into the same tmpfs directory (`edworld.dir` in `eht_settings.json`, default
`/dev/shm/eht`; layout `edworld::target_t`, written under a seqlock by `include/world_target.h`): the system's
address, whether the database has it, its allegiance and name, and - when the database has influence readings
of the system - its factions by influence (at most `edworld::max_target_factions`): name, influence, allegiance,
the trend at the last tick, whether it controls the system, and the states with the pushes on the bars as the
overlay's own faction lines show them. edworld draws the emblem from that when the address is its destination
and the system is known, and lists the factions under the panel from it when they are there; it asks EDSM only
for what the tool does not have (the whole list, never a mix of the two), and for everything when the tool does
not know the system or says nothing for two seconds. The factions follow the first layout's fields, and
`target_t::size` says whether they are there: an edworld of the first layout reads its part of the record and
ignores the rest. A directory that cannot be written leaves one warning in the log and edworld on EDSM.

## An arrow on a line

`line_t.pointer` puts an arrow before the line's text, in the line's colour, turned that many degrees
clockwise from straight up. The tool uses it for the way to a point on a body's surface: up is ahead,
90 is a right turn. An older layer skips the field and draws the line without the arrow.

## What cannot take the game down

None of this may crash the game:

- a failure to create the resources or to initialise ImGui means giving up on that swapchain once;
  presentation goes on untouched,
- a swapchain that does not accept the added usage flag is created the old way, without the overlay,
- waiting for a fence is limited to a second, after which the layer turns itself off,
- a failed ImGui assertion while a frame is drawn turns the overlay off on that swapchain, as any
  exception in drawing does; outside drawing it is said once in the log and ImGui goes on,
- no exception leaves the layer's entry points into the loader and the game: an instance or a device
  the layer cannot keep track of is destroyed and reported to the game as `VK_ERROR_OUT_OF_HOST_MEMORY`,
  a swapchain whose bookkeeping fails, or that gives no images, goes on without the overlay,
- every thread of the layer in the game (the reader of the tool's frames, the planet faces, the
  pictures, the screenshot key) catches what is thrown in it and stops alone, said once in the log;
  a reader that stopped, or could not start for want of its wakeup descriptor, shows
  `elite_help_tool: the reader stopped` in the layer's own lines,
- the fonts are rebuilt only after the card finished every frame of ours that used them; otherwise
  the old ones stay and the next frame tries again,
- a plugin that cannot be found, copied, loaded or that refuses the interface is reported in the log
  even without `EHT_OVERLAY_DEBUG`,
- the client thread and the registry are deliberately never released, so that the game process
  winding down cannot get stuck on them.

On the tool's side a frame that is not sent - it cannot be written as JSON, or it is larger than the
256 KiB the layer accepts - leaves the game showing the previous one; the tool's log says so once
when it starts and once when frames get through again.

The start order does not matter: the client keeps trying to connect, the server accepts whoever
comes, and neither minds the other side being absent or restarting. A newly connected game gets
the latest picture at once, without waiting for the next change.
