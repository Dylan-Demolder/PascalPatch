Frame data on screen while you play, the way the practice tools in modern fighting games show it.

**State panels.** One small panel per fighter (port 1, ports 1 and 2, or everyone): the character,
what they are doing ("Nair f7", "Guard", "DamageFlyN"), and chips for hitlag, hitstun and
"CAN ACT", the moment the fighter can move again. A thin bar along the bottom is their shield.

**Frame advantage.** Every time an attack connects, on a body or on a shield, the plugin counts
the frames until each side can act again and shows the difference under the attacker's panel:

- `Jab on shield -10`: the defender can act 10 frames before you, so they can punish.
- `Nair on shield -24 (landed 12f later)`: aerials are only as safe as they are low; this one hit
  12 frames before landing. Hit lower (or L-cancel) and the number climbs.
- `Usmash: true combo`: the second hit came before the defender could act.

Positive numbers mean you are plus (green), negative mean minus (red). Pausing the game stops the
clock. Turn on **Show state ids and positions** to see raw action state ids and coordinates, handy
when you are writing a plugin of your own.

Read-only: it never changes the game.
