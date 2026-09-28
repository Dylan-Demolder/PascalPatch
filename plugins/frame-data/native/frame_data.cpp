// frame-data: what each fighter is doing, frame by frame, and who is plus after every hit or
// shield (PascalPatch native plugin, read-only).
//
// A panel per fighter shows the action state, its frame, hitlag, hitstun, shield health and
// whether the fighter can act. When an attack connects (on a body or on a shield) the plugin
// counts the frames until each side can act again; the difference is the frame advantage
// ("Nair on shield -2"), shown under the attacker's panel. A second hit before the defender could
// act is reported as a true combo instead. Needs PascalPatch 0.3 (outlined HUD text).
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* ID = "frame-data";
constexpr int MAX_WAIT = 120;       // give up on an exchange after two seconds
constexpr int SHOW_FRAMES = 180;    // how long a result stays on screen

const pp_host* H = nullptr;

struct Fighter {
  uint32_t fp = 0, kind = 0, state = 0;
  float hitlag = 0, hitstun = 0, frame = 0, x = 0, y = 0, shield = 60;
  bool damaged = false, shielding = false, ready = false, air = false, iasa = false;
};

struct Exchange {        // an attack that connected, being timed
  bool active = false;
  int att = -1, def = -1;
  bool on_shield = false;
  char move[32] = "";
  int t = 0;             // frames since the hit (hitlag included)
  int att_ready = -1, def_ready = -1;
  bool att_air = false;  // an aerial: also time how long before the attacker landed
  int att_land = -1;
};

struct Result {
  char text[80] = "";
  int adv = 0;
  bool combo = false;
  int age = SHOW_FRAMES;
};

Fighter now[4], before[4];
Exchange ex;
Result result[4];        // by attacker port
unsigned long long frames = 0;
bool was_in_match = false;

bool is_shield_state(uint32_t s) { return s >= PP_ST_GUARD_ON && s <= PP_ST_GUARD_REFLECT && s != PP_ST_GUARD_OFF; }

void read(int port, Fighter& f) {
  f = Fighter{};
  f.fp = pp_fighter(H, port);
  if (!f.fp) return;
  f.kind = H->rd32(f.fp + PP_FT_KIND);
  f.state = H->rd32(f.fp + PP_FT_STATE);
  f.hitlag = pp_hitlag(H, f.fp);
  f.frame = H->rdf32(f.fp + PP_FT_ANIM_FRAME);
  f.x = H->rdf32(f.fp + PP_FT_POS);
  f.y = H->rdf32(f.fp + PP_FT_POS + 4);
  f.shield = H->rdf32(f.fp + PP_FT_SHIELD);
  f.air = H->rd32(f.fp + PP_FT_AIRBORNE) != 0;
  f.damaged = pp_state_is_damage(f.state);
  f.hitstun = f.damaged ? H->rdf32(f.fp + PP_FT_HITSTUN) : 0;
  f.shielding = is_shield_state(f.state);
  f.iasa = pp_can_interrupt(H, f.fp);
  // Can act this frame: out of hitlag and in a free state, past the move's IASA frame, or (after
  // being hit) out of hitstun, since the game lets a tumbling or reeling fighter act then.
  f.ready = f.hitlag <= 0 && (pp_state_is_actionable(f.state) || f.iasa || (f.damaged && f.hitstun <= 0) ||
                              f.state == PP_ST_DAMAGE_FALL);
}

// Nothing moved at all since last frame: the game is paused, so the exchange clock stops too.
// (Hitlag counts down every frame, so a hit freeze does not look like a pause.)
bool frozen() {
  bool any = false;
  for (int i = 0; i < 4; ++i) {
    const Fighter &a = now[i], &b = before[i];
    if (!a.fp) continue;
    any = true;
    if (a.fp != b.fp || a.state != b.state || a.frame != b.frame || a.hitlag != b.hitlag || a.x != b.x || a.y != b.y ||
        a.hitstun != b.hitstun)
      return false;
  }
  return any;
}

void finish(bool combo) {
  if (!ex.active) return;
  Result& r = result[ex.att];
  r.age = 0;
  r.combo = combo;
  if (combo) {
    std::snprintf(r.text, sizeof r.text, "%s: true combo", ex.move);
  } else {
    // unresolved sides count as the time we waited: "+120" reads as "they never got to act"
    int a = ex.att_ready >= 0 ? ex.att_ready : ex.t, d = ex.def_ready >= 0 ? ex.def_ready : ex.t;
    r.adv = d - a;
    std::snprintf(r.text, sizeof r.text, "%s %s %+d%s", ex.move, ex.on_shield ? "on shield" : "on hit", r.adv,
                  (ex.att_ready < 0 || ex.def_ready < 0) ? "+" : "");
    // An aerial's advantage depends on how low it hit: say how long the attacker still fell.
    if (ex.att_air && ex.att_land > 1) {
      size_t n = std::strlen(r.text);
      std::snprintf(r.text + n, sizeof r.text - n, " (landed %df later)", ex.att_land - 1);
    }
  }
  char line[96];
  std::snprintf(line, sizeof line, "P%d %s (P%d)", ex.att + 1, r.text, ex.def + 1);
  H->log(ID, line);
  if (H->setting_number(ID, "toast") > 0.5) H->toast(ID, r.text);
  ex.active = false;
}

