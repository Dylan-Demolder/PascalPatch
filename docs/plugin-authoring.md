# Plugin authoring

A PascalPatch plugin is a Windows DLL that runs inside the game, the way BakkesMod plugins run inside Rocket League. The game is never modified: `pascalpatch-launch.exe` starts an unmodified `melee_port.exe` and injects `pascalpatch_runtime.dll`, which loads your DLL and hands it the host table from [`sdk/include/pascalpatch/plugin.h`](../sdk/include/pascalpatch/plugin.h). Plugins are offline-only.

## The pieces

```
plugins/<id>/
  plugin.json        what the store and the F2 overlay show, and the settings
  README.md          shown on the plugin site
  native/            the DLL's source and CMakeLists.txt
```

`plugin.json`:

```json
{
  "id": "input-display", "name": "Input Display", "version": "1.0.0", "abi": 1,
  "entry": "input-display.dll",
  "summary": "Shows a controller's sticks, buttons and triggers on screen.",
  "author": "you", "license": "GPL-2.0-or-later", "tags": ["hud", "training"],
  "min_runtime": "0.3",
  "settings": [
    {"key": "scale", "type": "float", "label": "Size", "default": 1.0, "min": 0.5, "max": 2.0}
  ]
}
```

Setting types: `bool`, `int`, `float` (`min`/`max`), `choice` (`options`: values or `{value, label}`), `text`, and `key` (a hotkey: the default is a key name such as `"F5"`, the F2 window rebinds it by pressing a key, Esc unbinds it, and `setting_number` returns its Windows virtual-key code, 0 when unbound). A plugin can also declare settings itself with `declare_setting`, which is how a plugin loaded straight from a profile gets a settings tab.

`min_runtime` is the oldest PascalPatch that has every host call the plugin uses; the site shows it as "Needs PascalPatch 0.3 or newer". The plugin should still check `PP_HOST_HAS` and refuse to load (return non-zero, with a log line) on an older runtime.

## The DLL

Export `pp_plugin_load(const pp_host *host, const char *config_path)` and return 0. Everything else goes through `host`:

- guest memory (`rd*`/`wr*`), guest calls (`call`), hooks on indirect calls (`hook`, `trampoline`);
- `on_frame` to run once per game frame (VI retrace);
- since 0.2, check `PP_HOST_HAS(host, field)` first:
  - `setting_number` / `setting_text`: read a setting; cheap enough for every frame;
  - `set_status`: one line at the top of the plugin's F2 tab;
  - `hud_text` / `hud_rect` / `hud_circle`: draw from an `on_frame` callback in the game's 640 × 480 space; colours are `0xRRGGBBAA`.
