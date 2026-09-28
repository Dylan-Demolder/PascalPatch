// match-stats: the numbers behind a match, Slippi-style, without Slippi (PascalPatch native
// plugin, read-only).
//
// Tracks, for every player: openings (punishes started), kills, damage dealt, damage per opening,
// openings per kill, neutral wins (openings started while nobody was being punished), the
// L-cancel rate and inputs per minute. A card with the totals shows when the match ends (on the
// results screen), and a key shows it during the match. Each finished match is appended as one
// JSON line to match-stats.jsonl beside the plugin, for tools that chart progress over time.
// Needs PascalPatch 0.3.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <windows.h>
#include <intrin.h>

#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

constexpr const char* ID = "match-stats";
constexpr int RESET = 45;          // frames free before a punish ends (Slippi's rule)
constexpr int CARD_FRAMES = 60 * 20;

const pp_host* H = nullptr;

struct Stats {
  bool present = false;
  uint32_t kind = 0;
  int openings = 0, neutral_wins = 0, kills = 0, lc_ok = 0, lc_all = 0, inputs = 0;
  float damage = 0;           // dealt
  int stocks_left = 0;
};

struct Track {                 // per victim
  uint32_t fp = 0, state = 0;
  float pct = 0;
  int stocks = 0;
  bool punished = false;       // a punish on this fighter is running
  int by = -1, free = 0;
  uint32_t held = 0;
};

Stats st[4];
Track tr[4];
bool in_match = false;
unsigned frames = 0;
int card = 0;                  // frames the end-of-match card has left
char dll_dir[MAX_PATH] = "";

int attacker_of(int d) {
  int src = (int)H->rd32(tr[d].fp + PP_FT_HIT_SOURCE);
  if (src >= 0 && src < 4 && src != d && tr[src].fp) return src;
  int only = -1, n = 0;
  for (int i = 0; i < 4; ++i) if (i != d && tr[i].fp) { only = i; ++n; }
  return n == 1 ? only : -1;
}

void begin() {
  for (auto& s : st) s = Stats{};
  for (auto& t : tr) t = Track{};
  frames = 0;
  card = 0;
}

void save() {
  if (!dll_dir[0]) return;
  char path[MAX_PATH + 32];
  std::snprintf(path, sizeof path, "%s\\match-stats.jsonl", dll_dir);
  FILE* f = nullptr;
  if (fopen_s(&f, path, "a") != 0 || !f) return;
  std::fprintf(f, "{\"time\":%lld,\"frames\":%u,\"players\":[", (long long)std::time(nullptr), frames);
  bool first = true;
  for (int i = 0; i < 4; ++i) {
    const Stats& s = st[i];
    if (!s.present) continue;
    std::fprintf(f, "%s{\"port\":%d,\"character\":\"%s\",\"stocks_left\":%d,\"kills\":%d,\"openings\":%d,\"neutral_wins\":%d,"
                    "\"damage\":%.1f,\"lcancel\":[%d,%d],\"inputs\":%d}",
                 first ? "" : ",", i + 1, pp_kind_name(s.kind), s.stocks_left, s.kills, s.openings, s.neutral_wins, s.damage,
                 s.lc_ok, s.lc_all, s.inputs);
    first = false;
  }
  std::fprintf(f, "]}\n");
  std::fclose(f);
}

void update() {
  ++frames;
  bool any_punished = false;
  for (auto& t : tr) any_punished |= t.punished;
  for (int d = 0; d < 4; ++d) {
    Track& t = tr[d];
    uint32_t fp = pp_fighter(H, d);
    if (!fp) { t.fp = 0; continue; }
    Stats& s = st[d];
    s.present = true;
    s.kind = H->rd32(fp + PP_FT_KIND);
    float pct = H->rdf32(fp + PP_FT_PERCENT);
    int stocks = (int)(int8_t)H->rd8(pp_player_block(d) + PP_PL_STOCKS);
    uint32_t state = H->rd32(fp + PP_FT_STATE);
    s.stocks_left = stocks;
    if (fp != t.fp) { t = Track{}; t.fp = fp; t.pct = pct; t.stocks = stocks; t.state = state; continue; }

    // inputs: every newly pressed button (A B X Y Z L R, start excluded)
    uint32_t held = H->rd32(fp + PP_FT_HELD) & 0xF70;
    s.inputs += __popcnt(held & ~t.held);
    t.held = held;

    // L-cancels
    if (state != t.state && pp_state_is_aerial_landing(state)) {
      ++s.lc_all;
      if (H->rd8(fp + PP_FT_SINCE_LR) < 7) ++s.lc_ok;
    }

    // punishes
    if (stocks < t.stocks) {           // lost a stock: the punisher (or the last one) gets the kill
      int k = t.punished ? t.by : attacker_of(d);
      if (k >= 0) ++st[k].kills;
      t.punished = false;
    }
    if (pct > t.pct + 0.01f) {
      int a = attacker_of(d);
      if (a >= 0) {
        st[a].damage += pct - t.pct;
        if (!t.punished || t.by != a) {
          ++st[a].openings;
          if (!any_punished) ++st[a].neutral_wins;
          t.punished = true;
          t.by = a;
        }
        t.free = 0;
      }
    }
    if (t.punished) {
      if (pp_state_is_punished(state) || pp_hitlag(H, fp) > 0) t.free = 0;
      else if (++t.free > RESET) t.punished = false;
    }
    t.pct = pct;
    t.stocks = stocks;
    t.state = state;
  }
}