void track() {
  // a new hit: the defender's hitlag starts while it is hurt or shielding; the attacker is the
  // fighter whose hitlag starts on the same frame
  for (int d = 0; d < 4; ++d) {
    const Fighter& f = now[d];
    if (!f.fp || !(f.hitlag > 0 && before[d].hitlag <= 0) || !(f.damaged || f.shielding)) continue;
    int att = -1;
    for (int a = 0; a < 4; ++a)
      if (a != d && now[a].fp && now[a].hitlag > 0 && before[a].hitlag <= 0 && !now[a].damaged && !now[a].shielding) att = a;
    if (att < 0) continue;
    bool combo = ex.active && ex.def == d && ex.def_ready < 0 && !ex.on_shield && f.damaged;
    if (ex.active) finish(combo);
    ex = Exchange{};
    ex.active = true;
    ex.att = att;
    ex.def = d;
    ex.on_shield = f.shielding;
    ex.att_air = now[att].air;
    std::snprintf(ex.move, sizeof ex.move, "%s", pp_move_name(now[att].kind, now[att].state));
  }
  if (!ex.active) return;
  if (!now[ex.att].fp || !now[ex.def].fp) { ex.active = false; return; }
  if (frozen()) return;
  ++ex.t;
  if (ex.att_air && ex.att_land < 0 && !now[ex.att].air) ex.att_land = ex.t;
  if (ex.att_ready < 0 && now[ex.att].ready) ex.att_ready = ex.t;
  if (ex.def_ready < 0 && now[ex.def].ready) ex.def_ready = ex.t;
  if ((ex.att_ready >= 0 && ex.def_ready >= 0) || ex.t >= MAX_WAIT) finish(false);
}

void chip(float& x, float y, const char* text, uint32_t rgb, float op) {
  float w = 6.5f * (float)std::strlen(text) + 8;
  H->hud_rect(x, y, x + w, y + 13, pp_rgba(rgb, 0.85f * op), 3, 1);
  H->hud_text(x + 4, y + 0.5f, pp_rgba(0x0B1030, op), 12, text);
  x += w + 4;
}

void panel(int port, float op, bool detail) {
  const Fighter& f = now[port];
  const float w = 164, h = detail ? 62.0f : 48.0f;
  // right-hand panels stop short of the corner, where the port shows its own key hint
  float x0 = (port % 2 == 0) ? 8 : 640 - 70 - w;
  float y0 = 8 + (float)(port / 2) * (h + 30);
  uint32_t pc = pp_port_rgb[port];
  H->hud_rect(x0, y0, x0 + w, y0 + h, pp_rgba(0x111838, 0.78f * op), 6, 1);
  H->hud_rect(x0, y0, x0 + 3, y0 + h, pp_rgba(pc, op), 1, 1);

  char line[96];
  std::snprintf(line, sizeof line, "P%d", port + 1);
  H->hud_rect(x0 + 8, y0 + 4, x0 + 26, y0 + 17, pp_rgba(pc, op), 3, 1);
  H->hud_label(x0 + 17, y0 + 4, pp_rgba(0xFFFFFF, op), 12, 1, line);
  H->hud_text(x0 + 31, y0 + 3, pp_rgba(0xF2F4FA, op), 13, pp_kind_name(f.kind));
  H->hud_label(x0 + w - 7, y0 + 3, pp_rgba(f.air ? 0x9FB4FF : 0xB8E986, op), 11, 2, f.air ? "AIR" : "GROUND");

  const char* name = pp_move_name(f.kind, f.state);
  if (f.frame >= 0) std::snprintf(line, sizeof line, "%s  f%d", name, (int)std::floor(f.frame) + 1);
  else std::snprintf(line, sizeof line, "%s", name);
  H->hud_text(x0 + 9, y0 + 18, pp_rgba(0xF2F4FA, op), 13, line);

  float cx = x0 + 9, cy = y0 + 33;
  if (f.hitlag > 0) { std::snprintf(line, sizeof line, "HITLAG %d", (int)std::ceil(f.hitlag)); chip(cx, cy, line, 0xF2C200, op); }
  if (f.damaged && f.hitstun > 0) { std::snprintf(line, sizeof line, "HITSTUN %d", (int)std::ceil(f.hitstun)); chip(cx, cy, line, 0xFF6B5E, op); }
  if (f.ready) chip(cx, cy, "CAN ACT", 0x5BD68A, op);
  else if (f.iasa) chip(cx, cy, "IASA", 0x5BD68A, op);

  if (detail) {
    std::snprintf(line, sizeof line, "state %u  x %.1f  y %.1f", f.state, f.x, f.y);
    H->hud_text(x0 + 9, y0 + 48, pp_rgba(0x8C96BC, op), 11, line);
  }
  // shield health along the bottom edge
  float s = f.shield < 0 ? 0 : (f.shield > 60 ? 60 : f.shield);
  H->hud_rect(x0 + 6, y0 + h - 3, x0 + 6 + (w - 12) * s / 60, y0 + h - 1, pp_rgba(s < 20 ? 0xFF6B5E : 0x6FA8FF, 0.9f * op), 1, 1);

  const Result& r = result[port];
  if (r.age < SHOW_FRAMES && r.text[0]) {
    float fade = r.age < SHOW_FRAMES - 30 ? 1.0f : (float)(SHOW_FRAMES - r.age) / 30.0f;
    uint32_t c = r.combo ? 0xF2C200 : (r.adv > 0 ? 0x5BD68A : (r.adv < 0 ? 0xFF6B5E : 0xF2F4FA));
    H->hud_label(port % 2 == 0 ? x0 + 2 : x0 + w - 2, y0 + h + 4, pp_rgba(c, op * fade), 16, port % 2 == 0 ? 0 : 2, r.text);
  }
}

