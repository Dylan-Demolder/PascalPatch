# Savestates

Since PascalPatch 0.5, a plugin can capture the game and put it back, the way a training mode's "save position" does.

```cpp
H->state_save(0);    // capture into slot 0 at the start of the next frame
H->state_load(0);    // put slot 0 back at the start of the next frame
```

- There are four slots, 0 to 3 (`PP_STATE_SLOTS`). All plugins share them, so pick a slot in a setting, or use slot 0 and say so in your README.
- Both calls are requests. They happen at the start of the game's next frame, when the game is between frames, and return 1 if the request was taken. `state_load` returns 0 when the slot is empty.
- A savestate covers what Slippi's rollback covers: the game's data and heap. It does not cover sound, video or the controllers, so a load never replays old button presses.

## Only load into the match you saved in

A savestate holds one match: its stage, fighters and every object in it. Loading it on a menu or into a different match puts the game in a state it was never in. It may look fine, or it may crash later. So:

- save and load only while `pp_in_match` is true;
- keep a match counter, increment it every time a match starts, store it with the slot, and refuse to load a slot from another match.

```cpp
int g_match = 0, g_saved_match = -1;
bool g_was_in = false;

void frame(void*) {
  bool in = pp_in_match(H);
  if (in && !g_was_in) ++g_match;
  g_was_in = in;
  if (!in) return;
  if (pressed("save_key") && H->state_save(0)) { g_saved_match = g_match; H->toast(ID, "Saved"); }
  if (pressed("load_key")) {
    if (g_saved_match != g_match) H->toast(ID, "Nothing saved in this match");
    else if (H->state_load(0)) H->toast(ID, "Loaded");
  }
}
```

Training Lab does exactly this, and adds an option that loads the state back on its own after each attempt, as a drill.

## Your own state

A load rewinds the game, not your plugin. Anything your plugin tracks (counters, "was down" flags, timers, a recording's position) still holds the values from before the load. After you request a load, reset whatever depends on the game's past, typically on the next frame. A frame-data plugin, for example, should drop an exchange it was timing.

## Other uses

- **Drills:** save just before the situation to practise (a ledge, a tech chase), then load after each try.
- **Automated checks:** a plugin can save, drive a port with [`pad_set`](input.md), measure, load, and try the next input. This is how you can check a technique's timing frame by frame.
- **Rewind:** save every few seconds into a ring of slots. Remember there are only four, and other plugins may want them.
