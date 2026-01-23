# openspiel_grid16

Source: OpenSpiel `pathfinding` game rules (single-agent case).

Constructed from the OpenSpiel pathfinding definition (4x4 empty grid, start at
(0,0), goal at (3,3), no obstacles). Actions: stay, left, up, right, down.
Rewards follow OpenSpiel defaults: step -0.01, solve +100, group +100; reaching
the goal yields +200 once and then the goal state is absorbing.

Initial distribution: deterministic at the start cell (0,0).
