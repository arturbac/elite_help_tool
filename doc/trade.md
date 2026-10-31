# Trade

![Best known trades against every market visited, with the stale-market warning further up](images/trade_market.png)

- **The station's market on the overlay**: what it pays above and sells below the galactic
  average, beyond 25% and 500 Cr/t, with minerals that pay well only from mining listed apart. A
  market never opened says so in the side band - easy to miss - and, for a ship actually docked
  there (not a taxi ride, not an on-foot arrival), a second, wide reminder across the top middle of
  the screen in the ordinary overlay size: *open the Commodities Market to record it*
  (`overlay.market_reminder`, its own place and width, on by default). A colonisation building site,
  in space or on the ground, has no market to open, so neither is shown there.
- **"controlling faction changed since last reading - reopen the market"** (`market_stale` colour):
  the market has a reading, but the station's controlling faction has since changed and the market
  itself was never reopened. The game gives no notice when this happens, and what is legal to trade
  can depend on who runs the place, so the old reading may no longer be trustworthy.
- **Best known trades**: what to bring here and what to take from here, against every market you
  have ever opened. Each line gives the margin per tonne, the tonnes (capped by your hold, the stock
  and the demand), the profit of the run and the station. With a route plotted, it looks for what to
  take to its end and what to bring back.
- **Fleet carrier bars**: the bartender's shelf of every carrier visited, with prices and sold-out
  items, filterable to your own carriers. Where each material came from is tracked too, collected
  or from missions, over 30 days, 90 days or all time.
- **Bartender**: what you sold to bartenders, bought and bartered, item by item and summed by
  category (goods, assets, data), with the credits of sales and purchases. The game's own
  `Statistics` already count Horizons material traders, so they are not repeated here.
- **Bar sales** of your own carrier: the game never says a player bought from your bar, but the
  shelf does. Read the bartender on arriving, before adding anything, and again after: a fall in
  stock between readings is a sale at the price shown before it, a rise is what you added. Per
  item: units sold, revenue, in how many absences it sold at all out of those it lay on the
  shelf, and the price against a port's bartender. The port price comes from a built-in list of
  what a port's bartender pays, the same at every port (186 kinds, from the item pages of the Elite
  Dangerous wiki, checked against real sales at ports, every one of which adds up to the credit). The few rare kinds the
  list lacks are worked out from your own sales at ports - a sale of one kind gives its price, a
  sale where all kinds but one are known gives that one. A reading with no stock and no price
  anywhere, written before the bar has loaded, is skipped; a shelf sold out keeps its prices and
  counts as sold. Data, goods and assets
  are listed apart, each with its sums over the heading.
- **Mission value**: which kinds of mission pay best at your carrier's bar - what their material
  rewards fetch there, per mission and in all. The missions' credits are not counted.
  *Sold* is what the rewards have brought: each piece at what one piece put on the shelf has brought
  so far, the revenue of its kind over all the pieces put up (the first reading's stock and every
  rise since). *At the bar* is an estimate instead: the price on the bar times the share of absences
  the kind sold in there, so a price nobody pays counts for little. A kind never put on the bar takes a port's
  price when known and is otherwise counted as unvalued. Mission versions (`_004`, `_007`) are
  counted as one kind. Data and goods are listed apart; assets are left out, as they sell for next
  to nothing. The average is over all the missions of a kind, those that gave nothing of the
  category too.

These are the tabs of the Asset trade window. It reads its tables only while it is shown, and at most
once a second as items are picked up; a hidden window is read when it is shown again.

- **Holdings**, in the Assets window: every kind of data, goods and assets you hold, in three tables,
  with how many are in the backpack, in the ship's locker, in the store of each of our carriers (your own
  fleet carrier and the squadron's carrier) and the total - to see at the mission board
  whether a reward is one you lack or one you have plenty of. The backpack and the locker are the
  game's own lists; a mission's thing to deliver is left out, and so are consumables. The game writes
  what is picked up or taken out of the locker on foot at once; what is moved while aboard it carries out
  only on leaving the ship, and on boarding the backpack goes into the locker without a word for the
  backpack - so the backpack counts as emptied of them then. A carrier's store is in no journal, so it is
  kept as the cargo is: on docking at one of our carriers the backpack and the locker are noted
  together, and on leaving what they have less of was left in the store, what they have more of was
  taken from it - handed in at your own bar included. The bar of your own fleet carrier sells out of its
  store, so a reading of its bartender - yours in the game, or another commander's heard through EDDN -
  is the store's count of every kind on sale, a sold out one at 0; a kind not on sale keeps the count of
  the visits. A reading during a visit books what was moved so far and measures afresh from it, so what
  is added between two readings counts once; one heard from EDDN that is older than the store's last
  change is not applied. Double-click a carrier's count to correct it; a kind held nowhere else - many lie in a
  carrier's store only - is set with the row under the tables (carrier, kind, count, *Set*), and shows in
  the table of its kind. Tick *Junk* on a kind to mark it as junk; with *No junk* (on at every start)
  marked kinds are hidden in all three tables - clear it to see them again and take the mark off. The
  mark is kept in the micro resource dictionary in `live.sqlite`, the same for both commanders. It is kept in `live.sqlite` and counts only while EHT runs.
- **Bartender stock** (the first tab of Asset trade): the bartender's shelf of every carrier as last
  read, what is for sale and at what price - read it before and after adding to see what sold. With
  `eddn.listen` on, the shelf of your own fleet carrier is read for you too whenever another commander
  opens its bar with a tool that sends to EDDN (see [privacy.md](privacy.md)); such a reading shows
  the shelf before their purchase, and the next one shows what they bought.

On-foot consumables and kills are tracked too, but that lives with the rest of the on-foot combat
tracking in [missions_on_foot.md](missions_on_foot.md).

Market data is recorded when you open the commodity market, and kept in `live.sqlite`, the one
database that cannot be rebuilt from journals.
