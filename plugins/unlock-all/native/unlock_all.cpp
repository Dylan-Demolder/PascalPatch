// unlock-all: every character and stage is selectable from the first boot (PascalPatch native plugin).
//
// Melee keeps the unlocked-character flags as a bit field in its save data in RAM; the
// character select screen reads them from there. Setting all eleven unlockable bits every
// frame has the same effect as the well-known "unlock all characters" Gecko code (02 write
// 0x07FF to 0x8045BF28), without touching the memory card: nothing is written to the save
// unless the game itself saves.
//
// Stages are opened differently: when the game finds new unlocked-stage flags (u16 at
// 0x8045BF2A, one bit per unlockable stage: Battlefield, Final Destination, the past stages,
// ...) it celebrates them with notices and unlocks more (Random Stage Select, Sound Test, a
// trophy). So the flags are set only while VS mode shows its character or stage select, which
// is where they are read, and put back as they were before the match starts. Offline only,
// like every native plugin.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/plugin.h"

namespace {

constexpr uint32_t UNLOCKED_CHARACTERS = 0x8045BF28;   // NTSC 1.02, u16 bit field
constexpr uint16_t ALL_CHARACTERS = 0x07FF;              // Dr. Mario ... Young Link, Mr. Game & Watch
constexpr uint32_t UNLOCKED_STAGES = 0x8045BF2A;       // u16 bit field (NUM_UNLOCKABLE_STAGES bits)
constexpr uint16_t ALL_STAGES = 0x07FF;                  // the 11 unlockable stages
constexpr uint32_t GAME_MODE = 0x80479D30;             // routingInfo: curr_mode, ..., curr_state_id at +3
constexpr uint8_t MODE_VS = 2, STATE_CSS = 0, STATE_SSS = 1;

const pp_host* H = nullptr;
bool g_opened = false;       // the stage flags are ours right now
uint16_t g_stages = 0;       // the game's own, to put back

bool g_settings = false;     // the runtime serves settings (PascalPatch 0.2+)

bool on(const char* key) { return !g_settings || H->setting_number("unlock-all", key) != 0; }

void frame(void*) {
  uint16_t flags = H->rd16(UNLOCKED_CHARACTERS);
  if (on("characters") && (flags & ALL_CHARACTERS) != ALL_CHARACTERS) H->wr16(UNLOCKED_CHARACTERS, flags | ALL_CHARACTERS);
  uint8_t state = H->rd8(GAME_MODE + 3);
  bool selecting = on("stages") && H->rd8(GAME_MODE) == MODE_VS && (state == STATE_CSS || state == STATE_SSS);
  if (selecting && !g_opened) {
    g_stages = H->rd16(UNLOCKED_STAGES); g_opened = true;
    H->wr16(UNLOCKED_STAGES, g_stages | ALL_STAGES);
  } else if (!selecting && g_opened) {
    H->wr16(UNLOCKED_STAGES, g_stages); g_opened = false;
  }
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char*) {
  if (!host || host->abi != PP_PLUGIN_ABI || !PP_HOST_HAS(host, guest_alloc)) return 1;
  H = host;
  g_settings = PP_HOST_HAS(host, set_status);
  if (g_settings) {   // shown on the plugin's F2 tab; turning one off takes effect from the next frame
    H->declare_setting("unlock-all", R"({"key":"characters","type":"bool","label":"Unlock every character","default":true})");
    H->declare_setting("unlock-all", R"({"key":"stages","type":"bool","label":"Unlock every stage on the select screens","default":true,"help":"Only while choosing, so the game never shows unlock notices."})");
    H->set_status("unlock-all", "Characters and stages open (offline).");
  }
  H->on_frame(frame, nullptr);
  H->log("unlock-all", "all characters and stages unlocked (offline)");
  return 0;
}
