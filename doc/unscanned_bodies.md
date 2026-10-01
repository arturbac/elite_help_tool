# The bodies you never scanned are paid for too

A curiosity from the game's own sales, worked out from the journals. It started with one sale that
made no sense and ended with a rule that matches the sales to the credit.

## The sale that made no sense

On a long plotted route (34 jumps), the ship dropped into **Outotch YP-P d5-13**, the arrival star
scanned itself, the ship scooped fuel and jumped on 28 seconds later. No honk, no FSS, no map. The
journal knows exactly one thing about the system: the `AutoScan` of its K0 Va star of 0.92 solar
masses, already discovered by someone else.

Such a star is worth 1217 credits. Universal Cartographics paid this for it:

```json
{ "timestamp":"2025-12-23T11:57:18Z", "event":"SellExplorationData",
  "Systems":[ "Outotch YP-P d5-13" ], "Discovered":[ ],
  "BaseValue":331675, "Bonus":0, "TotalEarnings":305141 }
```

272 times the value of the only body the journal has. (`TotalEarnings` is 92% of `BaseValue`
because an NPC crew member takes a share; that part is ordinary.)

It was not alone. Selling the route system by system at one station, a dozen systems where the
journal holds only the arrival star sold for far more than the star:

| System | Arrival star | Paid (`BaseValue`) | The star alone |
|---|---|---:|---:|
| Outotch YP-P d5-13 | K0 Va, 0.92 M☉ | 331 675 | 1 217 |
| Hegoo CS-V c2-2 | K6 Va, 0.59 M☉, first discovery | 292 884 (+180 236 bonus) | about 3 150 |
| Col 285 Sector DI-Z c14-21 | K8 Va, 0.52 M☉ | 276 003 | 1 209 |
| Preae Theia SG-E c12-0 | K2 Va, 0.68 M☉, first discovery | 263 122 (+161 921 bonus) | about 3 150 |
| Hegoae FL-Y d5 | F4 Vb, 1.37 M☉ | 172 349 | 1 225 |
| Outotch RD-T d3-3 | F1 VI, 1.42 M☉ | 159 650 | 1 226 |
| Gludgae ZR-S c17-9 | G4 Vab, 0.89 M☉ | 152 418 | 1 216 |
| Outotz KW-E d11-8 | F3 Vb, 1.40 M☉ | 127 682 | 1 225 |
| Outotch RD-T d3-6 | A8 VI, 1.53 M☉ | 121 076 | 1 228 |
| Gludgeia SY-Z d13-5 | A9 Vb, 1.38 M☉ | 116 894 | 1 225 |
| Hegoo NU-B b41-0 | M2 Va, 0.42 M☉ | 101 330 | 1 208 |

while other star-only systems of the same trip sold for exactly the star: Butterfly Sector DQ-Y c0
for 1215 in the same few minutes, Hegue CW-B c27-0 for 1216 and Ploea Eurl TI-S d4-1 for 1217 a week
later.

## What it was not

- **The star.** The expensive ones are K8, K0, M2, A8, A9, F1, F3, G4; stars of the same class and
  mass elsewhere sold for the plain star value. Type, mass, age, temperature, luminosity: nothing
  separates the expensive stars from the cheap ones.
- **Distance.** The systems lie from 140 to 9800 light years from Sol.
- **An earlier visit.** The commander was new: the first journal shows `Systems_Visited` 0, and
  every one of these systems appears in the journals exactly once, on that route.
- **A misattributed sale.** Every system was sold on its own, and the crew wage written just before
  each sale is the same 6.8% of that sale's `BaseValue` throughout, so the amounts belong to the
  systems named.
- **The value formula.** For systems scanned in full the formula in
  [Exploration](exploration.md) matches 69 of 83 sales within a tenth of a percent,
  so the gap is real, not an error of the estimate.

## What it was: the rest of the system

