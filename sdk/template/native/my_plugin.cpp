// my-plugin: a starting point for PascalPatch plugins.
//
// Shows port 1's current move and the frame of its animation in a small panel, with a hotkey to
// hide it and an opacity setting. Rename it, then replace the panel with your own idea.
// Guide: https://github.com/Dylan-Demolder/PascalPatch/tree/master/docs/plugins
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"
#include "pascalpatch/plugin.h"

#include <cstdio>

namespace {

constexpr const char* ID = "my-plugin";   // must match "id" in plugin.json

const pp_host* H = nullptr;
bool g_visible = true;
bool g_key_was_down = false;

// A hotkey press (not hold): true on the first frame the key is down.
bool pressed(const char* setting) {
  int vk = (int)H->setting_number(ID, setting);
  bool down = vk != 0 && H->key_down(vk);
  bool edge = down && !g_key_was_down;
  g_key_was_down = down;
  return edge;
}

void frame(void*) {
  if (pressed("toggle_key")) {
    g_visible = !g_visible;
    H->toast(ID, g_visible ? "My Plugin shown" : "My Plugin hidden");
  }
  if (!g_visible || !pp_in_match(H)) return;   // fighters only exist during a match

  uint32_t fp = pp_fighter(H, 0);              // port 1's fighter, or 0
  if (!fp) return;
  uint32_t kind = H->rd32(fp + PP_FT_KIND), state = H->rd32(fp + PP_FT_STATE);
  float anim_frame = H->rdf32(fp + PP_FT_ANIM_FRAME);

  char line[96];
  std::snprintf(line, sizeof line, "%s  frame %d", pp_move_name(kind, state), (int)anim_frame + 1);

  float op = (float)H->setting_number(ID, "opacity");
  float w = 190, h = 44, x = 8, y = 8;
  H->hud_place(PP_CORNER_TOP_LEFT, w, h, &x, &y);   // shares the corner with other plugins' panels
  H->hud_rect(x, y, x + w, y + h, pp_rgba(0x14161C, 0.8f * op), 6, 1);
  H->hud_rect(x, y, x + 4, y + h, pp_rgba(pp_port_rgb[0], op), 2, 1);   // port colour bar
  H->hud_text(x + 12, y + 6, pp_rgba(0x9AA3B5, op), 11, pp_kind_name(kind));
  H->hud_text(x + 12, y + 21, pp_rgba(0xF2F4FA, op), 15, line);
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char* /*config_path*/) {
  if (!host || host->abi != PP_PLUGIN_ABI) return 1;
  if (!PP_HOST_HAS(host, hud_place)) {   // the newest call used below (PascalPatch 0.5)
    host->log(ID, "needs PascalPatch 0.5 or newer");
    return 1;
  }
  H = host;
  // Settings come from plugin.json when installed; declaring them too makes a loose DLL work.
  H->declare_setting(ID, R"({"key":"toggle_key","type":"key","label":"Show / hide","default":"Numpad0"})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.9,"min":0.2,"max":1.0})");
  H->set_status(ID, "Shows port 1's move and frame. Press the hotkey to hide it.");
  H->on_frame(frame, nullptr);
  H->log(ID, "loaded");
  return 0;
}
