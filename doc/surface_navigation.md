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
