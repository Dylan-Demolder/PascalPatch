// quick-match: boot straight into a match, skipping the title screen and both select screens
// (PascalPatch native plugin).
//
// Melee still carries its developers' "debug VS" game mode (GM_DEBUG_VS, 0x0E): a VS match whose
// fighters and stage are filled in by code (onEnterDebugVs) instead of the select screens, then
// the results screen. Boot leaves for the intro movie or the title screen in bootOnLeave; standing
// behind it, this plugin changes the pending game mode to debug VS (as the well-known "boot to
// CSS" Gecko code changes it to VS), and standing behind onEnterDebugVs it writes the match from
// the plugin's settings. Both are reached through the game-mode state tables, so hooks see them.
// Nothing is saved to the memory card; offline only, like every native plugin.
//
// quick-match.json beside the DLL (optional, written by PascalPatch for Character Studio's
// "Test in game") overrides the settings with its "match": {"match": {"p1": "fox", "p2": "marth",
// "p2_player": "human", "stage": "fd", "rules": "endless"}}; p1/p2 may also be a character
// number (CKind 0-25).
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace {

constexpr const char* ID = "quick-match";

// NTSC 1.02
constexpr uint32_t BOOT_ON_LEAVE = 0x801BF9A8;        // gmboot.c bootOnLeave
constexpr uint32_t DEBUG_VS_ON_ENTER = 0x801B13B8;    // gmvsmode.c onEnterDebugVs
constexpr uint32_t SCENE_END = 0x80479D64;            // gm_80479D58.unk_C: 1 ends the scene loop
constexpr uint8_t STATE_DEBUG_VS = 1;   // debug VS's minor scene for the match

// StartMeleeData (0xF0 bytes): StartMeleeRules (0x60), then six PlayerInitData of 0x24
constexpr uint32_t RULES_STKIND = 0x0E, RULES_TIME_LIMIT = 0x10, RULES_ITEM_FREQ = 0x0B;
constexpr uint32_t PLAYERS = 0x60, PLAYER_SIZE = 0x24;
constexpr uint8_t PKIND_HUMAN = 0, PKIND_CPU = 1, PKIND_NONE = 3;

const pp_host* H = nullptr;
uint32_t g_boot_leave = 0, g_debug_enter = 0;
bool g_settings = false;

struct Fighter { const char* value; const char* label; };
constexpr Fighter FIGHTERS[] = {   // CSS order of CharacterKind (CKind 0..25)
    {"falcon", "Captain Falcon"}, {"dk", "Donkey Kong"}, {"fox", "Fox"}, {"gnw", "Mr. Game & Watch"},
    {"kirby", "Kirby"}, {"bowser", "Bowser"}, {"link", "Link"}, {"luigi", "Luigi"}, {"mario", "Mario"},
    {"marth", "Marth"}, {"mewtwo", "Mewtwo"}, {"ness", "Ness"}, {"peach", "Peach"}, {"pikachu", "Pikachu"},
    {"ics", "Ice Climbers"}, {"jigglypuff", "Jigglypuff"}, {"samus", "Samus"}, {"yoshi", "Yoshi"},
    {"zelda", "Zelda"}, {"sheik", "Sheik"}, {"falco", "Falco"}, {"ylink", "Young Link"},
    {"doc", "Dr. Mario"}, {"roy", "Roy"}, {"pichu", "Pichu"}, {"ganondorf", "Ganondorf"},
};
constexpr int FIGHTER_COUNT = sizeof(FIGHTERS) / sizeof(FIGHTERS[0]);

struct Stage { const char* value; const char* label; uint16_t stkind; };
constexpr Stage STAGES[] = {   // StartMeleeRules::stkind is a St_Kind
    {"fd", "Final Destination", 0x20}, {"bf", "Battlefield", 0x1F}, {"ys", "Yoshi's Story", 0x08},
    {"fod", "Fountain of Dreams", 0x02}, {"ps", "Pokemon Stadium", 0x03}, {"dl", "Dream Land N64", 0x1C},
};

// Match setup: the settings, or quick-match.json when PascalPatch wrote one.
struct Setup {
  bool boot = true;
  int p1 = 2, p2 = 20;        // Fox, Falco
  std::string p2_player = "human", stage = "fd", rules = "endless";
};
Setup g_file;
bool g_has_file = false;

int fighter_index(const std::string& v, int fallback) {
  for (int i = 0; i < FIGHTER_COUNT; ++i) if (v == FIGHTERS[i].value) return i;
  return fallback;
}

std::string text(const char* key, const char* fallback) {
  if (!g_settings) return fallback;
  const char* v = H->setting_text(ID, key);
  return v && *v ? v : fallback;
}

Setup current() {
  if (g_has_file) return g_file;
  Setup s;
  s.boot = text("start", "match") == "match";
  s.p1 = fighter_index(text("p1", "fox"), 2);
  s.p2 = fighter_index(text("p2", "falco"), 20);
  s.p2_player = text("p2_player", "human");
  s.stage = text("stage", "fd");
  s.rules = text("rules", "endless");
  return s;
}

void read_file(const char* config_path) {
  if (!config_path) return;
  std::ifstream in(config_path);
  if (!in) return;
  nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
  if (!j.is_object() || j.find("match") == j.end()) return;   // a plain config: the settings rule
  j = j["match"];
  if (!j.is_object()) { H->log(ID, "quick-match.json: \"match\" is not an object; using the settings"); return; }
  auto fighter = [&](const char* key, int fallback) {
    if (j.find(key) == j.end()) return fallback;
    if (j[key].is_number_integer()) { int v = j[key].get<int>(); return v >= 0 && v < FIGHTER_COUNT ? v : fallback; }
    return j[key].is_string() ? fighter_index(j[key].get<std::string>(), fallback) : fallback;
  };
  auto str = [&](const char* key, const std::string& fallback) {
    return j.find(key) != j.end() && j[key].is_string() ? j[key].get<std::string>() : fallback;
  };
  g_file.p1 = fighter("p1", g_file.p1);
  g_file.p2 = fighter("p2", g_file.p2);
  g_file.p2_player = str("p2_player", g_file.p2_player);
  g_file.stage = str("stage", g_file.stage);
  g_file.rules = str("rules", g_file.rules);
  g_has_file = true;
}

// Boot is leaving: go to debug VS instead of the intro movie or the title screen.
void boot_on_leave(pp_cpu* cpu, void*) {
  H->call(cpu, g_boot_leave);
  if (!current().boot) return;
  H->wr8(PP_SCENE + PP_SCENE_PENDING, PP_SCENE_DEBUG_VS);   // the mode change is already pending
  H->log(ID, "boot: straight into a match");
}

void write_player(uint32_t p, int ckind, uint8_t kind, uint8_t cpu_level, uint8_t color, int port, int8_t stocks) {
  H->wr8(p + 0x00, (uint8_t)ckind);
  H->wr8(p + 0x01, kind);
  H->wr8(p + 0x02, (uint8_t)stocks);
  H->wr8(p + 0x03, color);
  H->wr8(p + 0x04, (uint8_t)port);   // slot: the controller port
  H->wr8(p + 0x0F, cpu_level);
}

// onEnterDebugVs has filled in its test match (Link vs Mario, time, no items): make it ours.
void debug_vs_on_enter(pp_cpu* cpu, void*) {
  uint32_t state = H->reg(cpu, 3);
  H->call(cpu, g_debug_enter);
  uint32_t start = H->rd32(state + 0x10);   // GameModeState::info.enter_data (gmVsMelee_StartData)
  if (!start) return;
  Setup s = current();

  uint16_t stkind = STAGES[0].stkind;
  for (const Stage& st : STAGES) if (s.stage == st.value) stkind = st.stkind;
  H->wr16(start + RULES_STKIND, stkind);
  H->wr8(start + RULES_ITEM_FREQ, 0xFF);   // no items

  // byte 0: match_kind (3 bits), x0_3 (3), timer_enabled, timer_counts_up; byte 2 bit 7: is_stock
  uint8_t b0 = H->rd8(start) & 0x1C;
  int8_t stocks = 0;
  if (s.rules == "stock4" || s.rules == "stock1") {
    stocks = s.rules == "stock4" ? 4 : 1;
    b0 |= (1 << 5) | 0x02;                       // stock, timer on
    H->wr32(start + RULES_TIME_LIMIT, 8 * 60);   // 8 minutes
    H->wr8(start + 2, H->rd8(start + 2) | 0x80);
  } else {
    b0 |= 0x01;                                  // time match, no limit, the clock counts up
    H->wr32(start + RULES_TIME_LIMIT, 0);
  }
  H->wr8(start, b0);

  write_player(start + PLAYERS, s.p1, PKIND_HUMAN, 0, 0, 0, stocks);
  uint8_t kind = PKIND_HUMAN, level = 0;
  if (s.p2_player == "none") kind = PKIND_NONE;
  else if (s.p2_player.rfind("cpu", 0) == 0) { kind = PKIND_CPU; level = (uint8_t)std::atoi(s.p2_player.c_str() + 3); if (level < 1 || level > 9) level = 9; }
  write_player(start + PLAYERS + PLAYER_SIZE, s.p2, kind, level, s.p2 == s.p1 ? 1 : 0, 1, stocks);
  for (int i = 2; i < 6; ++i) H->wr8(start + PLAYERS + i * PLAYER_SIZE + 1, PKIND_NONE);

  char line[160];
  std::snprintf(line, sizeof line, "match: %s vs %s (%s) on stage %u, %s", FIGHTERS[s.p1].label, FIGHTERS[s.p2].label,
                s.p2_player.c_str(), stkind, s.rules.c_str());
  H->log(ID, line);
}

// The restart key ends the match's scene and sends debug VS back to its match state (id 1)
// instead of on to the results: a fresh match, fighters at their spawn points.
bool g_restart_was = false;
void frame(void*) {
  if (!g_settings || !PP_HOST_HAS(H, key_down)) return;
  int vk = (int)H->setting_number(ID, "restart_key");
  bool down = vk && H->key_down(vk);
  if (down && !g_restart_was && pp_scene_major(H) == PP_SCENE_DEBUG_VS && pp_scene_minor(H) == STATE_DEBUG_VS) {
    H->wr8(PP_SCENE + PP_SCENE_NEXT, STATE_DEBUG_VS + 1);   // replay the match scene
    H->wr32(SCENE_END, 1);
  }
  g_restart_was = down;
}

std::string options(bool fighters) {
  std::string o = "[";
  if (fighters) {
    for (int i = 0; i < FIGHTER_COUNT; ++i)
      o += std::string(i ? "," : "") + R"({"value":")" + FIGHTERS[i].value + R"(","label":")" + FIGHTERS[i].label + "\"}";
  } else {
    bool first = true;
    for (const Stage& st : STAGES) { o += std::string(first ? "" : ",") + R"({"value":")" + st.value + R"(","label":")" + st.label + "\"}"; first = false; }
  }
  return o + "]";
}

