# Community goals

The **Community goals** window lists the community goals the commander has signed up for, with the
commander's own part in each and the reward bracket that part reaches.

## Where the numbers come from

The game writes a `CommunityGoal` journal event at login and again each time a goal's panel is opened.
The window shows the last of those readings; nothing is stored in the database. After a delivery the
band is the old one until the goal's panel is opened again.

## Columns

| column | meaning |
|---|---|
| Goal | the goal's title |
| System / market | where the goal is handed in |
| Contributed | the commander's contribution so far |
| Band | `PlayerPercentileBand`: the best percentage of contributors the commander is in (10, 25, 50, 75, 100) |
| Bracket | **top 50%** (green), **top 75% only** (amber) or **below 75%** (red) |
| Contributors | how many commanders have contributed |
| Tier | the tier the goal has reached |
| Time left | until the goal's expiry, or "complete" |

Many goals give their rewards by these brackets, so the bracket is what tells whether more deliveries are
needed. The threshold of each band moves while others deliver, mostly in the last days of a goal.

## Log

Each time the band of a goal changes, one line goes to the log: the goal, the contribution, the band and
the number of contributors.
