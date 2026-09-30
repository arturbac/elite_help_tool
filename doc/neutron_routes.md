# Neutron routes

- **The Route window** loads a neutron route plotted by [Spansh](https://spansh.co.uk) (its JSON file)
  and follows it: which waypoint is next, how many are left, and the next one put on the clipboard at
  every arrival, for the galaxy map's search. *Copy next destination* puts it there again.
- **Reversed** flies the file from its end. It can be set before loading or changed on a loaded
  route; the distances are counted anew from the coordinates.
- Progress moves at an arrival only, and only forward: the game's own course between two waypoints
  may pass systems off the list, and a route there and back passes the same system more than once.
  A double click on a row makes that waypoint the next one.
- **Remember** keeps the route across restarts, **Forget** drops it. **Clear** drops a route loaded
  for one trip only; a remembered route, if there is one, is shown again.
- **On the overlay, in flight**: a small reminder of what to do right now with the remembered route -
  go to the next waypoint, approach it and jump, or that a jump is under way - so the ritual can be
  followed without the Route window open. Shown only in supercruise, in the main ship; docked or on
  foot it goes quiet, since neither has anything to do with the next jump.
