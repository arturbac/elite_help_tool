# Exploration and exobiology

![The arrival/to-map/life panel, with a genus map of the bodies sampled](images/exploration_life.png)

The bottom right corner of the overlay follows the order of work in a new system:

1. **arrival** — whether the arrival star was discovered before ("discovered before - jump on") or is
   `UNDISCOVERED`, the star's radius in km and in light seconds, and how many bodies from the honk
   have been scanned already. The game writes neither where its exclusion zone ends nor how far the
   ship is from the star - that is only on the HUD - so the radius is the scale to read the HUD's
   distance against, which matters at a white dwarf or a neutron star, a few thousand km across.
   At a white dwarf a line says how far to keep: the game gives no warning before the drop, and the
   heat comes too late to be one. The figure is the HUD distance at which the danger began at one DC
   dwarf, 78 of its radii (`exploration.white_dwarf_danger_radii`), scaled by this dwarf's radius,
   with the number of measurements it rests on (`exploration.white_dwarf_measurements`),
2. **to map** — bodies worth the probes, most valuable first, green when it is a first discovery,
3. **life** — bodies with biological signals and, for each genus, the species it most likely is on
   this world, with its price; green when it is worth landing for (`bio_worth`, 5M).

The species is guessed from your own history. Every species sampled is stored together with its
world (planet class, atmosphere, temperature, gravity, the star above it), and a genus on a new world
is compared with what has already been found under the same atmosphere. On 374 species from the
archive, guessing each one from the rest, the first answer is right in 302 cases and one of the first
two in 349. The more samples, the narrower the guess.

Beside the guess stands what one scan would teach, in its own colour whatever the species pays: the
history of both accounts is asked what it knows of the genus on a world like this one. `new: genus`,
`new: atmosphere`, `new: 12 K warmer` (colder, heavier, lighter: past every find under this
atmosphere), `new: F star` (never under this kind of star - the variant may be a new one), or
`few like it (2)` (fewer than `exploration.little_known`, 3, finds within 5 K and a tenth of the
gravity). A genus seen often on such worlds gets no note, and a cheap one stays grey: nothing is left
to learn from it. A body with such a genus is listed in the same colour (`exploration.new_knowledge`).
The first scan is enough: it names the species and puts the world into the history.

On the surface, in the place of the target panel left of centre, the sample in progress is shown:
species, price, 1/3–3/3 and the distance from the nearest earlier sample against the colony range,
counted live from `Status.json` — a green "sample" when you may take the next one. Below it, what else
grows on this body. It shows in analysis mode or with the sampler in hand, in an SRV always - not in combat mode nor on foot
with a weapon.

Navigating on foot to a codex entry or a place tipped off by a guide is its own window, in
[surface_navigation.md](surface_navigation.md).

**What cartography really pays.** The *Cartography* tab of the Data window lists every sale of
cartographic data in the journals: when, which systems, the bodies sold and those EHT could price,
EHT's estimate at the moment of the sale (the same reckoning as the cartography a death would cost),
and what the game paid - base, bonus and the sum that reached the account. The game writes one sum for
all the systems of a sale and nothing for a body, so only a sale of one system is an exact price;
over those the tab gives how many times the estimate was paid (median, lowest, highest). Sell system
by system to learn more. Exobiology needs no such check: the price list agrees to the credit with
every sample sold in the archive, and the first-logged bonus is always four times the price.
`journal_tailer --cartography --dir <journals> --commander <FID>` prints the same.

## Codex

With every sample the layer takes a picture of the centre of the screen (see
[overlay/README.md](../overlay/README.md)), and
the tool puts it in `codex/pictures/` next to itself, described in `codex/pictures.json`.
`codex/index.html` is written from the database at start and after every sample: family by family,
species by species — price, number of finds, temperature and gravity range, atmospheres, worlds and
stars, pictures, a table of systems and bodies, and what of the family has not been found yet. The
*Codex* button opens it. The pictures live in the directory, not in the database: the database can be
rebuilt from the journals, the pictures cannot.

What the pictures show can be, though. Every picture is named after the journal's timestamp of the
moment it was taken for (the sample, the jump, the scanner), so `pictures.json` and `sky/sky.json` are
a convenience, not the only record: at start, a picture missing from its list - the list lost, broken,
or the picture copied back from a backup - is described again out of the journals, and a list that
cannot be read is kept aside as `.broken`. Only the place of a sample on the ground is lost that way,
since it comes from `Status.json` and no journal has it. Backing up means copying the `codex/`
directory and the journals.

Settings are in the `exploration` section of `eht_settings.json`: `bio_worth`, `bio_bodies`,
`candidates`, `capture`, `capture_size`, `codex_dir`, `jpeg_quality`.

## Sky album

![The sky album page, stars and planets as the ship looked at them](images/sky-album.png)

Out of a jump the ship faces the arrival star, and after the surface scanner closes it still faces the
planet it mapped. At those two moments the layer quietly takes a picture of 80% of the middle screen in
its own 16:9 shape (`exploration.sky_size`), with no frame and no countdown: the star after the jump, in
unpopulated space, tried at 1 s, 2 s and 3 s in turn (`sky_star_delays_ms`: at 0 s the tunnel is still on
the screen; the series stops at whichever attempt first finds the ship's own view, so a map or a scanner
opened during one attempt only costs that attempt, not the ones still to come), and the
planet as it stood before the scanner: in analysis mode, with a planet of this system set as the
destination, the view is taken every 2 s and kept aside; the scanner opening holds the last one, and
the mapping puts it into the album. Without such a view the planet is taken 1.5 s after the scanner
closes (`sky_planet_delay_ms`). The star's scan comes some seconds after the
jump, so the first pictures are described once it is in. Each body is taken once, and only from the ship's own view in supercruise -
a menu or the galaxy map open at the moment an attempt falls due drops that attempt alone. They go to
`codex/sky/<system>/`, named after the journal's moment of the jump or the scanner, are described in
`codex/sky/sky.json`, and `codex/sky.html` shows them with the
star's class, mass, temperature and radius or the planet's class, atmosphere, gravity and temperature,
and whether it was a first discovery. The *Sky* button opens it; `sky_pictures` turns it off.
