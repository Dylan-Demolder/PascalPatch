// info-display: the numbers behind each fighter, the way a lab shows them (PascalPatch native plugin,
// read-only).
//
// A panel per fighter with the rows you pick: action state and how long it has lasted, position,
// own and knockback velocity, ground speed, jumps left, shield health, intangibility, the ledge
// regrab timer, stick and trigger, hitlag and hitstun. And a flash on the fighter the frame it can
// act again after hitstun, landing lag, shield stun, a knockdown or its own move ("actionable").
//
// Fighter fields and the camera are the SDK's (pascalpatch/melee.h, camera.h).
// Needs PascalPatch 0.3; the flash is a ring on 0.3 and a filled glow on 0.4.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/camera.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace {

constexpr const char* ID = "info-display";

constexpr uint32_t LABEL = PP_RGB_DIM, VALUE = PP_RGB_TEXT, GOOD = PP_RGB_GOOD, WARN = PP_RGB_WARN, BAD = PP_RGB_BAD;
constexpr int FLASH_FRAMES = 8;

const pp_host* H = nullptr;

struct Row { const char* key; const char* label; bool on_by_default; };
constexpr Row ROWS[] = {
  {"row_state", "State and frame", true},
  {"row_pos", "Position", true},
  {"row_vel", "Own velocity", true},
  {"row_kb", "Knockback velocity", true},
  {"row_ground", "Ground speed", false},
  {"row_jumps", "Jumps left", true},
  {"row_shield", "Shield health", true},
  {"row_intang", "Intangibility", true},
  {"row_ledge", "Ledge regrab timer", false},
  {"row_stun", "Hitlag and hitstun", false},
  {"row_stick", "Stick and trigger", false},
};

struct Fighter {
  uint32_t fp = 0, state = 0xFFFFFFFF;
  int in_state = 0;          // frames in the current state
  int lag = 0;               // frames of lag before now
  int flash = 0;             // frames of flash left
  int last_lag = 0;          // how long the lag it just left lasted
};
Fighter ft[4];


bool on(const char* k) { return H->setting_number(ID, k) > 0.5; }

void fmt(char* out, size_t n, const char* f, ...) {
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(out, n, f, ap);
  va_end(ap);
}

// States a fighter is stuck in until they end: the lag the flash marks the end of.
bool is_lag(uint32_t s) {
  return pp_state_is_damage(s) || pp_state_is_aerial_landing(s) || s == PP_ST_LANDING_FALL_SPECIAL ||
         s == PP_ST_GUARD_SET_OFF || pp_state_is_down(s) || (s >= PP_ST_ATTACK_11 && s < PP_ST_LANDING_AIR_N) ||
         s >= 0x155;
}

bool can_act(uint32_t fp, uint32_t s) {
  if (s == PP_ST_LANDING) return H->rdf32(fp + PP_FT_ANIM_FRAME) >= H->rdf32(fp + PP_FT_LANDING_LAG);   // ftCo_Landing_IASA
  return pp_state_is_actionable(s) || pp_can_interrupt(H, fp);
}

void track(Fighter& f) {
  uint32_t s = H->rd32(f.fp + PP_FT_STATE);
  if (s != f.state) { f.state = s; f.in_state = 1; } else ++f.in_state;
  if (f.flash > 0) --f.flash;
  if (can_act(f.fp, s)) {
    if (f.lag >= 3) { f.flash = FLASH_FRAMES; f.last_lag = f.lag; }
    f.lag = 0;
  } else if (is_lag(s) || f.lag > 0) {
    ++f.lag;
  }
}

void flash(const pp_camera& cam, const Fighter& f, int port, float op) {
  if (f.flash <= 0) return;
  float x = H->rdf32(f.fp + PP_FT_POS), y = H->rdf32(f.fp + PP_FT_POS + 4);
  float sx, sy, ppu;
  if (!pp_project(&cam, x, y + 6.0f, 0, &sx, &sy, &ppu)) return;
  float t = (float)f.flash / FLASH_FRAMES, r = (6.0f + 2.5f * (1 - t)) * ppu;   // about the body, widening as it fades
  if (r < 8) r = 8;
  if (r > 70) r = 70;
  if (PP_HOST_HAS(H, hud_capsule)) H->hud_circle(sx, sy, r, pp_rgba(GOOD, 0.35f * t * op), 1);
  H->hud_circle(sx, sy, r, pp_rgba(GOOD, t * op), 0);
  H->hud_circle(sx, sy, r + 1.5f, pp_rgba(GOOD, 0.6f * t * op), 0);
  (void)port;
}