- since 0.3:
  - `hud_label`: outlined text (readable over any stage), left-aligned, centred or right-aligned;
  - `toast`: a short message at the top of the screen, shown even with the overlay closed;
  - `key_down`: whether a key is held, only while the game window has focus and the overlay is closed (so typing in the overlay never fires a plugin's hotkey);
  - `overlay_open`.
- since 0.4:
  - `hud_capsule`: the hull of two circles, for hitboxes, hurtboxes and swept shapes.

Callbacks run on the game's simulation thread, inside the frame: don't do file or network I/O per frame. Blocking stops the game (and only the game: the window, the overlay and presentation carry on, and the port's pacer resumes at 60 Hz afterwards), which is exactly how Training Lab pauses and slows the game; anything else should return promptly.

What a frame's callbacks draw is published when the frame ends, so HUD drawn just before a callback blocks shows up only after the next frame completes.

## Reading the game: `melee.h`

[`sdk/include/pascalpatch/melee.h`](../sdk/include/pascalpatch/melee.h) names what plugins need most, checked against real matches: the player blocks (character, stocks, human/CPU), the fighter struct (action state, animation frame, position, percent, hitlag, hitstun, shield, jumps, the L-cancel counter, the interrupt flag, hitboxes), the scene controller, button bits and fighter kinds. [`melee_states.h`](../sdk/include/pascalpatch/melee_states.h) holds every action state's name (341 common ones plus each fighter's own), generated from the doldecomp/melee enums by `tooling/native/gen_melee_tables.py`.

```c
#include "pascalpatch/melee.h"

void frame(void *user) {
    if (!pp_in_match(host)) return;              /* fighter pointers outlive the match: always check */
    uint32_t fp = pp_fighter(host, 0);           /* port 1, validated, or 0 */
    if (!fp) return;
    uint32_t kind = host->rd32(fp + PP_FT_KIND), state = host->rd32(fp + PP_FT_STATE);
    const char *move = pp_move_name(kind, state);          /* "Nair", "Fsmash", "DamageFlyN" */
    float percent = host->rdf32(fp + PP_FT_PERCENT);
    int can_act = pp_state_is_actionable(state) || pp_can_interrupt(host, fp);
}
```

Helpers cover the usual questions: `pp_state_is_damage`, `_shield`, `_aerial_landing`, `_grabbed`, `_down`, `_punished` (the test combo counters and "openings" are built on), `pp_hitlag`, `pp_kind_name`, `pp_port_rgb` and `pp_rgba` for Pascal UI colours.

Game flags are often left over from an earlier state. The interrupt flag behind `pp_can_interrupt` stays set through shield stun and landings, where the game never reads it, so the helper only counts it in attacks, grabs, throws and specials. Before trusting a field, log it every frame across the moment you care about (see Testing against the game below).

## Examples

Every plugin in `plugins/` is a complete, tested example:

| Plugin | Shows how to |
|---|---|
| `input-display` | draw a HUD from the pad state, with settings |
| `frame-data` | follow two fighters' states frame by frame and time an exchange |
| `tech-trainer` | react to state transitions (landings, jumpsquat, techs) and keep a tally with a reset hotkey |
| `combo-counter` | detect hits from percent changes and group them with the "punished" test |
| `training-lab` | write game memory (percent, shield, stocks) and hold the frame for pause and slow motion |
| `match-stats` | summarise a match when it ends and append a history file beside the DLL |
| `hitbox-viewer` | project world positions onto the screen with the game's camera and draw capsules (falling back to circles on 0.3) |
| `di-trainer` | read the stage's blast zones and a fighter's attributes, and replay the game's own physics to predict a launch |
| `unlock-all` | change the game's save flags only while a screen that reads them is up |

## What makes a plugin useful

The plugins above came from watching what players actually reach for in practice:

- **Answer one question the game hides.** "Was that L-cancel early or late, and by how much?" is worth more than "L-cancel: failed". Name frames, percents and moves, not states.
- **Say it where the eyes already are.** Near the damage meters or under the timer, outlined so it reads over any stage, and gone after a few seconds.
- **Quiet by default, detail on request.** Put raw ids and positions behind a setting.
- **Hands stay on the controller.** Hotkeys for anything used mid-drill, settings in the F2 window for the rest, and a toast to confirm a toggle.
- **Agree with the numbers players already know.** Use the game's own windows (7 frames for L-cancel, 20 for techs) and Slippi's definitions (45 frames to end a punish), so results match replays and wikis.
- **Read-only unless it has to write.** Say in the README when a plugin changes game memory, and only while an option is on.

## Testing against the game

The title-screen demo (scene 24/1) runs real CPU fighters, so a plugin that reads fighter data can be checked by just booting and waiting. For repeatable matches, drive the port with an input script (`--script`, one line per frame: `1800 X`, `1804 L sx=70 sy=-100`, `1810 p=2 R`). The port's own scripts in `port/scripts` assume the Slippi boot menus; without Slippi, boot with a memory card that already has save data, press Start twice to skip the intro, Start at the title, then down, A, A for VS Melee. Log what the plugin decides (`host->log`), and compare the log with what the script did.

## Try it

Put the DLL and its `.json` in a folder and launch through the sandbox (never Slippi):

```sh
pascalpatch-launch.exe --mods <plugin folder> --sandbox <private folder> -- melee_port.exe <port args>
```

The app's Play button does the same for a profile. In game, press **F2**: the overlay lists every plugin, and your plugin's tab shows its settings, status line and log. Changes apply on the next frame and are saved for the next launch. The overlay needs the port's D3D12 renderer (the default); on D3D11 plugins still run but there is no overlay.

## Publish it

See [plugin-site.md](plugin-site.md): the plugin site builds a reproducible, signed package from `plugins/<id>` and your built DLL.

## Static plugins (legacy Dolphin path)

The older static ABI (`plugin_init`/`plugin_shutdown`, composed as PPC source into a disposable decompilation worktree and linked into a DOL) is still validated by the host for Dolphin profiles. It is not dynamic loading. See [static-plugins.md](static-plugins.md).
