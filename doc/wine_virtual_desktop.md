# Elite on several monitors: the Wine virtual desktop, borderless under KWin

> **Linux only.** This is how the author plays Elite Dangerous across three monitors under Proton
> on KDE Plasma (Wayland). It is a working recipe, not the only way.

## Why a virtual desktop

Under Proton, Elite's fullscreen covers one monitor. To stretch the game across several, give Wine
a **virtual desktop**: a single window as large as all the monitors together, which the game sees
as one very wide screen. You then set the game's resolution to the desktop's size.

This is also what the EHT overlay is laid out for. On a triple screen the middle belongs to the
game, and the overlay stays in the side bands (see *In-game overlay* in the README).

The author's setup, for reference: three 3840x2160 monitors at 125% scaling in KDE, and a Wine
desktop of **9000x2160**, a little narrower than all three, centred.

## 1. Turn the virtual desktop on

The setting lives in the game's Wine prefix, in the registry. `winetricks` has a verb for it,
`vd=<width>x<height>`, which accepts any size.

### Steam version: protontricks

```sh
protontricks 359320 vd=9000x2160
```

`359320` is Elite Dangerous' Steam app id. To turn it off again: `protontricks 359320 vd=off`.

The same through the registry directly, if you prefer to see what changes:

```sh
protontricks -c 'wine reg add "HKCU\Software\Wine\Explorer" /v Desktop /d Default /f &&
  wine reg add "HKCU\Software\Wine\Explorer\Desktops" /v Default /d 9000x2160 /f' 359320
```

Afterwards the prefix's `user.reg` holds:

```
[Software\\Wine\\Explorer]
"Desktop"="Default"

[Software\\Wine\\Explorer\\Desktops]
"Default"="9000x2160"
```

### A prefix outside Steam

For a game started without Steam (see [elite_without_steam.md](elite_without_steam.md)), point
`winetricks` at that prefix and at Proton's own `wine`:

```sh
WINEPREFIX=/games/ed-frontier/compatdata/359320/pfx \
WINE=$HOME/.local/share/Steam/compatibilitytools.d/GE-Proton11-6-x86_64/files/bin/wine \
  winetricks vd=9000x2160
```

### In the game

Start the game and set the display to **9000x2160** (the desktop's size), in borderless or
fullscreen mode. The game then fills the whole Wine desktop.

## 2. Open it borderless and in place under KWin

The virtual desktop is an ordinary X window (through XWayland), and by default KWin gives it a
title bar and places it wherever it likes. Three things do **not** work:

- the *No Border* entry in the window menu makes the window fit one monitor,
- `_MOTIF_WM_HINTS` set after the window is mapped (xprop, xdotool) is ignored by KWin,
- Wine asks for a vertically maximised window, which with only *no border* set gives a frame and
  the wrong height (9000x2116 instead of 9000x2160).

What works is a **KWin window rule** with every property forced. It applies when the window first
opens, so the game starts borderless and in place every time.

### Work out the numbers

KWin rules use **logical** pixels, that is physical pixels divided by the display scale:

| value | formula | author's setup |
|---|---|---|
| size | desktop size ÷ scale | 9000x2160 ÷ 1.25 = **7200x1728** |
| x | (all monitors' logical width − size width) ÷ 2 | (3 × 3072 − 7200) ÷ 2 = **1008** |
| y | 0 | **0** |

`kscreen-doctor -o` shows each output's logical geometry and scale.

### The rule

*System Settings → Window Management → Window Rules → Add New…*, or `~/.config/kwinrulesrc`:

| property | match / value | apply |
|---|---|---|
| Window class | regular expression `^(steam_app_359320\|steam_proton)$` | |
| Window title | regular expression `^(Wine [Dd]esktop\|Pulpit Wine)$`: the title follows your system's language, so add yours if it differs | |
| No titlebar and frame | yes | **Force** |
| Position | 1008,0 | **Force** |
| Size | 7200,1728 | **Force** |
| Maximized horizontally | no | **Force** |
| Maximized vertically | no | **Force** |

The window class is `steam_app_359320` for the game started from Steam, and `steam_proton` when
started without Steam. Steam opens two desktops at start: a short-lived one with class
`steam_proton`, then the real one with the launcher, `steam_app_359320`. Matching on both covers
both ways of starting the game.

The same rule in `~/.config/kwinrulesrc`. Add the section's id to `rules=` in `[General]`, then
run `qdbus6 org.kde.KWin /KWin reconfigure`:

```ini
[5e1f9d2a-7a3c-4c1e-9b1a-ed0000009000]
Description=Elite Dangerous - Wine desktop, borderless, centred on three screens
noborder=true
noborderrule=2
position=1008,0
positionrule=2
size=7200,1728
sizerule=2
maximizehoriz=false
maximizehorizrule=2
maximizevert=false
maximizevertrule=2
title=^(Wine [Dd]esktop|Pulpit Wine)$
titlematch=3
wmclass=^(steam_proton|steam_app_359320)$
wmclasscomplete=false
wmclassmatch=3
```

The `rule=2` values are *Force*. A `match` of `3` means a regular expression. The author's own
rule matches the Polish title exactly (`titlematch=1`, `title=Pulpit Wine`).

## Notes

- Keep the game on X11 (XWayland), the Proton default. The virtual desktop, this KWin rule and EHT's
  F11 screenshot key all rely on it. Do not add `PROTON_ENABLE_WAYLAND=1`.
- A window's own position saved by an application is not honoured under Wayland. Placing windows is
  the compositor's job, which is why this is a KWin rule and not a setting in the game or in EHT.