The journal knows one body, but the game knows the whole system. [EDSM](https://www.edsm.net/)
holds the bodies other explorers scanned years ago. Col 285 Sector DI-Z c14-21, for example, has
three terraformable water worlds and a terraformable high metal content world besides its star.

Value every body EDSM knows with the same formula, as if it had been scanned with the FSS, and
compare what the game paid on top of the star with the value of the rest of the system:

| System | Bodies in EDSM | Paid beyond the star | The rest, fully scanned | Share |
|---|---:|---:|---:|---:|
| Col 285 Sector DI-Z c14-21 | 10 | 274 794 | 1 096 179 | 0.251 |
| Hegoae FL-Y d5 | 8 | 171 124 | 683 501 | 0.250 |
| Outotch RD-T d3-3 | 23 | 158 424 | 625 697 | 0.253 |
| Gludgae ZR-S c17-9 | 10 | 151 202 | 599 808 | 0.252 |
| Outotch RD-T d3-6 | 12 | 119 848 | 477 392 | 0.251 |
| Hegoo NU-B b41-0 | 26 | 100 122 | 389 487 | 0.257 |
| Preae Theia UZ-G d10-0 | 13 | 38 149 | 152 593 | 0.250 |
| Wregoe PU-A c0 | 6 | 15 919 | 62 675 | 0.254 |
| Hyades Sector VD-S b4-0 | 10 | 4 500 | 9 000 | 0.500 |

A quarter for the valuable bodies, a half for the small icy ones. Both come from one rule. A
scanned planet is paid

```
v = max(500, k + k * 0.56591828 * mass^0.2)
paid = v + max(v / 3, 500)
```

and a planet of the system that was **not scanned at all** is paid only the second part:

```
paid = max(v / 3, 500)
```

For a large body that is a quarter of its scanned value; a small body worth 1000 when scanned is paid
500, a half. The cheap star-only systems are simply systems with nothing else in them: EDSM knows a
single star there.

## Confirmed where the journal can check it

Systems the commander scanned partly are the real test, because there the journal says how many
bodies the system has (`FSSDiscoveryScan.BodyCount`), which ones were scanned, and what they are
worth. Value the scanned bodies from the journal, add `max(v / 3, 500)` for every planet EDSM knows
that was not scanned, and compare with the sale:

| System | Honk: bodies | Scanned | Unscanned in EDSM | Estimate | Paid |
|---|---:|---:|---:|---:|---:|
| Col 285 Sector OF-R b20-1 | 8 | 3 | 5 | 6 315 | 6 314 |
| HIP 28966 | 11 | 8 | 3 | 432 406 | 432 405 |
| HIP 116750 | 41 | 27 | 14 | 167 829 | 167 830 |

To the credit, with the unscanned bodies counted; 1.66, 1.12 and 1.36 times the estimate without
them. Hegoo BN-K b22-0 is the same story where EDSM does not help: the honk counted 19 bodies, the
commander scanned 2 worth 2812, EDSM knows only those 2, and the sale paid 199 927 - for 17 bodies
nobody has put on a public map.

## What this means when you play

- Every system you jump into and sell pays something for **all** its planets, scanned or not: a
  quarter of their FSS value, at least 500 each. The arrival star's automatic scan is enough to
  make the system sellable; neither a honk nor the FSS is needed.
- A long route through systems explored long ago is not worthless: the star-only systems of the
  one route above sold for about 1.45 million credits, in systems where the ship did nothing but
  scoop.
- The FSS is still worth it: it pays the other three quarters, and mapping is where the real money
  is.
- A first discovery seems to follow the same rule with its multiplier on top (Hegoo CS-V c2-2 and
  Preae Theia SG-E c12-0 above), but nobody else has scanned those systems, so there is nothing to
  check it against.

## Still open

- **Unscanned stars.** The rule above is checked for planets. What an unscanned companion star
  pays is not settled: there were too few of them in these sales to tell.
- **First discoveries.** The bonus on the two first discovered star-only systems (61% of
  `BaseValue`) is much larger than the bonus of an ordinary lone star (about 93% of a value 100
  times smaller); without the bodies it cannot be taken apart.
- **Is it intended?** Searching the web (Frontier forums, the issue tracker, Steam discussions,
  update notes) turned up nothing about it: the common understanding is that unscanned bodies pay
  nothing. It may well be the same thing as the "third more, never less than 500" that Odyssey
  added to every planet sold, paid per body of the system rather than per body scanned.

## In EHT

EHT's estimate counts only bodies the journal has, so for a system not scanned in full the sale is
higher than the estimate shown. The journal names the unscanned bodies nowhere, and EHT downloads
nothing from public databases ([Privacy](privacy.md)), so their classes and masses are not known;
only their number is, after a honk.

## How it was checked

From the journals: every `SellExplorationData` of a single system, the `Scan` events of that
system, `FSSDiscoveryScan` for the body count and `SAAScanComplete` for mapping (systems with a
mapped body were left out of the check). Bodies the journal does not have came from EDSM's public
`api-system-v1/bodies` endpoint, valued with the formula above.
