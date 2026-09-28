# Designing a useful plugin

A plugin that gets installed once and switched off is not worth the effort of building. These notes come from building the first-party plugins and watching what players actually keep on in practice.

## Start from a question a player asks

Good plugins answer a question the game hides:

- "Was that L-cancel early or late, and by how much?"
- "How many frames did I have to act out of shield?"
- "Why did I die there? Was my DI bad?"
- "What is this move's startup, and am I plus on shield?"
- "How much of my punish game is converting?"

Write the question in one sentence before you write code. If it takes a paragraph, the plugin is two plugins.

Bad starting points are "show all the fighter's data" or "a menu of options". They are fun to build and hard to use.

## Say it in the player's words

- **Moves and frames, not state ids.** "Nair, landed on frame 4, L-cancelled 2 frames early" beats "AttackAirN → LandingAirN, 0x67F=5".
- **Tell players what to change.** "Late by 1 frame" is something they can act on. "Missed" is not.
- **Use the numbers players already know.** Use the game's own windows (7 frames for an L-cancel, 20 for a tech) and Slippi's definitions (45 frames without a hit ends a punish), so your results match replays, wikis and coaches.

## Say it where the eyes already are

A player mid-drill watches their fighter and the damage meters, not the corners.

- Put short feedback near the fighter or above the damage meter, outlined (`hud_label`), and fade it out after a second or two.
- Use corner panels (`hud_place`) for running totals the player glances at between reps.
- Use colour to mean the same thing everywhere: green good, yellow close, red missed. And the port's colour for whose data it is.

## Quiet by default, detail on request

- The first launch should show one clear thing. Put raw values, extra lines and debug views behind settings.
- Anything that fires often gets a way to turn it off.
- Stay silent when there is nothing to say. An empty panel saying "waiting..." is noise.

## Hands stay on the controller

- Give a hotkey to anything used mid-drill: reset the tally, toggle the view, save or load a state. Confirm it with a toast.
- Controller shortcuts (on the D-pad) are even better for training features, because the player never lets go of the pad. Put them behind a setting.
- Everything else goes in the F2 settings, with a `help` tooltip wherever the label is not enough.

## Play well with others

- Share the screen with `hud_place`. Never cover the damage meters or the timer.
- Pick hotkeys from the free ones (see [input](input.md)).
- One job per plugin. A frame data plugin that also counts combos makes both harder to turn off separately.

## Read-only unless it must write

Most plugins should only read. When yours changes the game (sets percent, holds a controller, loads a state, changes the stage):

- make each change a setting that is off unless the player turns it on, or a clear hotkey action;
- say in the README exactly what it writes;
- undo it when switched off: release the controller, stop overriding values.

## Test it on people

Hand it to someone who did not write it. Watch where they look, what they misread, and what they switch off. The first-party trainers each changed a lot after this. Tech Trainer, for example, started as a single pass or fail line and became a per-technique coach that names the frame.

## Ideas that would help players

If you are looking for something to build, these come up often:

- a ledge trainer: ledgedash intangibility, regrab timing, getup options;
- a shield-drop and platform-drop trainer;
- a crouch-cancel and ASDI-down helper showing the percent a move stops being crouch-cancellable;
- a character-specific trainer (Fox and Falco lasers, Peach float cancels, Marth tipper spacing, Sheik tech chases);
- a punish summary after each stock: openings, damage per opening, kill move;
- a stage-control heat map of where each player spent the match;
- accessibility: high-contrast outlines for fighters, larger damage numbers, colour-blind port colours;
- a reaction-time trainer: flash a cue, measure the frames to the player's input.

Check the plugin site first. If someone is already building your idea, improving theirs usually helps players more than a second one would.