// A spot for a panel: stacked in its corner with other plugins' panels on PascalPatch 0.5
// (hud_place), or the fixed spot given on older runtimes.
void place(int corner, float w, float h, float fixed_x, float fixed_y, float& x, float& y) {
  if (PP_HOST_HAS(H, hud_place)) H->hud_place(corner, w, h, &x, &y);
  else { x = fixed_x; y = fixed_y; }
}

void panel(int port, const Fighter& f, float op) {
  const uint32_t fp = f.fp, s = f.state;
  char lines[12][64];
  uint32_t colors[12];
  int n = 0;
  auto add = [&](uint32_t rgb, const char* fm, ...) {
    va_list ap;
    va_start(ap, fm);
    std::vsnprintf(lines[n], sizeof lines[n], fm, ap);
    va_end(ap);
    colors[n++] = rgb;
  };
  auto vec = [&](uint32_t a, float& x, float& y) {   // no "-0.000"
    x = H->rdf32(fp + a) + 0.0f; y = H->rdf32(fp + a + 4) + 0.0f;
    if (std::fabs(x) < 0.0005f) x = 0;
    if (std::fabs(y) < 0.0005f) y = 0;
  };
  float x, y;
  if (on("row_state")) add(can_act(fp, s) ? GOOD : VALUE, "%s  f%d", pp_state_name(H->rd32(fp + PP_FT_KIND), s), f.in_state);
  if (on("row_pos")) { vec(PP_FT_POS, x, y); add(VALUE, "Pos     %8.2f %8.2f", x, y); }
  if (on("row_vel")) { vec(PP_FT_SELF_VEL, x, y); add(VALUE, "Vel     %8.3f %8.3f", x, y); }
  if (on("row_kb")) {
    vec(PP_FT_KB_VEL, x, y);
    add(x || y ? WARN : VALUE, "KB vel  %8.3f %8.3f", x, y);
  }
  if (on("row_ground")) add(VALUE, "Ground  %8.3f  %s", H->rdf32(fp + PP_FT_GROUND_VEL), H->rd32(fp + PP_FT_AIRBORNE) ? "air" : "grounded");
  if (on("row_jumps")) {
    int max = (int)H->rd32(fp + PP_FT_MAX_JUMPS), used = H->rd8(fp + PP_FT_JUMPS_USED);
    int left = max - used < 0 ? 0 : max - used;
    add(left ? VALUE : BAD, "Jumps   %d of %d   walljumps %d", left, max, H->rd8(fp + PP_FT_WALLJUMPS));
  }
  if (on("row_shield")) {
    float sh = H->rdf32(fp + PP_FT_SHIELD);
    add(sh > 30 ? VALUE : sh > 15 ? WARN : BAD, "Shield  %5.1f / 60", sh);
  }
  if (on("row_intang")) {
    int ti = (int)H->rd32(fp + PP_FT_INTANGIBLE), tv = (int)H->rd32(fp + PP_FT_INVINCIBLE), mv = (int)H->rd32(fp + PP_FT_MOVE_HURT);
    if (ti > 0) add(GOOD, "Intangible  %d frame%s left", ti, ti == 1 ? "" : "s");
    else if (tv > 0) add(GOOD, "Invincible  %d frame%s left", tv, tv == 1 ? "" : "s");
    else if (mv == PP_HURT_INTANGIBLE) add(GOOD, "Intangible  (move)");
    else if (mv == PP_HURT_INVINCIBLE) add(GOOD, "Invincible  (move)");
    else add(LABEL, "Vulnerable");
  }
  if (on("row_ledge")) {
    int cd = (int)H->rd32(fp + PP_FT_LEDGE_CD);
    add(cd > 0 ? WARN : VALUE, cd > 0 ? "Ledge   regrab in %d" : "Ledge   can grab", cd);
  }
  if (on("row_stun")) {
    float hl = H->rdf32(fp + PP_FT_HITLAG), hs = pp_state_is_damage(s) ? H->rdf32(fp + PP_FT_HITSTUN) : 0;
    add(hl > 0 || hs > 0 ? WARN : VALUE, "Hitlag  %.0f   hitstun %.0f", hl, hs);
  }
  if (on("row_stick")) {
    vec(PP_FT_STICK, x, y);
    add(VALUE, "Stick   %6.3f %6.3f  L/R %.2f", x, y, H->rdf32(fp + PP_FT_TRIGGER));
  }
  if (!n) return;

  const float w = 176, lh = 12.5f, h = 18 + n * lh;
  float x0 = port % 2 == 0 ? 8.0f : 640 - 8 - w, y0;
  if (H->setting_number(ID, "place") > 0.5)   // halfway down the sides; ports 3 and 4 under 1 and 2
    y0 = 205 + (float)(port / 2) * (h + 6);
  else
    place(port % 2, w, h, x0, 8 + (float)(port / 2) * (h + 6), x0, y0);
  H->hud_rect(x0, y0, x0 + w, y0 + h, pp_rgba(0x111838, 0.8f * op), 5, 1);
  H->hud_rect(x0, y0, x0 + 3, y0 + h, pp_rgba(pp_port_rgb[port], op), 1, 1);
  char t[32];
  fmt(t, sizeof t, "P%d  %s", port + 1, pp_kind_name(H->rd32(fp + PP_FT_KIND)));
  H->hud_text(x0 + 9, y0 + 3, pp_rgba(LABEL, op), 11, t);
  for (int i = 0; i < n; ++i) H->hud_text(x0 + 9, y0 + 16 + i * lh, pp_rgba(colors[i], op), 10.5f, lines[i]);
}