void frame(void*) {
  ++frames;
  bool in = pp_in_match(H);
  if (!in) {
    if (was_in_match) { ex.active = false; for (auto& r : result) r = Result{}; }
    was_in_match = false;
    return;
  }
  if (!was_in_match) for (auto& b : before) b = Fighter{};
  was_in_match = true;
  for (int i = 0; i < 4; ++i) read(i, now[i]);
  if (H->setting_number(ID, "advantage") > 0.5) track();
  for (auto& r : result) if (r.age < SHOW_FRAMES) ++r.age;

  if (H->setting_number(ID, "panels") > 0.5 || H->setting_number(ID, "advantage") > 0.5) {
    int which = (int)H->setting_number(ID, "ports");   // 0 P1, 1 P1 and P2, 2 all
    float op = (float)H->setting_number(ID, "opacity");
    if (op <= 0) op = 0.9f;
    bool detail = H->setting_number(ID, "detail") > 0.5;
    bool panels = H->setting_number(ID, "panels") > 0.5;
    for (int i = 0; i < 4; ++i) {
      if (!now[i].fp || (which == 0 && i > 0) || (which == 1 && i > 1)) continue;
      if (panels) panel(i, op, detail);
      else if (result[i].age < SHOW_FRAMES && result[i].text[0]) {
        // results only: a single outlined line in the top corner of the attacker's side
        uint32_t c = result[i].combo ? 0xF2C200 : (result[i].adv > 0 ? 0x5BD68A : (result[i].adv < 0 ? 0xFF6B5E : 0xF2F4FA));
        H->hud_label(i % 2 == 0 ? 10.0f : 568.0f, 10.0f + (float)(i / 2) * 22, pp_rgba(c, op), 18, i % 2 == 0 ? 0 : 2, result[i].text);
      }
    }
  }
  for (int i = 0; i < 4; ++i) before[i] = now[i];
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char*) {
  if (!host || host->abi != PP_PLUGIN_ABI) return 1;
  if (!PP_HOST_HAS(host, hud_label)) {
    if (host->log) host->log(ID, "needs PascalPatch 0.3 or newer");
    return 2;
  }
  H = host;
  H->declare_setting(ID, R"({"key":"ports","type":"choice","label":"Fighters","default":"p1p2","options":[{"value":"p1","label":"Port 1"},{"value":"p1p2","label":"Ports 1 and 2"},{"value":"all","label":"All ports"}]})");
  H->declare_setting(ID, R"({"key":"panels","type":"bool","label":"State panels","default":true})");
  H->declare_setting(ID, R"({"key":"advantage","type":"bool","label":"Frame advantage after hits and shields","default":true})");
  H->declare_setting(ID, R"({"key":"toast","type":"bool","label":"Also pop up each result","default":false})");
  H->declare_setting(ID, R"({"key":"detail","type":"bool","label":"Show state ids and positions","default":false})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.9,"min":0.3,"max":1.0})");
  H->set_status(ID, "Frame data on screen during matches.");
  H->on_frame(frame, nullptr);
  return 0;
}
