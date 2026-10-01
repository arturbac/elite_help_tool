# Credits: where your money came from and where it went

The **Credits** window shows the player's finances, read from the journal alone. Nothing is typed in and
nothing is stored: a background thread reads the journal's whole history when EHT starts (a couple of
seconds for a year and a half of a busy commander, about 1.8 GB of journal files), and every 30 seconds
after that it rereads only the files that grew.

## What the window shows

- **The balance now**: the game's own latest reading, plus every movement the journal recorded after
  it. The tooltip says when the game last read it and how much has moved since.
- **This session**, counted from its `LoadGame`: earned, spent, net, net per hour played, and a row
  for each kind of income or expense that moved at all, with how many events made it up.
- **History** by day, week (ISO, Monday to Sunday), month, quarter or year, newest first, in the
  machine's own time zone. For each period: time played, earned, spent, net, net per hour, money moved
  between your own pockets, the balance at the period's end, and one column for each kind of income
  or expense that ever appeared. A cell's tooltip gives the number of events behind it.

**Earned** and **spent** add up the categories that came out ahead or behind in that period. Transfers
between your own pockets are left out of both: a fleet carrier's bank and the squadron bank.
**Played** runs from `LoadGame` to the last line of the session, so time left idle in the game counts too.

Every commander found in the journal files gets a ledger. The commander of the newest session is shown
first, and the others can be picked from the list.

## The categories

| Category | From |
|---|---|
| Exploration | `SellExplorationData`, `MultiSellExplorationData`, codex vouchers; bought exploration data |
| Exobiology | `SellOrganicData` (value and first-logged bonus) |
| Trade | `MarketSell`, `MarketBuy` - but see colonisation goods below |
| Missions | `MissionCompleted` reward, less any donation |
| Bounties & bonds | `RedeemVoucher` of bounties and combat bonds |
| On-foot goods | `SellMicroResources`, `BuyMicroResources` |
| Other income | community goals, search and rescue, other vouchers |
| Ships & modules | ships and modules bought and sold, including a ship or module sold in the same purchase |
| Suits & weapons | suits and weapons bought, sold and upgraded |
| Upkeep | fuel, repairs, ammunition, vehicle restock, limpets, ship and module transfers, taxis, dropships |
| Rebuy | `Resurrect` |
| Fines & bounties | `PayFines`, `PayBounties`, `PayLegacyFines` |
| Crew | `NpcCrewPaidWage`, `CrewHire` |
| Engineers | credits given to an engineer |
| Powerplay | salary and fast-track |
| Carrier | buying and decommissioning a fleet carrier |
| Colonisation goods | goods handed in at a construction site, at what they were last bought for |
| Carrier bank | `CarrierBankTransfer` deposits and withdrawals |

**Colonisation goods** are moved over from Trade when they are delivered. `ColonisationContribution` names
the goods and their counts, and each is priced at the last `MarketBuy` of that commodity. That way Trade
shows only real trading, and colonisation shows what it cost.

## What the balance says and no event does

The game reports the balance itself on every `LoadGame` and on every carrier bank transfer
(`PlayerBalance`). Between two such readings, the movements the journal names should add up to the
difference, and most of the time they do, to the credit. When they do not, the difference is booked as
well, so the window always agrees with the game. It goes into one of three columns, each marked with a star:

- **Squadron bank\***: deposits and withdrawals have no journal event at all. A difference in whole
  millions is taken for one, and so is a large difference (100 million or more) that is not a
  colonisation payout. That one is rounded to the million, and the small rest goes to Unexplained,
  since something else happened in the same stretch. On two commanders sharing a squadron, a
  withdrawal on one account shows up as a deposit on the other within the hour.
- **Colonisation payouts\***: construction sites pay for the goods delivered, about 1.3 times what
  they were bought for going by the history, but `ColonisationContribution` carries no amount. A
  positive difference in a stretch with a delivery in it is booked here, at the time of the last
  delivery.
- **Unexplained\***: whatever is left. It is mostly small, and often caused by the reading's own timing:
  a purchase in the same second as `LoadGame` can land on either side of it.

Differences found within three hours of each other that add up to nothing are dropped: they are one
payment read on both sides of a reading. Buying a Squadron Carrier is one example, since the price left
the balance before `CarrierBuy` was written.

Crew wages come off the balance as written, except right after a sale of exploration data or on-foot
goods: those sales' sums are already net of the crew's cut, and the wage logged with them is not taken
off a second time.
