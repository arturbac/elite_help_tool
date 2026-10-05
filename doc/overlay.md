# In-game overlay

![Side bands with the BGS, mission and system-map panels drawn over a supercruise view](images/overlay_side_bands.png)

A Vulkan layer loaded into the game's process draws EHT's information straight into the frame. It
works in fullscreen, and under Wayland, where no other window may sit on top of the game. The
layer only draws what the tool sends it. The database and the logic stay in EHT, so the game
process carries no state of ours.

- **Side bands.** On a wide or triple screen the middle belongs to the game, so the blocks stay in
  the side bands and long text wraps instead of growing towards the centre.
- **Head-up readouts** flank the centre of the middle screen: the target in a fight, crew, and the
  sample in progress on foot. They give way to any interface that takes over that screen, such as
  the galaxy map.
- **Dogfights.** The target readout, on the left of centre, fills in as the scan goes: ship, pilot
  and combat rank, then hull and shield in per cent, then faction, legal status, bounty and the
  targeted subsystem with its health. On the right stand your hired pilot with their rank and the
  fighter's state: out and who flies it, in the bay, or destroyed and being rebuilt. A kill stays on
  screen for a few seconds with the victim's faction, the credits it paid and, marked "paid by", the
  factions that paid them - other factions than the victim's, the ones that had the bounty on it.
  When the danger is now (heat, an interdiction, the game's own danger warning), what a death would cost moves to the
  middle of the screen. Both readouts stand where you look during a fight, clear of the crosshair.
- **What stands where:** the system and its exploration work on the right, the station's market
  and mission cargo top right, the route, cargo, missions and what a death would cost on the left.
- **Last port:** where an escape pod would take you, beside the ships in transit, with its distance
  in light years when its system's position is known. Only ports count: not on-foot settlements,
  fleet carriers, construction sites or the colonisation ship.
- **System map:** the bodies and ports of the current system, drawn in the game's colours, with
  arrows at the places open missions send you to. Stars and planets are small lit balls: each planet
  and moon is lit from its star's side, water and ice shine a little, bare rock does not, and a star
  lights itself with only a darker limb. A body the surface scanner has been on wears its own face,
  cut out of the scanner's views (see [Faces of the planets](exploration.md#faces-of-the-planets)), and
  a face with life on it gets a green ring. A body with an atmosphere wears it as a veil in the
  colour of its main gas - nitrogen pale blue, oxygen blue, sulphur dioxide yellow, methane
  turquoise, argon violet, neon pink, vapours orange - faint over the face, thickest at the limb
  and glowing a little past it, reaching further for thick air than for thin. The names of the
  planets and moons say what is left to map: green for a body already mapped, amber for one worth
  mapping (`overlay.minimum_body_value`) and not mapped yet, grey for the rest
  (`overlay.system_map.mapped` and `to_map` in the settings). The balls are shaded on the graphics card as they are
  drawn, the faces read on a thread of their own and copied into one texture a few at a time, which
  together costs the game a few microseconds a frame. The orbital ports you have docked at are small
  models lit from the same star: a Coriolis is a cuboctahedron and a Dodec a dodecahedron, each with
  its dark docking slot, an Ocellus a large ball on a spindle, an Orbis (Artemis and Apollo alike) a
  small core in a large ring on three spokes, an outpost a few boxes on a spine over its pad, an
  asteroid base a lump of rock with a box on it. The tool turns and shades them and sends only the
  flat polygons; the layer fills them in the order given.
- **Readable over anything.** The ground under each block darkens by how bright the game is
  beneath it, so the text stays legible over an ice planet as over black space.
- **F11** saves a screenshot of the whole screen, overlay included.
- **A small sample of the screen** for the tool to watch: when asked, the layer shrinks the middle of
  the game's image on the graphics card - halving it blit after blit, each step the mean of 2 x 2
  pixels - to a few hundred pixels across and writes it into a file shared with the tool, read out
  only once the frame's fence has passed. The tool looks for what it wants in the sample and then asks
  for a real picture of just the rectangle that holds it. It costs the game a few microseconds a frame.
  `overlay.sample.always` keeps the sample made at all times (`every_ms`, `size`, `width`), for looking at
  what the game shows from outside it; `overlay/tools/sample_to_png.py` turns the newest one into a PNG.
- **Temperatures.** Under the frame rate at the top of the right band stand the graphics card's and
  the processor's temperatures in degrees Celsius, orange from 10 degrees below the driver's
  critical level and red at it (3 degrees of hysteresis). They are read from `/sys/class/hwmon`,
  which every user may read - no root, no group, nothing written - in a thread of their own every
  2 s, the sensors looked for after the start, so a missing one never holds anything back. The card
  is the one with the most video memory, by its hottest spot (junction) or else its edge, and a
  card that sleeps is not woken to be read; the processor is its hottest die (k10temp's Tccd,
  Tdie or Tctl, zenpower, coretemp's package, or the thermal zone). A card from NVIDIA with its own
  driver is asked through its library when it is installed. Where the driver gives no critical
  level, `sensors.gpu_critical` (100) and `sensors.cpu_critical` (90) stand in; `sensors.enabled`
  turns the line off, `sensors.interval_ms` sets the pace.
  With `evidence.dir` set (see [the settlement glare](settlement_glare.md)) a line goes every 10 s
  (`sensors.log_interval_s`) to the day's `<dir>/sensors-YYYY-MM-DD.jsonl` - `ts_utc`, `gpu_c`,
  `gpu_sensor`, `pci`, `cpu_c`, `cpu_sensor`.

  ![The temperature readout under the frame rate](images/overlay_temperatures.png)
- **Server link.** Under the temperatures the overlay tells trouble with Frontier's servers while it
  happens - the game's own disconnect dialog comes only after it has given up, and by then its server
  has as a rule been silent for 40 seconds or more. Red is the game server itself, the session at
  stake: `game server silent N s` (not one UDP datagram received for `overlay.server_silence_ms`,
  3000 by default, while a session runs - several come every second while all is well),
  `EDServer#N dropped N s ago - a disconnect may follow` (one of the game's servers given up after too
  many retries) and, once it happens, `disconnected: <reason>`. Amber is trouble the game goes on
  through, only worse: `EDServer#N: packets lost N s ago` (a packet arrived after a gap), `player link
  dropped N s ago (relay, <reason>)` (the link to another player - a wingmate, an opponent in a
  conflict zone - given up: their trouble or the way to them, not Frontier's), and the web
  API beside the game server - the inventory, the journal upload, colonisation and the like:
  `Frontier API: N requests
  failed in the last minute (no answer | HTTP 502 | ...)` and `Frontier API: <request> took N s` (the
  game writes down only requests of 10 s or more). See [network incidents](network_incidents.md#while-it-happens)
  for what is read and why.
- **Players in the instance.** Under the server link a line counts the other players in the same
  instance: `2 players in the instance, 1 of the wing, 1 through a relay - newest 8 s ago`, amber for
  a minute after one not of the wing comes, plain after; `player left N s ago, none in the instance
  now` for half a minute after the last goes. Out in the black, where nobody was expected, this is the
  first word of company. It is read from the game's netLog: the game writes every session it shares
  with another machine - `IJoinSession:Island` when it enters an instance, `JoinSession:Island` when
  another player is in it, `LeftSession(n)` or `Disconnected` when one goes, `WingSession` for the
  wing and `((Relay))` for a link through a relay. In the history checked, 96 of a hundred such joins
  named the instance the game was in. The netLog never gives the commander's name nor the ship - only
  the machine - so the line says how many, not who. `overlay.players_in_instance` (default `true`)
  turns it off.
- **Graphics for the place.** The game's own upscaler is cheap and its edges stair-step: good enough
  over the ground of a settlement, where frames are dear, and hard on the eyes along the long straight
  edges of a station or a carrier. Two sets of the game's graphics are named in the settings, one for
  near a planet and one for space, and while the game's graphics file holds the other one, a line in
  amber at the top of the right band, straight under the layer's own lines, asks for the place's set:
  `graphics near the planet: set FSR Balanced + SMAA - now FSR Ultra Quality + SMAA`. It goes as soon as the set is applied in the game's menu. Near a planet is what
  Status.json says by giving latitude and longitude - from orbital cruise down to the ground - or on
  foot on a planet; everywhere else is space. A place counts after it has lasted `settle_ms`, so a
  flicker of the flags asks for nothing. The file is the newest `Custom.*.fxcfg` in the game's
  `Options/Graphics`, found in the same Wine user's home as the journals; the game writes it when the
  options are applied, and EHT only reads it - the three values compared are `UpscalingQuality`,
  `SSAAMultiplier` and `AAMode`. Off by default:

  ```json
  "graphics": {
     "remind": false,
     "dir": "",
     "settle_ms": 3000,
     "planet": { "name": "FSR Balanced + SMAA", "upscaling": 2, "supersampling": 0.59, "anti_aliasing": 4 },
     "space": { "name": "FSR Ultra Quality + SMAA", "upscaling": 2, "supersampling": 0.77, "anti_aliasing": 4 }
  }
  ```

  The numbers are those the game writes into the file for the set; `name` is only what the line says.
  `dir` points at another `Options/Graphics` when the journals are not in a Wine user's `Saved Games`.
- **Jump panel.** While the drive charges for a jump to another system, the overlay lists the
  destination's factions under the game's panel with their influence and the last tick's trend
  (`overlay.jump_emblem.factions`, `factions_y`, `factions_width`). It knows only systems already in
  its database: on a first visit, and in unpopulated space while exploring, it shows no list.

  The game's panel shows the destination's superpower emblem, and it is wrong for the Federation, the
  Empire and the Alliance (only independents get the right one). The overlay does not paint over it:
  edworld, a separate d3d11 proxy in the game process, draws the right emblem onto the panel itself, so
  it moves with the panel when the cockpit camera swings. The tool tells edworld what its database knows
  of the destination, in `target` in the tmpfs directory `edworld.dir` (default `/dev/shm/eht`, empty:
  nothing written), so edworld paints the emblem from the tool's record instead of EDSM's, which can be
  older. The record carries the destination's factions as well, the same ones the list under the panel shows,
  for edworld to list them on the panel itself (the tool's whole list when it has influence readings of the
  system, else edworld takes the whole list from EDSM and says so).

  ![The corrected jump panel with the destination's factions listed below it](images/overlay_jump_panel.png)

The layout (text size, band widths, where the readouts stand, opacity) comes from
`eht_settings.json` and changes live. The frame rate also stands alone in the top left corner of
the middle screen; `overlay.layout.centre_fps` (default `true`) turns it off. The Steam launch option is one variable:
[building_and_running.md](building_and_running.md). How the layer works and why nothing in
it can take the game down: [overlay/README.md](../overlay/README.md).
