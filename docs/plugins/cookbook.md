# Cookbook

Short recipes for what plugins do most. Each recipe assumes the template's setup: `const pp_host* H`, `constexpr const char* ID`, and `#include "pascalpatch/melee.h"`.

## Run once when a match starts or ends

```cpp
bool g_in = false;
void frame(void*) {
  bool in = pp_in_match(H);
  if (in && !g_in) { /* match start: reset counters */ }
  if (!in && g_in) { /* match end: summarise, write a file */ }
  g_in = in;
  if (!in) return;
  ...
}
```

## React when a fighter's action state changes

```cpp
uint32_t g_last[4] = {~0u, ~0u, ~0u, ~0u};
int g_frames_in_state[4];

for (int port = 0; port < 4; ++port) {
  uint32_t fp = pp_fighter(H, port);
  if (!fp) { g_last[port] = ~0u; continue; }
  uint32_t s = H->rd32(fp + PP_FT_STATE);
  if (s != g_last[port]) {
    on_state_change(port, g_last[port], s);      // e.g. KneeBend -> EscapeAir: a wavedash began
    g_frames_in_state[port] = 0;
  }
  ++g_frames_in_state[port];
  g_last[port] = s;
}
```

Counting your own frames since the change is more reliable than `PP_FT_ANIM_FRAME` when you want "frames spent in this state".

## Detect a hit

```cpp
float g_pct[4];
...
float pct = H->rdf32(fp + PP_FT_PERCENT);
if (pct > g_pct[port] + 0.01f) {
  int attacker = H->rd32(fp + PP_FT_HIT_SOURCE);   // port of whoever hit it
  on_hit(port, attacker, pct - g_pct[port]);
}
g_pct[port] = pct;
```

Reset `g_pct` at match start and when a stock is lost, because the percent drops to 0 on respawn. Combo Counter groups hits into combos with `pp_state_is_punished`.

## Measure frame advantage

The frame on which each fighter can act again: attacker minus defender.

```cpp
bool actionable(uint32_t fp) {
  uint32_t s = H->rd32(fp + PP_FT_STATE);
  return pp_state_is_actionable(s) || pp_can_interrupt(H, fp);
}
```

Start counting when a hit connects (the defender enters hitlag). Note the frame each side first becomes actionable, and show the difference. See [frame_data.cpp](../../plugins/frame-data/native/frame_data.cpp) for the full version, with shield stun and landings.

## A toggle hotkey with a toast

```cpp
struct Key { const char* setting; bool was = false; };
bool pressed(Key& k) {
  int vk = (int)H->setting_number(ID, k.setting);
  bool down = vk && H->key_down(vk);
  bool edge = down && !k.was;
  k.was = down;
  return edge;
}

Key g_toggle{"toggle_key"};
bool g_on = true;
...
if (pressed(g_toggle)) { g_on = !g_on; H->toast(ID, g_on ? "Coach on" : "Coach off"); }
```

## Feedback that fades

```cpp
struct Flash { char text[64]; uint32_t rgb; int frames = 0; };
Flash g_flash;

void say(const char* text, uint32_t rgb) {
  std::snprintf(g_flash.text, sizeof g_flash.text, "%s", text);
  g_flash.rgb = rgb;
  g_flash.frames = 90;                                  // 1.5 seconds
}

void draw_flash(float x, float y) {
  if (g_flash.frames <= 0) return;
  float alpha = g_flash.frames < 20 ? g_flash.frames / 20.0f : 1.0f;   // fade over the last third
  H->hud_label(x, y, pp_rgba(g_flash.rgb, alpha), 18, 1, g_flash.text);
  --g_flash.frames;
}

say("L-cancel: 2 frames early", PP_RGB_WARN);
```

## A panel that shares its corner

```cpp
void panel(const char* title, const char* body, int port) {
  float w = 200, h = 42, x = 8, y = 8;
  if (PP_HOST_HAS(H, hud_place)) H->hud_place(PP_CORNER_TOP_RIGHT, w, h, &x, &y);
  else x = 640 - 8 - w;
  H->hud_rect(x, y, x + w, y + h, pp_rgba(0x14161C, 0.8f), 6, 1);
  H->hud_rect(x, y, x + 4, y + h, pp_rgba(pp_port_rgb[port], 1), 2, 1);
  H->hud_text(x + 12, y + 6, pp_rgba(0x9AA3B5, 1), 11, title);
  H->hud_text(x + 12, y + 20, pp_rgba(0xF2F4FA, 1), 15, body);
}
```

## Text above a fighter

Project the fighter's position with `pascalpatch/camera.h` (see [drawing on screen](hud.md#world-positions-on-screen)) and draw a label a little above it:

```cpp
pp_camera cam;
if (pp_camera_read(H, &cam)) {
  float x = H->rdf32(fp + PP_FT_POS), y = H->rdf32(fp + PP_FT_POS + 4), z = H->rdf32(fp + PP_FT_POS + 8);
  float sx, sy;
  if (pp_project(&cam, x, y + 18, z, &sx, &sy, nullptr)) H->hud_label(sx, sy, pp_rgba(PP_RGB_TEXT, 1), 14, 1, "Tech!");
}
```

## A choice setting with a `switch`

```json
{"key": "dummy", "type": "choice", "label": "Dummy", "default": "stand",
 "options": [{"value": "stand", "label": "Stand"}, {"value": "shield", "label": "Shield"}, {"value": "jump", "label": "Jump"}]}
```

```cpp
switch ((int)H->setting_number(ID, "dummy")) {    // the index into options
  case 0: /* stand */ break;
  case 1: /* shield */ break;
  case 2: /* jump */ break;
}
```

Use `setting_text` instead when you compare by value. That way, adding an option in the middle of the list later does not shift the indices.

## Hold a dummy's shield

```cpp
void dummy_shield(int port) {
  pp_pad_state s{};
  s.buttons = PP_BTN_R;
  s.trigger_r = 1.0f;
  H->pad_set(port, &s);
}
// when the option is switched off, or the match ends:
H->pad_release(port);
```

## Save a file beside the DLL

Some plugins keep a history, such as Match Stats' past matches. Find your DLL's folder once at load time, and write only at match end:

```cpp
#include <windows.h>
std::string g_dir;

void find_dir() {
  HMODULE self = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCSTR)&find_dir, &self);
  char path[MAX_PATH];
  GetModuleFileNameA(self, path, MAX_PATH);
  g_dir = path;
  g_dir.resize(g_dir.find_last_of("\\/") + 1);
}
```

Say in your README what the file is and where it goes. Never write outside your plugin's folder.

## Watch a raw value while developing

```cpp
char line[128];
std::snprintf(line, sizeof line, "state %X  frame %.0f  jumps %u", s, H->rdf32(fp + PP_FT_ANIM_FRAME), H->rd8(fp + PP_FT_JUMPS_USED));
H->set_status(ID, line);     // live on the plugin's F2 tab
```

## Find the stage and its blast zones

```cpp
float left, right, top, bottom;
if (pp_stage_blast_zones(H, &left, &right, &top, &bottom)) { /* world units */ }
float edge = pp_stage_floor_edge(pp_stage_kind(H));   // half the main floor's width; 0 off the tournament stages
```

DI Trainer uses these with the launched fighter's attributes (`PP_FT_GRAVITY`, `PP_FT_FALL_SPEED`) to replay the game's own knockback physics and predict whether a launch kills. See [di_trainer.cpp](../../plugins/di-trainer/native/di_trainer.cpp).
