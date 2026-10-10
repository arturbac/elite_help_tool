# Surface navigation

- **The Surface window** takes a place on a body as two numbers, the way a tip-off or a guide gives
  it: `-12.3456, 45.6789`, `12,34 -56,78`, `Lat: 12.3 Lon: 45.6`, `12.3 S 45.6 E`. It says at once how
  it read them. The body is the one you are near unless you name another, so a target can be set
  before arriving. The targets of the session stay in a list; a double click goes to one again.
- **The codex of the body you are near** fills a table in the same window: every species, geological
  feature or other entry the codex logged with a place on this body, by any account whose journals are
  in the directory, the nearest first, with the distance and the way to each. A double click makes one
  the target. The journals name the body only by its number there, so a body counts once you have
  approached it or touched down on it; the whole archive is read once in the background, then only
  what is added.
- **On the overlay**, beside the centre on the right, an arrow turned the way to go (straight up is
  straight ahead), the distance, the course, how far to turn, and at a walking or flying pace how long
  it takes. Within 25 m it says the target is reached. Near another body it only names the body the
  target is on. Everything comes from `Status.json`: position, heading and the body's radius.
- **The way down (proof of concept)**: over a body, in a ship, not landed, the right side band shows
  the place being flown to - the Surface window's target on this body, or else the destination set in
  the game when its latitude and longitude are known - from an earlier approach, or from this one: a
  destination whose place is not known yet is looked up again every 10 seconds, and `ApproachSettlement`
  writes it on the way down. The game's destination shows only over its own body when the system's
  scans name that body; over an unscanned body it is trusted. It says the distance
  with the arrow of the turn, the height, how far below the horizon the place lies, and, above 3 km,
  the angle of the path flown - lower down the positions `Status.json` gives repeat and jump, and that
  angle is noise. Then the vertical speed, the ground speed and the time to the place. A chart under it
  plots the height against the way still to go over the last five minutes, the place at its right edge,
  with the straight line from here to it. The place is taken at the body's mean radius, its own height
  unknown, and a settlement's coordinates are its centre, not its pads. The height is the one
  `Status.json` gives, over the body's mean radius (`mean`, Flags bit 29: in orbital cruise and high
  up) or over the ground under the ship (`ground`); the track starts over when it changes from one to
  the other, and the one fix over the mean radius the game writes on a pad or at touchdown is left
  out.
- **The pad given (proof of concept)**: from `DockingGranted` until the ship is down or leaves, the
  request is cancelled or denied, or the ship enters supercruise, jumps, dies or the game is loaded
  again (at most 10 minutes), beside the centre on the left: the pad's number, the station and its type.