bool watched(int port) {
  int which = (int)H->setting_number(ID, "ports");   // 0 port 1, 1 ports 1 and 2, 2 everyone
  return which == 2 || port == 0 || (which == 1 && port == 1);
}

bool was_in_match = false;

void frame(void*) {
  if (!pp_in_match(H)) { was_in_match = false; return; }
  if (!was_in_match) for (auto& f : ft) f = Fighter{};
  was_in_match = true;

  static bool key_was_down = false, hidden = false;
  int vk = (int)H->setting_number(ID, "toggle_key");
  bool key_down = vk > 0 && H->key_down(vk);
  if (key_down && !key_was_down) { hidden = !hidden; H->toast(ID, hidden ? "Info display hidden" : "Info display shown"); }
  key_was_down = key_down;

  float op = (float)H->setting_number(ID, "opacity");
  if (op <= 0) op = 0.9f;
  pp_camera cam;
  bool have_cam = on("flash") && pp_camera_read(H, &cam);
  for (int i = 0; i < 4; ++i) {
    Fighter& f = ft[i];
    uint32_t fp = pp_fighter(H, i);
    if (!fp) { f = Fighter{}; continue; }
    if (fp != f.fp) { f = Fighter{}; f.fp = fp; }
    track(f);
    if (hidden) continue;
    if (have_cam && (on("flash_all") || watched(i))) flash(cam, f, i, op);
    if (watched(i)) panel(i, f, op);
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
  H->declare_setting(ID, R"({"key":"ports","type":"choice","label":"Fighters","default":"p1p2","options":[{"value":"p1","label":"Port 1"},{"value":"p1p2","label":"Ports 1 and 2"},{"value":"all","label":"All ports"}]})");
  char json[160];
  for (const Row& r : ROWS) {
    fmt(json, sizeof json, R"({"key":"%s","type":"bool","label":"%s","default":%s})", r.key, r.label, r.on_by_default ? "true" : "false");
    H->declare_setting(ID, json);
  }
  H->declare_setting(ID, R"({"key":"place","type":"choice","label":"Panels","default":"top","options":[{"value":"top","label":"Top corners"},{"value":"middle","label":"Halfway down the sides"}]})");
  H->declare_setting(ID, R"({"key":"flash","type":"bool","label":"Flash when a fighter can act again","default":true})");
  H->declare_setting(ID, R"({"key":"flash_all","type":"bool","label":"Flash every fighter, not just those with a panel","default":true})");
  H->declare_setting(ID, R"({"key":"toggle_key","type":"key","label":"Show / hide","default":"Insert"})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.9,"min":0.3,"max":1.0})");
  H->set_status(ID, "Showing each fighter's state, position, velocities, jumps, shield and intangibility.");
  H->on_frame(frame, nullptr);
  return 0;
}