void draw_card(float alpha) {
  int n = 0;
  for (auto& s : st) n += s.present;
  if (!n) return;
  const float col = 118, x0 = 320 - (col * (float)n + 120) / 2, y0 = 120;
  const char* rows[] = {"Kills", "Openings", "Neutral wins", "Damage", "Damage / opening", "Openings / kill", "L-cancels", "Inputs / min"};
  const int R = 8;
  float h = 44 + 18.0f * R;
  H->hud_rect(x0, y0, x0 + 120 + col * (float)n, y0 + h, pp_rgba(0x0B1030, 0.88f * alpha), 8, 1);
  H->hud_label(x0 + 12, y0 + 8, pp_rgba(0x9FB4FF, alpha), 15, 0, "Match stats");
  for (int r = 0; r < R; ++r)
    H->hud_text(x0 + 12, y0 + 36 + 18.0f * (float)r, pp_rgba(0xB8C0E0, alpha), 13, rows[r]);
  int c = 0;
  float minutes = (float)frames / 3600.0f;
  for (int i = 0; i < 4; ++i) {
    const Stats& s = st[i];
    if (!s.present) continue;
    float x = x0 + 120 + col * (float)c + col / 2;
    char head[32];
    std::snprintf(head, sizeof head, "P%d %s", i + 1, pp_kind_name(s.kind));
    H->hud_label(x, y0 + 12, pp_rgba(pp_port_rgb[i], alpha), 13, 1, head);
    char v[R][24];
    std::snprintf(v[0], 24, "%d", s.kills);
    std::snprintf(v[1], 24, "%d", s.openings);
    std::snprintf(v[2], 24, "%d", s.neutral_wins);
    std::snprintf(v[3], 24, "%.0f%%", s.damage);
    if (s.openings) std::snprintf(v[4], 24, "%.1f%%", s.damage / (float)s.openings); else std::snprintf(v[4], 24, "-");
    if (s.kills) std::snprintf(v[5], 24, "%.1f", (float)s.openings / (float)s.kills); else std::snprintf(v[5], 24, "-");
    if (s.lc_all) std::snprintf(v[6], 24, "%d/%d  %d%%", s.lc_ok, s.lc_all, (100 * s.lc_ok + s.lc_all / 2) / s.lc_all); else std::snprintf(v[6], 24, "-");
    if (minutes > 0.05f) std::snprintf(v[7], 24, "%.0f", (float)s.inputs / minutes); else std::snprintf(v[7], 24, "-");
    for (int r = 0; r < R; ++r) H->hud_label(x, y0 + 36 + 18.0f * (float)r, pp_rgba(0xF2F4FA, alpha), 13, 1, v[r]);
    ++c;
  }
}

void frame(void*) {
  bool now = pp_in_match(H);
  if (now && !in_match) begin();
  if (!now && in_match) {                       // the match just ended
    char line[160];
    for (int i = 0; i < 4; ++i)
      if (st[i].present) {
        std::snprintf(line, sizeof line, "P%d %s: %d kills, %d openings (%d neutral), %.0f%% dealt, L-cancel %d/%d, %d inputs",
                      i + 1, pp_kind_name(st[i].kind), st[i].kills, st[i].openings, st[i].neutral_wins, st[i].damage, st[i].lc_ok,
                      st[i].lc_all, st[i].inputs);
        H->log(ID, line);
      }
    if (frames > 600) {                          // skip matches quit within ten seconds
      if (H->setting_number(ID, "history") > 0.5) save();
      if (H->setting_number(ID, "card") > 0.5) card = CARD_FRAMES;
    }
  }
  in_match = now;
  if (now) {
    update();
    int vk = (int)H->setting_number(ID, "peek_key");
    if (vk > 0 && H->key_down(vk)) draw_card(1.0f);
  } else if (card > 0) {
    --card;
    int vk = (int)H->setting_number(ID, "peek_key");
    if (vk > 0 && H->key_down(vk)) card = 0;     // dismiss
    else draw_card(card < 60 ? (float)card / 60.0f : 1.0f);
  }
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char* config_path) {
  if (!host || host->abi != PP_PLUGIN_ABI) return 1;
  if (!PP_HOST_HAS(host, hud_label)) {
    if (host->log) host->log(ID, "needs PascalPatch 0.3 or newer");
    return 2;
  }
  H = host;
  // history goes beside the plugin (its config file, or the DLL itself)
  HMODULE self = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCSTR>(&pp_plugin_load), &self);
  if (config_path && *config_path) std::snprintf(dll_dir, sizeof dll_dir, "%s", config_path);
  else GetModuleFileNameA(self, dll_dir, MAX_PATH);
  if (char* slash = std::strrchr(dll_dir, '\\')) *slash = 0; else if (char* s2 = std::strrchr(dll_dir, '/')) *s2 = 0; else dll_dir[0] = 0;

  H->declare_setting(ID, R"({"key":"card","type":"bool","label":"Show the stats when a match ends","default":true})");
  H->declare_setting(ID, R"({"key":"peek_key","type":"key","label":"Hold to see the stats during a match","default":"F4"})");
  H->declare_setting(ID, R"J({"key":"history","type":"bool","label":"Keep a history file (match-stats.jsonl)","default":true})J");
  H->set_status(ID, "Counting openings, kills and L-cancels. Hold F4 in a match to see them.");
  H->on_frame(frame, nullptr);
  return 0;
}
