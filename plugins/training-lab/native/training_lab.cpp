// training-lab: practice tools for any match (PascalPatch native plugin).
//
//   Pause / frame advance   hold the game on a frame and step it one frame at a time
//   Slow motion             half or quarter speed, toggled with a key
//   Percent                 reset to 0% with a key, or lock a player at a set percent
//   Infinite shield         shields never shrink or break
//   Endless stocks          nobody runs out of stocks, so a practice match never ends
//
// Pause and slow motion hold the simulation thread inside the frame callback, which the port
// tolerates: its pacer resumes at 60 Hz after any stall. Only the game stops; the window, the
// F2 overlay and the other plugins' HUD stay live. Writes game memory (percent, shield, stocks)
// only while those options are on. Needs PascalPatch 0.3 (hotkeys and toasts).
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <windows.h>

#include <cstdio>

namespace {

constexpr const char* ID = "training-lab";
constexpr uint32_t PL_DAMAGE = 0x60;   // player block: u16 damage the HUD counts toward

const pp_host* H = nullptr;

struct Key {
  const char* setting;
  bool was = false;
  bool pressed() {   // a fresh press this frame
    int vk = (int)H->setting_number(ID, setting);
    bool down = vk > 0 && H->key_down(vk);
    bool edge = down && !was;
    was = down;
    return edge;
  }
};

Key k_pause{"pause_key"}, k_step{"step_key"}, k_slow{"slow_key"}, k_reset{"reset_key"};
bool paused = false, slow = false;
bool shown = false;   // the PAUSED badge has been on screen for a frame (the HUD is published when a frame ends)

bool on(const char* k) { return H->setting_number(ID, k) > 0.5; }

bool port_selected(int port) {
  int who = (int)H->setting_number(ID, "who");   // 0 P1, 1 P2, 2 everyone, 3 CPUs only
  if (who == 2) return true;
  if (who == 3) return pp_player_type(H, port) == 1;
  return port == who;
}

void set_percent(uint32_t fp, int port, float pct) {
  H->wrf32(fp + PP_FT_PERCENT, pct);
  H->wr16(pp_player_block(port) + PL_DAMAGE, (uint16_t)(pct + 0.5f));
}

void badge(const char* text, uint32_t rgb) {
  H->hud_rect(262, 322, 378, 344, pp_rgba(0x111838, 0.8f), 6, 1);
  H->hud_label(320, 325, pp_rgba(rgb, 1.0f), 16, 1, text);
}

// Holds the game here while paused. Returns when the player resumes or steps one frame.
void hold() {
  for (;;) {
    if (k_pause.pressed()) { paused = false; H->toast(ID, "Resumed"); return; }
    if (k_step.pressed()) return;                       // run exactly one frame, stay paused
    if (!pp_in_match(H)) { paused = false; return; }
    Sleep(4);
  }
}

void frame(void*) {
  if (!pp_in_match(H)) { paused = false; return; }

  if (k_pause.pressed()) {
    paused = !paused;
    shown = false;
    H->toast(ID, paused ? "Paused: step with the frame advance key" : "Resumed");
  }
  if (k_slow.pressed()) {
    slow = !slow;
    H->toast(ID, slow ? (H->setting_number(ID, "slow_speed") > 0.5 ? "Quarter speed" : "Half speed") : "Full speed");
  }
  if (!paused && k_step.pressed()) { paused = true; shown = false; H->toast(ID, "Paused: step with the frame advance key"); }
  bool reset = k_reset.pressed();

  for (int i = 0; i < 4; ++i) {
    uint32_t fp = pp_fighter(H, i);
    if (!fp || !port_selected(i)) continue;
    if (reset) set_percent(fp, i, 0);
    if (on("lock_percent")) set_percent(fp, i, (float)H->setting_number(ID, "percent"));
    if (on("infinite_shield")) H->wrf32(fp + PP_FT_SHIELD, 60.0f);
    if (on("endless_stocks")) {
      uint32_t b = pp_player_block(i);
      if ((int8_t)H->rd8(b + PP_PL_STOCKS) < 2) H->wr8(b + PP_PL_STOCKS, 4);
    }
  }
  if (reset) H->toast(ID, "Percent reset");

  if (paused) {
    badge("PAUSED", 0xF2C200);
    // let the first paused frame finish so the badge reaches the screen, then hold
    if (shown) hold();
    shown = true;
  } else if (slow) {
    badge(H->setting_number(ID, "slow_speed") > 0.5 ? "1/4 SPEED" : "1/2 SPEED", 0x9FB4FF);
    // one extra frame's time (or three) per frame: the pacer then runs at a half (quarter) rate
    Sleep(H->setting_number(ID, "slow_speed") > 0.5 ? 50 : 17);
  }
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char*) {
  if (!host || host->abi != PP_PLUGIN_ABI) return 1;
  if (!PP_HOST_HAS(host, hud_label)) {
    if (host->log) host->log(ID, "needs PascalPatch 0.3 or newer");
    return 2;
  }
  H = host;
  H->declare_setting(ID, R"({"key":"pause_key","type":"key","label":"Pause / resume","default":"F5"})");
  H->declare_setting(ID, R"({"key":"step_key","type":"key","label":"Frame advance","default":"F6"})");
  H->declare_setting(ID, R"({"key":"slow_key","type":"key","label":"Slow motion on / off","default":"F7"})");
  H->declare_setting(ID, R"({"key":"slow_speed","type":"choice","label":"Slow motion speed","default":"half","options":[{"value":"half","label":"Half speed"},{"value":"quarter","label":"Quarter speed"}]})");
  H->declare_setting(ID, R"({"key":"who","type":"choice","label":"Percent, shield and stock options apply to","default":"p2","options":[{"value":"p1","label":"Port 1"},{"value":"p2","label":"Port 2"},{"value":"all","label":"Everyone"},{"value":"cpu","label":"CPU players"}]})");
  H->declare_setting(ID, R"({"key":"reset_key","type":"key","label":"Reset percent to 0","default":"F9"})");
  H->declare_setting(ID, R"({"key":"lock_percent","type":"bool","label":"Lock percent","default":false})");
  H->declare_setting(ID, R"({"key":"percent","type":"int","label":"Locked percent","default":60,"min":0,"max":999})");
  H->declare_setting(ID, R"({"key":"infinite_shield","type":"bool","label":"Infinite shield","default":false})");
  H->declare_setting(ID, R"({"key":"endless_stocks","type":"bool","label":"Endless stocks","default":false})");
  H->set_status(ID, "F5 pause, F6 frame advance, F7 slow motion, F9 reset percent.");
  H->on_frame(frame, nullptr);
  return 0;
}
