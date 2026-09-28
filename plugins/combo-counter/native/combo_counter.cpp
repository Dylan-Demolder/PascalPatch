// combo-counter: counts the hits and damage of every combo as it happens (PascalPatch native
// plugin, read-only).
//
// A combo starts when a fighter takes damage and lasts while they stay punished (hit, tumbling,
// grabbed, thrown, knocked down, teching) or get less than `reset` frames to act between hits,
// the rule Slippi's stats use (45 frames). While a combo runs, a counter under the timer shows
// "4 HITS 37%"; when it ends, the total stays up for a moment ("5 hit combo, 52%, KILL") and the
// best combo of the match is kept in the corner. Needs PascalPatch 0.3.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* ID = "combo-counter";
constexpr int SHOW = 180;

const pp_host* H = nullptr;

struct Combo {
  bool on = false;
  int attacker = -1;
  int hits = 0;
  float start_pct = 0, pct = 0;
  int free = 0;            // frames the victim has been able to act since the last hit
  char opener[24] = "";
};

struct Summary { char text[80] = ""; uint32_t rgb = 0xF2F4FA; int age = SHOW; };

struct Victim {
  uint32_t fp = 0;
  float pct = 0;
  int stocks = 0;
  Combo c;
  Summary last;
};

Victim v[4];
struct Best { int hits = 0; float dmg = 0; } best[4];   // by attacker
bool was_in_match = false;

int setting_int(const char* k, int dflt) {
  double d = H->setting_number(ID, k);
  return d > 0 ? (int)d : dflt;
}

void end_combo(int d, bool kill) {
  Combo& c = v[d].c;
  if (!c.on) return;
  c.on = false;
  float dmg = c.pct - c.start_pct;
  if (c.hits < setting_int("min_hits", 2) && !kill) return;
  Summary& s = v[d].last;
  s.age = 0;
  s.rgb = c.attacker >= 0 ? pp_port_rgb[c.attacker] : 0xF2F4FA;
  std::snprintf(s.text, sizeof s.text, "%d hit combo, %.0f%%%s", c.hits, dmg, kill ? ", KILL" : "");
  if (c.attacker >= 0) {
    Best& b = best[c.attacker];
    if (c.hits > b.hits || (c.hits == b.hits && dmg > b.dmg)) { b.hits = c.hits; b.dmg = dmg; }
  }
  char line[128];
  std::snprintf(line, sizeof line, "P%d on P%d: %s (opened with %s)", c.attacker + 1, d + 1, s.text, c.opener);
  H->log(ID, line);
}

// who hit this fighter: the game records the port; fall back to the only other fighter
int attacker_of(int d) {
  int src = (int)H->rd32(v[d].fp + PP_FT_HIT_SOURCE);
  if (src >= 0 && src < 4 && src != d && v[src].fp) return src;
  int only = -1, n = 0;
  for (int i = 0; i < 4; ++i) if (i != d && v[i].fp) { only = i; ++n; }
  return n == 1 ? only : -1;
}

void frame(void*) {
  if (!pp_in_match(H)) {
    if (was_in_match) for (int d = 0; d < 4; ++d) end_combo(d, false);
    was_in_match = false;
    return;
  }
  if (!was_in_match) { for (auto& x : v) x = Victim{}; for (auto& b : best) b = Best{}; }
  was_in_match = true;

  const int reset = setting_int("reset", 45);
  for (int d = 0; d < 4; ++d) {
    Victim& x = v[d];
    uint32_t fp = pp_fighter(H, d);
    if (!fp) { end_combo(d, false); x.fp = 0; continue; }
    float pct = H->rdf32(fp + PP_FT_PERCENT);
    int stocks = (int)(int8_t)H->rd8(pp_player_block(d) + PP_PL_STOCKS);
    uint32_t st = H->rd32(fp + PP_FT_STATE);
    if (fp != x.fp) { x.fp = fp; x.pct = pct; x.stocks = stocks; continue; }

    Combo& c = x.c;
    bool died = stocks < x.stocks || pp_state_is_dead(st);
    if (died && c.on) end_combo(d, true);
    if (pct + 0.01f < x.pct) {       // respawned at 0%
      end_combo(d, false);
      x.pct = pct;
    }
    if (pct > x.pct + 0.01f) {        // took damage this frame
      int a = attacker_of(d);
      if (c.on && a != c.attacker) end_combo(d, false);   // someone else took over
      if (!c.on) {
        c = Combo{};
        c.on = true;
        c.attacker = a;
        c.start_pct = x.pct;
        uint32_t afp = a >= 0 ? v[a].fp : 0;
        std::snprintf(c.opener, sizeof c.opener, "%s", afp ? pp_move_name(H->rd32(afp + PP_FT_KIND), H->rd32(afp + PP_FT_STATE)) : "?");
      }
      ++c.hits;
      c.free = 0;
      c.pct = pct;
    }
    if (c.on && !died) {
      if (pp_state_is_punished(st) || pp_hitlag(H, fp) > 0) c.free = 0;
      else if (++c.free > reset) end_combo(d, false);
    }
    x.pct = pct;
    x.stocks = stocks;
    if (x.last.age < SHOW) ++x.last.age;
  }

  // draw: live counters and fresh summaries stacked under the timer
  float op = (float)H->setting_number(ID, "opacity");
  if (op <= 0) op = 0.95f;
  float y = 92;
  const int min_hits = setting_int("min_hits", 2);
  for (int d = 0; d < 4; ++d) {
    Victim& x = v[d];
    if (x.c.on && x.c.hits >= min_hits) {
      uint32_t rgb = x.c.attacker >= 0 ? pp_port_rgb[x.c.attacker] : 0xF2F4FA;
      char big[24], small[24];
      std::snprintf(big, sizeof big, "%d HITS", x.c.hits);
      std::snprintf(small, sizeof small, "%.0f%%", x.c.pct - x.c.start_pct);
      H->hud_label(314, y, pp_rgba(rgb, op), 26, 2, big);
      H->hud_label(326, y + 6, pp_rgba(0xF2F4FA, op), 20, 0, small);
      y += 32;
    } else if (x.last.age < SHOW && x.last.text[0]) {
      float a = x.last.age < SHOW - 40 ? 1.0f : (float)(SHOW - x.last.age) / 40.0f;
      H->hud_label(320, y, pp_rgba(x.last.rgb, op * a), 20, 1, x.last.text);
      y += 26;
    }
  }
  if (H->setting_number(ID, "best") > 0.5) {
    float by = 440;
    for (int a = 0; a < 4; ++a) {
      if (!best[a].hits || !v[a].fp) continue;
      char line[48];
      std::snprintf(line, sizeof line, "P%d best: %d hits, %.0f%%", a + 1, best[a].hits, best[a].dmg);
      H->hud_label(632, by, pp_rgba(pp_port_rgb[a], op * 0.9f), 13, 2, line);
      by -= 15;
    }
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
  H->declare_setting(ID, R"({"key":"min_hits","type":"int","label":"Show combos from this many hits","default":2,"min":1,"max":10})");
  H->declare_setting(ID, R"({"key":"reset","type":"int","label":"Frames free before a combo ends","default":45,"min":10,"max":120})");
  H->declare_setting(ID, R"({"key":"best","type":"bool","label":"Keep each player's best combo on screen","default":true})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.95,"min":0.3,"max":1.0})");
  H->set_status(ID, "Counting combos during matches.");
  H->on_frame(frame, nullptr);
  return 0;
}
