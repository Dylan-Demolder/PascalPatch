Practice tools for any match, on hotkeys (rebind them in the F2 window):

- **F5 pause / resume** and **F6 frame advance**: freeze the game and step through a
  combo or a move one frame at a time. Pair it with Frame Data to see every state change.
- **F7 slow motion**: half or quarter speed, for learning timings by feel.
- **F9 reset percent**, or **lock** a player at a set percent to drill a kill confirm or a
  combo at exactly the percent it works at.
- **Infinite shield**: shields never shrink or break, for practising out-of-shield options.
- **Endless stocks**: nobody runs out, so a practice match never ends.
- **Savestates** (PascalPatch 0.5): **D-pad right** (or **End**) saves the moment, **D-pad left**
  (or **Delete**) puts it back: both fighters, their percents, the stage, the timer, everything.
  Set up a situation once and replay it as often as you like, as in UnclePunch's Training Mode.
  A state belongs to the match it was saved in.
- **Drills**: have the state come back on its own, when the exchange is over (the dummy's port
  is free again, or KO'd) or after 3 or 5 seconds, with a try counter. Save the moment before
  your pressure, your edge-guard or your tech chase starts, and run it again and again.

Percent, shield and stock options apply to the player you pick: port 1, port 2 (the default, your
practice partner), everyone, or only CPU players. Hotkeys only work while the game window has
focus and the F2 window is closed.

## The dummy

On PascalPatch 0.5, Training Lab can play a practice partner for you, in the spirit of
UnclePunch's Training Mode. Set up a match with the dummy's port (port 2 by default) as a
**human** player, then press **Home** to hand that port to the dummy (or turn it on for every
match in the settings). It plays through the port's controller, so it is held to the same rules
as you are:

- **Stance**: stands, crouches (for crouch-cancel practice), holds shield, or keeps jumping.
- **DI**: survival (up and in), combo (down and away), in, out, or random per hit; plus
  **SDI** during hitlag, some or as much as possible.
- **Techs**: in place, toward you, away, never, or random (misses included), wherever it lands,
  platforms too. After a missed tech it **gets up** the way you pick: stand, roll either way,
  getup attack, or random, on the first possible frame, for tech-chase practice.
- **Counter-actions**: when hitstun ends, or when shield stun ends, it jumps, nairs, grabs,
  spot dodges, rolls, up-Bs, down-Bs, air dodges or attacks, on the first possible frame: out of
  shield that means a frame-1 shield grab, or a jump-cancelled up-B or shine, so you learn what
  is safe on shield and what is not.
- **Recovery**: knocked off Battlefield, Final Destination, Dream Land, Yoshi's Story, Fountain
  of Dreams or Pokemon Stadium, it double jumps back and up-Bs, for edge-guard practice.
- **Mash out of grabs**, so you learn which throws and pummels you really get.
- **Record and play back**: press **PageUp** to record your own inputs (up to 20 seconds; press
  it again to stop), then **PageDown** to have the dummy play them back, once or on a loop,
  mirrored when it faces the other way. Record a pressure string, then practise against it.

A badge in the bottom-left corner shows what the dummy is doing. Only teching touches game
memory: the dummy marks a well-timed L/R press in its tech timer, because when a tumble ends
depends on where it lands.

Pause and slow motion hold the game inside the frame; the window, the F2 window and other
plugins keep running. This plugin writes game memory (percent, shield, stocks, the tech timer,
a loaded state) only while those options are on or when you load.
