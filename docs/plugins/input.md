# Hotkeys and controllers

## Hotkeys

Give every hotkey a `key` setting rather than hard-coding a key, so players can rebind it in the F2 window:

```json
{"key": "reset_key", "type": "key", "label": "Reset the tally", "default": "F9"}
```

```cpp
bool g_was_down = false;
void frame(void*) {
  int vk = (int)H->setting_number(ID, "reset_key");     // 0 when unbound
  bool down = vk && H->key_down(vk);
  if (down && !g_was_down) { reset(); H->toast(ID, "Tally reset"); }
  g_was_down = down;
}
```

- `key_down` is true only while the game window has focus and the F2 overlay is closed. Typing in the overlay or in another window never fires a plugin.
- Act on the press, the first frame the key is down, not on every frame it is held. Keep one "was down" flag per key.
- Confirm toggles with a `toast`, because the player's eyes are on the game.

### Key names

For `default`, and what `setting_text` returns:

- `A` to `Z`, `0` to `9`, `F1` to `F24`, `Numpad0` to `Numpad9`
- `Backspace`, `Tab`, `Enter`, `Shift`, `Ctrl`, `Alt`, `Pause`, `CapsLock`, `Space`
- `PageUp`, `PageDown`, `End`, `Home`, `Left`, `Up`, `Right`, `Down`, `Insert`, `Delete`
- `NumpadMultiply`, `NumpadAdd`, `NumpadSubtract`, `NumpadDecimal`, `NumpadDivide`
- `Semicolon`, `Equals`, `Comma`, `Minus`, `Period`, `Slash`, `Grave`, `LeftBracket`, `Backslash`, `RightBracket`, `Quote`

### Keys already in use

Choose a default that does not collide. Players can rebind any clash, but a good default saves them the trouble.

| Key | Used by |
|---|---|
| F1, F2, F3, F11 | Melee Unlocked (F1 PC settings, F3 Lab view, F11 menu) and the PascalPatch overlay (F2): keep clear |
| F4 | Match Stats |
| F5, F6, F7, F9 | Training Lab: pause, step, slow motion, reset |
| F8 | Tech Trainer |
| F10 | DI Trainer |
| F12 | Wavedash Trainer |
| Insert | Info Display |
| Home, End, Delete, PageUp, PageDown | Training Lab: dummy, save, load, record, play |
| Backspace | Quick Match: restart |
| Numpad0 | the SDK template |
| Numpad1 | Hitbox Viewer |

The number row, letters and the rest of the numpad are free. Many players use a keyboard for the game itself, though, so letters are a poor default.

### Controller hotkeys

Players practising with a controller in hand like controller shortcuts. Training Lab uses the D-pad, which Melee barely uses in a match. Read the buttons from the fighter (`PP_FT_PRESSED` is the rising edge):

```cpp
uint32_t pressed = H->rd32(fp + PP_FT_PRESSED);
if (pressed & PP_BTN_DR) save();
```

Only take over a D-pad direction behind a setting, because some characters use it (Peach's turnip, taunts).

## Reading controllers

For what a fighter acts on, read the fighter: `PP_FT_HELD`, `PP_FT_PRESSED`, `PP_FT_RELEASED`, `PP_FT_STICK` (x, y from -1 to 1), `PP_FT_CSTICK` and `PP_FT_TRIGGER`. These are after the game's dead zones and clamping, so they are exactly what the move logic sees. For the raw controller state before any processing (an input display that shows the stick's true position, say), read `HSD_PadMasterStatus` at `PP_PAD_STATUS`, one entry every `PP_PAD_STRIDE` bytes per port. [input-display](../../plugins/input-display) does this.

## Controlling a port: `pad_set` (0.5)

`pad_set` holds a port's controller in a state from the next frame on, as if a controller were plugged in and held that way. `pad_release` hands the port back.

```cpp
pp_pad_state s{};
s.buttons = PP_BTN_R;          // a full press needs the digital bit ...
s.trigger_r = 1.0f;            // ... and the analog value, as on a real controller
H->pad_set(1, &s);             // port 2 shields
...
H->pad_release(1);
```

- The state goes in before the game reads the controllers, so the game's own clamping, dead zones and button edges apply. The fighter sees exactly what a real controller would give. (Checked in game: a stick of -0.5 reads -0.5000.)
- Only human ports follow it. A CPU's fighter reads its AI, so a dummy port must be set to human.
- A button press needs a frame with the button up before it: holding A for ten frames is one press. To mash, alternate on and off.
- Call `pad_set` every frame you want to change the state. It holds the last state until you change it or release it.
- Release the port when the feature is switched off, and when a match ends.

`pad_set` is what dummies, input recording and playback, and scripted drills are built on. Training Lab's dummy ([training_lab.cpp](../../plugins/training-lab/native/training_lab.cpp)) shields, DIs, techs, answers the end of hitstun with an action, recovers to the stage and plays back recordings, all through `pad_set`. Recording is the reverse: store the fighter's stick, C-stick, trigger and buttons each frame, then feed them back.

Treat another player's port with care. Only drive a port the player picked in a setting, and never the port they are playing on.