void declare() {
  H->declare_setting(ID, R"J({"key":"start","type":"choice","label":"When the game starts","default":"match","group":"Start","options":[{"value":"match","label":"Go straight into a match"},{"value":"menus","label":"Show the title screen, as usual"}],"help":"Takes effect the next time the game starts."})J");
  H->declare_setting(ID, (R"({"key":"p1","type":"choice","label":"Player 1","default":"fox","group":"Match","options":)" + options(true) + "}").c_str());
  H->declare_setting(ID, (R"({"key":"p2","type":"choice","label":"Player 2","default":"falco","group":"Match","options":)" + options(true) + "}").c_str());
  H->declare_setting(ID, R"J({"key":"p2_player","type":"choice","label":"Player 2 is","default":"human","group":"Match","options":[{"value":"human","label":"A human player (Training Lab's dummy can play it)"},{"value":"cpu1","label":"CPU level 1"},{"value":"cpu3","label":"CPU level 3"},{"value":"cpu5","label":"CPU level 5"},{"value":"cpu7","label":"CPU level 7"},{"value":"cpu9","label":"CPU level 9"},{"value":"none","label":"Nobody (player 1 alone)"}]})J");
  H->declare_setting(ID, (R"({"key":"stage","type":"choice","label":"Stage","default":"fd","group":"Match","options":)" + options(false) + "}").c_str());
  H->declare_setting(ID, R"J({"key":"rules","type":"choice","label":"Rules","default":"endless","group":"Match","options":[{"value":"endless","label":"Endless: no time limit, no stocks (practice)"},{"value":"stock4","label":"4 stocks, 8 minutes"},{"value":"stock1","label":"1 stock, 8 minutes"}]})J");
  H->declare_setting(ID, R"J({"key":"restart_key","type":"key","label":"Restart the match","default":"Backspace","group":"Match"})J");
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char* config_path) {
  if (!host || host->abi != PP_PLUGIN_ABI) return 1;
  H = host;
  g_settings = PP_HOST_HAS(host, set_status);
  if (g_settings) declare();
  read_file(config_path);
  g_boot_leave = H->hook(BOOT_ON_LEAVE, boot_on_leave, nullptr);
  g_debug_enter = H->hook(DEBUG_VS_ON_ENTER, debug_vs_on_enter, nullptr);
  if (!g_boot_leave || !g_debug_enter) { H->log(ID, "could not hook the boot and debug VS scenes"); return 1; }
  H->on_frame(frame, nullptr);
  if (g_settings) {
    Setup s = current();
    char line[160];
    std::snprintf(line, sizeof line, g_has_file ? "Set by PascalPatch: %s vs %s. Changes here apply when it is removed from the profile."
                                                : "%s vs %s. Changes apply from the next match.", FIGHTERS[s.p1].label, FIGHTERS[s.p2].label);
    H->set_status(ID, line);
  }
  return 0;
}
