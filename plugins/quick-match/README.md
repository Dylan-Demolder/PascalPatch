Starts the game straight in a match: no intro movie, no title screen, no character or stage
select. About eight seconds after launch you are playing.

In the F2 window, pick:

- the two fighters;
- the stage (the six tournament stages);
- whether player 2 is a human port (so Training Lab's dummy can play it), a CPU (level 1 to 9)
  or nobody;
- the rules: endless (no time limit, no stocks, for practice), 4 stocks or 1 stock.

Changes apply from the next match. **Backspace** restarts the match, with fresh stocks and both
fighters back at their spawn points. Set "When the game starts" to the title screen to get the
usual menus back without removing the plugin.

It uses the VS mode Melee's developers left in the game for testing, which fills in the match
from code instead of the select screens, so the memory card is never touched.

For tools: a `"match"` in the `quick-match.json` beside the DLL overrides the settings, e.g.
`{"match": {"p1": "fox", "p2": "marth", "p2_player": "human", "stage": "fd", "rules": "endless"}}`
(`p1` and `p2` also take a character number, 0-25, in select-screen order). PascalPatch writes
one for Character Studio's "Test in game".
