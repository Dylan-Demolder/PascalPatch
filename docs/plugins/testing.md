# Testing and debugging

## The loop

1. Edit your source.
2. Build: `cmake --build build --config Release`.
3. Install the new build: `python <PascalPatch>/tooling/pack_plugin.py . build/Release/<id>.dll --install`.
4. Relaunch the game from the app (**Play**). A DLL is loaded once, when the game starts, so a running game keeps the old one.
5. Get to a match fast with Quick Match, below.

Put steps 2 and 3 in a script and the loop takes seconds.

## Get to a match in one step: Quick Match

Install **Quick Match** from the app's Browse page. It boots the game straight into a match, skipping the intro, the title screen and both select screens:

- choose the fighters, the second player (a human, a CPU level from 1 to 9, or nobody), the stage, and the rules (an endless match, 4 stocks, or 1 stock) in its settings;
- **Backspace** restarts the match with everyone back at their spawn points;
- after the results you are on the normal menus.

`pp_in_match` is true during a Quick Match. Its scene is major 0x0E, minor 1.

For training plugins, set the second player to "human" and drive it with [`pad_set`](input.md), or plug in a second controller.

## Logs

- `H->log(ID, "text")` writes a line to the run log, prefixed `[mod:<id>]`. The last 60 lines also appear on your plugin's F2 tab.
- The F2 window's **Console** tab shows the whole log live, including the runtime's own lines: which plugins loaded, hook warnings, reads outside RAM.
- After a run, the full log is at `<data>/logs/<profile>/<time>.pascalpatch.log`. The data folder is `%USERPROFILE%/.local/share/pascalpatch` unless `PASCALPATCH_DATA` points elsewhere. `pascalpatch logs <profile>` lists the files.

Log decisions, not every frame: "tech: in place on frame 3 (window 20)" rather than 60 lines a second. When you do need per-frame values, log only across the moment you care about, from a few frames before to a few after, then remove it.

## The F2 window

Press **F2** in game. Your plugin's tab shows:

- **its status line** (`set_status`). Use it for live state while developing: the counters, the mode, what the plugin last saw;
- **its settings**, which change on the next frame. That makes them a quick way to try values without rebuilding: temporarily expose a threshold as a setting, tune it in game, then set the default;
- **its log**, and "failed to load" with the reason if `pp_plugin_load` returned non-zero.

`key_down` is always false while the window is open, so your hotkeys cannot fire while you type in it.

## Checking against the game

A plugin is only as good as its reading of the game. Before you trust a field or a rule:

1. **Watch it change.** Show the value on screen, or log it on every change, while you do the thing slowly. Then do it fast.
2. **Try the edges.** Try it frame-perfect, one frame early, one frame late. Try every character that does it differently (Peach's float, Yoshi's jump, the Ice Climbers, Zelda and Sheik transforming). Try the stage's edge, a platform, the ledge.
3. **Compare with what players know.** If a frame count disagrees with the wikis or with Slippi replays, find out why before you publish.
4. **Leave and come back.** Finish the match, go to the menus, start another. A plugin that reads stale fighter pointers or forgets to reset its state shows up here.
5. **Test beside other plugins.** Turn on Frame Data, Hitbox Viewer and Training Lab with yours and check that panels do not overlap and hotkeys do not clash.

### Repeatable inputs: savestates and `pad_set`

For timing-sensitive checks, a plugin can test itself:

1. save a state just before the moment;
2. drive a port with `pad_set`, input by input;
3. read the result, load the state, and try the next timing.

See [savestates](savestates.md).

### Input scripts

The port can also play a script of inputs (`--script <file>`, one line per frame such as `1800 X`, `1804 L sx=70 sy=-100`, or `1810 p=2 R`). Pass it through the launcher after `--`. Scripts are timed from boot, so combine them with Quick Match for a match that starts on a known frame.

## When it goes wrong

| Symptom | Likely cause |
|---|---|
| The plugin is not on the F2 list | The DLL is not named `<id>.dll`, or it is not in the installed plugins. Check the Console tab for the load lines. |
| "failed to load" | `pp_plugin_load` returned non-zero: your log line says why. Often an old PascalPatch (`PP_HOST_HAS`). |
| Settings missing | The `plugin` argument to the settings calls is not your id. |
| Nothing drawn | `pp_in_match` is false (are you in a match?), the overlay is off (D3D11 renderer), or you drew outside 0 to 640 × 0 to 480. |
| Garbage values after a match | Fighter pointers are read outside a match. Check `pp_in_match` first. |
| Hotkey fires many times | Act on the press edge, not while the key is held. |
| The game freezes | A callback blocks or loops. Callbacks run inside the game's frame. |
| The game crashes | A write to the wrong address, or a hook that skipped the original. Remove writes until it stops, and check every address against the decomp. |

## The port's settings window

The port opens its own settings window at every launch unless its settings file, `port-settings.ini`, has the line `startup 0`. That window covers the game, so for scripted or automated tests, pass the port `--settings-path <file>` with a file that contains `startup 0`.
