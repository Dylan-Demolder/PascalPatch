// unlock-all: every character and stage is selectable from the first boot (PascalPatch native plugin).
//
// Melee keeps the unlocked-character and unlocked-stage flags as bit fields in its save data in
// RAM (u16 at 0x8045BF28 and 0x8045BF2A, NTSC 1.02); the select screens read them from there.
// Setting every bit is what the well-known "unlock all characters" Gecko code does (02 write
// 0x07FF to 0x8045BF28), but flags that stay set are found by the menus, which celebrate each
// one with a notice and a trophy: a fresh save then boots into a long run of "You got ..."
// screens. So both fields are set only while a character or stage select is on screen, where
// they are read, and put back as they were once the match starts. Nothing is written to the
// memory card unless the game itself saves. Offline only, like every native plugin.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/plugin.h"

namespace {

constexpr uint32_t UNLOCKED_CHARACTERS = 0x8045BF28;   // NTSC 1.02, u16 bit field
constexpr uint16_t ALL_CHARACTERS = 0x07FF;              // Dr. Mario ... Young Link, Mr. Game & Watch
constexpr uint32_t UNLOCKED_STAGES = 0x8045BF2A;       // u16 bit field (NUM_UNLOCKABLE_STAGES bits)
constexpr uint16_t ALL_STAGES = 0x07FF;                  // the 11 unlockable stages
constexpr uint32_t GAME_MODE = 0x80479D30;             // routingInfo: curr_mode, ..., curr_state_id at +3
constexpr uint8_t STATE_CSS = 0, STATE_SSS = 1;

const pp_host* H = nullptr;

// One bit field that is ours while a select screen is up, and the game's own value otherwise.
struct Flags {
  uint32_t addr;
  uint16_t all;
  bool opened = false;
  uint16_t saved = 0;
  void set(bool want) {
    if (want && !opened) { saved = H->rd16(addr); opened = true; }
    if (want) { if ((H->rd16(addr) & all) != all) H->wr16(addr, H->rd16(addr) | all); }
    else if (opened) { H->wr16(addr, saved); opened = false; }
  }
};
Flags g_chars{UNLOCKED_CHARACTERS, ALL_CHARACTERS}, g_stage_flags{UNLOCKED_STAGES, ALL_STAGES};

// the modes whose minor scenes 0 and 1 are a character select and a stage select
bool selecting() {
  uint8_t mode = H->rd8(GAME_MODE), state = H->rd8(GAME_MODE + 3);
  bool match_mode = mode == 0x02 || mode == 0x03 || mode == 0x04 || mode == 0x05 || mode == 0x0F ||
                    (mode >= 0x10 && mode <= 0x13) || mode == 0x1B || mode == 0x1C;
  return match_mode && (state == STATE_CSS || state == STATE_SSS);
}

bool g_settings = false;     // the runtime serves settings (PascalPatch 0.2+)

bool on(const char* key) { return !g_settings || H->setting_number("unlock-all", key) != 0; }

void frame(void*) {
  bool sel = selecting();
  g_chars.set(sel && on("characters"));
  g_stage_flags.set(sel && on("stages"));
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char*) {
  if (!host || host->abi != PP_PLUGIN_ABI || !PP_HOST_HAS(host, guest_alloc)) return 1;
  H = host;
  g_settings = PP_HOST_HAS(host, set_status);
  if (g_settings) {   // shown on the plugin's F2 tab; turning one off takes effect from the next frame
    H->declare_setting("unlock-all", R"({"key":"characters","type":"bool","label":"Unlock every character","default":true})");
    H->declare_setting("unlock-all", R"({"key":"stages","type":"bool","label":"Unlock every stage","default":true})");
    H->set_status("unlock-all", "Every character and stage on the select screens (your save is untouched).");
  }
  H->on_frame(frame, nullptr);
  H->log("unlock-all", "all characters and stages unlocked (offline)");
  return 0;
}
