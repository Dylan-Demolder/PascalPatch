# The host API

Everything a plugin does goes through the `pp_host` table that `pp_plugin_load` receives. The table is defined in [`sdk/include/pascalpatch/plugin.h`](../../sdk/include/pascalpatch/plugin.h), and that header is the authority. This page groups the functions by job and says which version of PascalPatch added each one.

## Loading

```cpp
extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char* config_path);
```

- It is called once, before the game's first frame. Keep `host`, because it stays valid for the whole run.
- `config_path` is `<id>.json` beside the DLL, or NULL. See [plugin.json](plugin-json.md#the-config-file).
- Return 0 to stay loaded. Any other value unloads the plugin, and the F2 window marks it "failed to load". Log the reason first.
- There is no unload callback: a plugin lives until the game closes.

## Versions and `PP_HOST_HAS`

The table only grows. A newer runtime serves every older field, and `host->size` says how much of the table the running PascalPatch has. Before calling anything added after 0.1, check it:

```cpp
if (!PP_HOST_HAS(host, pad_set)) { host->log(ID, "needs PascalPatch 0.5 or newer"); return 1; }
```

To support older runtimes, degrade instead of refusing. For example, the hitbox viewer draws circles when `hud_capsule` (0.4) is missing. Set `"min_runtime"` in plugin.json to the version your plugin needs, so the app and the site can say so before anyone installs it.

| Since | Functions |
|---|---|
| 0.1 | `log`, `rd8/16/32`, `rdf32`, `wr8/16/32`, `wrf32`, `reg`, `set_reg`, `freg`, `set_freg`, `call`, `trampoline`, `hook`, `on_frame`, `guest_alloc` |
| 0.2 | `declare_setting`, `setting_number`, `setting_text`, `set_status`, `hud_text`, `hud_rect`, `hud_circle` |
| 0.3 | `toast`, `key_down`, `overlay_open`, `hud_label` |
| 0.4 | `hud_capsule` |
| 0.5 | `hud_place`, `pad_set`, `pad_release`, `state_save`, `state_load` |

## Threads and timing

Every callback (`on_frame`, hooks, trampolines) runs on the game's simulation thread, in the middle of a frame. Consequences:

- **Return quickly.** The game waits for you. File or network I/O and long loops belong at load time, or in a match-end handler that runs once.
- **No locks needed** between your own callbacks: they never run at the same time.
- **Blocking pauses the game, and only the game.** The window, the F2 overlay and drawing carry on. Training Lab pauses and slow-steps the game this way. The port's pacer catches up to 60 Hz afterwards.
- HUD drawn in a frame appears when that frame ends. So HUD drawn just before a callback blocks shows up only after the next frame completes.

## Logging and status

| Function | Since | |
|---|---|---|
| `log(plugin, message)` | 0.1 | One line in the run log (`[mod:<id>] message`) and the last 60 lines on the plugin's F2 tab. Cheap, but don't log every frame outside a debugging session. |
| `set_status(plugin, text)` | 0.2 | One line at the top of the plugin's F2 tab: what it is doing right now ("Recording: 142 frames", "Dummy: shield"). |
| `toast(plugin, text)` | 0.3 | A short bubble at the top of the screen for a few seconds, even with the overlay closed. Use it to confirm a hotkey ("Infinite shield on"). A newer toast replaces the older one. |

The `plugin` argument is your plugin's id. It must match `id` in plugin.json, or the settings and log lines will not find your tab.

## Game memory

```cpp
uint8_t  rd8(uint32_t addr);    uint16_t rd16(uint32_t addr);
uint32_t rd32(uint32_t addr);   float    rdf32(uint32_t addr);
void wr8(uint32_t addr, uint8_t v);   ...   void wrf32(uint32_t addr, float v);
```

- Addresses are the GameCube's, `0x80000000` to `0x817FFFFF`, exactly as the decomp and every Melee memory map list them.
- Byte order is handled for you: values come back in the PC's order.
- A read outside RAM returns 0 and logs a warning instead of crashing, but a wrong pointer still gives garbage. Check pointers with `pp_is_ptr` before following them. `melee.h`'s helpers do this for you.

See [reading the game](game-data.md) for what is at which address.

## Settings

| Function | Since | |
|---|---|---|
| `declare_setting(plugin, spec_json)` | 0.2 | Adds a setting (the plugin.json form). Ignored when plugin.json has that key already. Call it from `pp_plugin_load`. |
| `setting_number(plugin, key)` | 0.2 | bool (0/1), int, float, a choice's index, a key's virtual-key code. 0 for an unknown key. |
| `setting_text(plugin, key)` | 0.2 | text, a choice's value, a key's name. The pointer stays valid until your plugin's next `setting_text` call, so copy it if you need two at once. |

## Drawing

All drawing happens inside an `on_frame` callback, in a 640 × 480 space that is scaled to the window. Colours are `0xRRGGBBAA`. See [drawing on screen](hud.md).

| Function | Since |
|---|---|
| `hud_text(x, y, rgba, size, text)` | 0.2 |
| `hud_rect(x0, y0, x1, y1, rgba, rounding, filled)` | 0.2 |
| `hud_circle(x, y, radius, rgba, filled)` | 0.2 |
| `hud_label(x, y, rgba, size, align, text)`: outlined text | 0.3 |
| `hud_capsule(x0, y0, r0, x1, y1, r1, rgba, filled)` | 0.4 |
| `hud_place(corner, w, h, &x, &y)`: a spot for a panel that no other plugin covers | 0.5 |

## Input

| Function | Since | |
|---|---|---|
| `key_down(vk)` | 0.3 | Whether a keyboard key is held. Only while the game window has focus and the overlay is closed. |
| `overlay_open()` | 0.3 | Whether the F2 window is open. |
| `pad_set(port, &state)` | 0.5 | Hold a controller port in a state from the next frame on, as if a controller were plugged in. |
| `pad_release(port)` | 0.5 | Give the port back to its real controller. |

See [hotkeys and controllers](input.md).

## Savestates

| Function | Since | |
|---|---|---|
| `state_save(slot)`, `state_load(slot)` | 0.5 | Capture or restore the game in slot 0 to 3, at the start of the next frame. |

See [savestates](savestates.md).

## Running game code

| Function | Since | |
|---|---|---|
| `on_frame(fn, user)` | 0.1 | Run `fn(user)` once per frame, at the vertical blank. Register as many as you like. |
| `call(cpu, addr)` | 0.1 | Call a game function with the registers in `cpu`. |
| `hook(addr, fn, user)` | 0.1 | Put `fn` in front of a game function reached through a pointer. Returns an address that still runs the original. |
| `trampoline(fn, user)` | 0.1 | A game address that runs `fn`, for storing in a game table or callback field. |
| `reg`, `set_reg`, `freg`, `set_freg` | 0.1 | Read and write the integer (r0 to r31) and float (f0 to f31) registers of a call. |
| `guest_alloc(size)` | 0.1 | Zeroed, 32-byte-aligned game memory that is never freed. The pool is only a few KB in total. |

See [hooks and game functions](hooks.md).
