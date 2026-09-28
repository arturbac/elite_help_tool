# How EHT works out when the BGS tick happened

Elite Dangerous has no fixed tick hour. Frontier moves it every few days, and over a weekend the
tick can fail to come for about 48 hours. EHT does not predict the tick. It reconstructs, from your
own journals, when the tick has already happened.

The key point: EHT does **not** work out a narrow tick window for each system on its own. If you
visit a system rarely, its data alone cannot give one. The narrow window comes from pooling
observations from **every** system you fly through. For a given system, EHT then only checks
whether that system has gone through the latest tick yet.

## 1. The raw observation: two readings of the same system

Every `FSDJump` and `Location` journal event carries the list of factions present in the system,
with their influence (`src/database_import_state.cc`). For each such reading EHT:

- takes `last_seen` from `faction_presence`, which is when this system was last read. This is done
  **before** the current reading is stored, because storing it would move that value;
- compares each faction's influence with the last stored row;
- if the **influence** changed, stores an observation `[previous reading, now]`. The tick must have
  fallen inside that interval (`note_tick`).

EHT watches influence only, not faction states. States can change outside the tick, while influence
is recalculated at the tick and nowhere else, so influence alone marks out the window.

Two influence changes that are not the tick are filtered out:

- **End of a war.** The beaten faction's share is handed out at once. If a conflict passes into an
  empty status in the same reading, no observation is stored (`war_settled_now`). In the author's
  data this filter removed 53 of 735 observations and cut the median window from 38 to 25 minutes.
- **Freshly colonised systems.** Until the first weekly recalculation (Thursday 07:00 UTC),
  influence there follows a rhythm of its own. Such a system is ignored until then
  (`settled_colony_clause` in `src/database_storage.cc`).

## 2. Rare visits give wide windows, and those are thrown away

Say you come back to a system after a week and its influence is different. All that tells you is
that some tick happened during that week, which is useless. Therefore:

- a window wider than **24 h** is not stored at all (`ticks.max_window_h`);
- only windows of **≤ 3 h** are used when building waves (`load_recent_ticks`).

Useful observations come from moments when you entered a system, the tick passed, and you were back
there within a few hours. For example, you were hopping between systems in one area, relogging, or
passing through the same system on a route. **How rarely you visit a system over weeks does not
matter.** What matters is whether any of your play sessions spanned a tick.

Tick observations belong to the galaxy, not to a commander (table `galaxy.tick_observation`). If
several commanders share one journal directory, all of them feed the same data.

## 3. Observations from many systems form a wave, not an intersection

It is tempting to intersect the windows from many systems. That does not work: the galaxy
recalculates system by system, and nearby systems can be recalculated hours apart (the largest
measured spread is 315 minutes). The intersection often comes out empty.

Instead, EHT sorts the observations by window end and groups them into **waves**. A gap longer than
**8 h** (`ticks.same_wave_gap_h`) starts a new wave. Each wave has two ends:

- **Start** (`start_begin..start_end`): the system recalculated earliest. This end matters for
  **influence**: missions have to be handed in before it. It is moved **5 minutes** earlier
  (`ticks.client_lag_min`). A reading with the old value proves only that the new value had not
  reached the client yet, not that the server had not recalculated. When handing in missions, an
  error in this direction is the safe one.
- **End** (`end_begin..end_end`): the system recalculated latest. This end matters for **wars**:
  only after it do combat bonds count towards the new day.

The influence tick and the war tick run on separate clocks. They can be hours apart on the same
day. The war tick is found the same way, from changes in days won (`won_days`) instead of influence.

## 4. Applying the wave to a particular system

For a given system, `describe_tick` (`ui/src/current_state.cc`) shows two things:

- **"changed dd.mm HH:MM"**: when you last saw influence change in this very system. Read it as
  "the tick was here no later than this". It is delayed by your own visit, so it is not a window.
  A reading that brings only a different faction state does not count: the game names the same state
  differently on `FSDJump` and on `Location` (a faction in retreat shows `Retreat`, then `None`, with
  the same influence), so each jump would otherwise read as a tick just now.
- **galaxy `start–end UTC (N sys)`**: the window of the newest wave across the galaxy. The tick of
  your system lies within it, usually well inside, since systems are at most a few hours apart.

The **`awaiting`** flag is set when the last change in this system is older than the start of the
newest wave. It means "the tick has started, but either it has not reached this system yet or you
have not been here since". This is how a rarely visited system still gets a meaningful answer, even
though it cannot give a narrow window by itself.

The two cases are told apart by `last_seen` from `faction_presence`, which moves at every reading,
while influence writes a row only when it changed:

- **"wave started, not seen here since"** (alert colour): no reading of the system since the wave
  began. Missions handed in may still count towards the old day.
- **"wave started, unchanged here since"** (plain colour): the system was read after the wave began and
  its influence had not moved. Either the tick has not reached it yet, or it came and moved nothing,
  which quiet systems do, fresh colonies above all. The journal cannot tell the two apart.

## Limitations

- **The window is an upper bound, not the tick hour.** Fewer sessions make it wider, but do not
  shift it.
- **A day with no session during the tick produces no new wave.** The newest wave is then
  yesterday's, and `awaiting` compares against that older tick.
- **No forecast.** `load_tick_stats` reports only the typical and the longest gap between waves.
- **Settlement war states and the faction support panel are not in the journal** at all, so EHT
  cannot use them.

## Settings

All thresholds live in the `ticks` section of `eht_settings.json`:

| setting | default | meaning |
|---|---|---|
| `max_window_h` | 24 | a wider window between two readings is not stored |
| `same_wave_gap_h` | 8 | a gap longer than this starts a new wave |
| `client_lag_min` | 5 | how much earlier than seen the start of a wave is reported |

The 3-hour limit on windows used for waves is fixed in the query in `load_recent_ticks`.
