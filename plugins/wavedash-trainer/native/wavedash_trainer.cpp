// wavedash-trainer: a coach for wavedashes (PascalPatch native plugin, read-only).
//
// Every wavedash is broken down in a panel: a frame strip (jumpsquat, where L/R was pressed, the
// airborne frames before the air dodge, the landing lag), a stick gauge (where the stick was, the
// band the game treats as flat and the ideal notch), how long the wavedash was against the longest
// your character can do, the gap to the next one, and one tip on what to change.
//
// The rules are the game's (PlCo.dat's constants, checked frame by frame in game):
//   - L/R during jumpsquat does nothing: the earliest air dodge is on the frame jumpsquat ends
//     ("frame 1"), which goes straight from jumpsquat to the landing;
//   - the air dodge moves at 3.1 along the stick, x0.9 a frame (ftCo_EscapeAir);
//   - stick heights up to 0.28 read as 0, so the shallowest real angle is about 16.7 degrees below
//     horizontal; a flatter stick air dodges sideways and never lands;
//   - on the ground the slide loses the character's traction a frame, twice that above walking
//     speed (ft_80084F3C), and landing lag is 10 frames.
// So the longest wavedash is frame 1 at 16.7 degrees for every character (landing speed 2.67);
// its length in units comes from each character's traction. Needs PascalPatch 0.3.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* ID = "wavedash-trainer";

constexpr float AIRDODGE_SPEED = 3.1f, AIRDODGE_DECAY = 0.9f, STICK_DEADZONE = 0.28f, FAST_FRICTION = 2.0f;
constexpr float BEST_DEG = 16.71f;                                   // asin(0.2875): the first stick notch past 0.28
constexpr float BEST_SPEED = AIRDODGE_SPEED * 0.95777f * AIRDODGE_DECAY;   // frame 1 at that angle: 2.67
constexpr int MAX_AIR = 8;            // airborne frames to wait for an air dodge
constexpr int MAX_DODGE = 6;          // frames from the air dodge to the landing for it to count as a wavedash
constexpr int GAP_WATCH = 40;         // frames after the landing lag in which a new jump counts as a chain
constexpr int HISTORY = 16;
constexpr uint32_t GOOD = PP_RGB_GOOD, OK = PP_RGB_WARN, BAD = PP_RGB_BAD, INFO = PP_RGB_TEXT, DIM = PP_RGB_DIM;
constexpr uint32_t C_SQUAT = 0x8A93B8, C_AIR = 0xF2A444, C_LAG = 0x5B7BD6;

const pp_host* H = nullptr;

enum Phase { IDLE, SQUAT, AIR, DODGE, LAG };
enum Grade { G_NONE, G_PERFECT, G_GOOD, G_OK, G_FAIL };

struct Attempt {
  int squat = 0;            // jumpsquat frames
  int early = -1;           // jumpsquat frame (1-based) L/R was pressed on, -1 none
  int dodge = 0;            // airborne frame of the air dodge (1 = perfect), 0 none
  int dodge_to_land = 0;    // frames from the air dodge to the landing
  float sx = 0, sy = 0;     // stick at the air dodge
  float speed = 0;          // ground speed on landing
  float length = 0, best = 0;   // slide lengths (units): this one, and the longest possible
  bool out_of_shield = false;
  Grade grade = G_NONE;
  char headline[64] = "", tip[80] = "";
};

struct Player {
  uint32_t fp = 0, state = 0, prev = 0;
  Phase phase = IDLE;
  int t = 0;                // frames in the current phase
  int since_lag = -1;       // frames since the last wavedash's landing lag ended
  int gap = -1;             // the gap before this attempt (chains)
  Attempt cur, last;
  bool has_last = false;
  int age = 1000;           // frames since `last` was graded
  Grade history[HISTORY] = {};
  int hist_n = 0;
  int perfect = 0, clean = 0, all = 0;
};
Player pl[4];
bool was_in_match = false;

bool on(const char* k) { return H->setting_number(ID, k) > 0.5; }
bool pressed_lr(uint32_t fp) { return (H->rd32(fp + PP_FT_PRESSED) & (PP_BTN_L | PP_BTN_R)) != 0; }
bool is_jump(uint32_t s) { return s == PP_ST_JUMP_F || s == PP_ST_JUMP_F + 1; }

// Slide length from a landing speed, the way the ground slows a fighter down.
float slide(float v, float traction, float walk_max) {
  float sum = 0;
  for (int i = 0; i < 400 && v > 0; ++i) {
    sum += v;
    v -= v > walk_max ? traction * FAST_FRICTION : traction;
  }
  return sum;
}

void fmt(char* out, size_t n, const char* f, ...) {
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(out, n, f, ap);
  va_end(ap);
}

const char* plural(int n) { return n == 1 ? "" : "s"; }

void finish(Player& p, Grade g) {
  Attempt& a = p.cur;
  a.grade = g;
  ++p.all;
  if (g == G_PERFECT || g == G_GOOD) ++p.clean;
  if (g == G_PERFECT) ++p.perfect;
  for (int i = HISTORY - 1; i > 0; --i) p.history[i] = p.history[i - 1];
  p.history[0] = g;
  if (p.hist_n < HISTORY) ++p.hist_n;
  p.last = a;
  p.has_last = true;
  p.age = 0;
  char msg[160];
  fmt(msg, sizeof msg, "P%d %s | %s", (int)(&p - pl) + 1, a.headline, a.tip);
  H->log(ID, msg);
  p.phase = IDLE;
}

// Pressed during jumpsquat and never air dodged.
void grade_early(Player& p) {
  Attempt& a = p.cur;
  int by = a.squat - a.early + 1;
  fmt(a.headline, sizeof a.headline, "Too early: L/R %d frame%s before the jump", by, plural(by));
  fmt(a.tip, sizeof a.tip, "L/R during jumpsquat is ignored. Press it %d frame%s later.", by, plural(by));
  finish(p, G_FAIL);
}

void grade_landed(Player& p) {
  Attempt& a = p.cur;
  float traction = H->rdf32(p.fp + PP_FT_TRACTION), walk = H->rdf32(p.fp + PP_FT_WALK_MAX);
  a.length = slide(a.speed, traction, walk);
  a.best = slide(BEST_SPEED, traction, walk);
  int pct = a.best > 0 ? (int)(100 * a.length / a.best + 0.5f) : 0;
  float deg = std::atan2(-a.sy, std::fabs(a.sx)) * 57.29578f;
  bool in_place = deg > 80;
  const char* oos = a.out_of_shield ? " out of shield" : "";
  if (in_place) {
    fmt(a.headline, sizeof a.headline, "Wavedash in place%s, frame %d", oos, a.dodge);
    fmt(a.tip, sizeof a.tip, "Straight down: no slide. Angle the stick to travel.");
    finish(p, a.dodge <= 2 ? G_GOOD : G_OK);
    return;
  }
  fmt(a.headline, sizeof a.headline, "Wavedash%s: frame %d, %.0f deg, %d%% length", oos, a.dodge, deg, pct);
  Grade g = a.dodge == 1 && pct >= 97 ? G_PERFECT : a.dodge <= 2 && pct >= 85 ? G_GOOD : G_OK;
  if (g == G_PERFECT) {
    fmt(a.tip, sizeof a.tip, "Perfect: first frame, shallowest angle. Do it again.");
  } else if (a.dodge > 1) {
    int late = a.dodge - 1;
    fmt(a.tip, sizeof a.tip, "%d frame%s late: press L/R the frame your jumpsquat ends (%d after jump).",
        late, plural(late), a.squat);
  } else {
    fmt(a.tip, sizeof a.tip, "Angle %.0f deg: aim shallower, just under the flat band (%.0f deg).", deg, BEST_DEG);
  }
  finish(p, g);
}

// The air dodge did not reach the ground in time: late, or the stick read as flat.
void grade_floated(Player& p) {
  Attempt& a = p.cur;
  if (a.sy > -STICK_DEADZONE && (a.sx != 0 || a.sy != 0)) {
    fmt(a.headline, sizeof a.headline, "Stick too flat: the air dodge went sideways");
    fmt(a.tip, sizeof a.tip, "Stick heights up to 0.28 count as flat. Tilt a little further down.");
  } else if (a.sx == 0 && a.sy == 0) {
    fmt(a.headline, sizeof a.headline, "Neutral air dodge: no stick direction");
    fmt(a.tip, sizeof a.tip, "Hold the stick down and to the side as you press L/R.");
  } else {
    fmt(a.headline, sizeof a.headline, "Too late: air dodge on airborne frame %d", a.dodge);
    fmt(a.tip, sizeof a.tip, "You were too high to land. Press L/R the frame jumpsquat ends (%d after jump).", a.squat);
  }
  finish(p, G_FAIL);
}

void step(Player& p) {
  const uint32_t fp = p.fp, s = p.state;
  const bool entered = s != p.prev;
  if (p.since_lag >= 0 && ++p.since_lag > GAP_WATCH) p.since_lag = -1;
  if (p.age < 1000) ++p.age;

  switch (p.phase) {
    case IDLE:
      break;
    case SQUAT:
      if (s == PP_ST_KNEE_BEND) {
        ++p.cur.squat;
        if (pressed_lr(fp) && p.cur.early < 0) p.cur.early = p.cur.squat;
        break;
      }
      // jumpsquat is over: frame 1
      if (s == PP_ST_ESCAPE_AIR || s == PP_ST_LANDING_FALL_SPECIAL) {
        p.cur.dodge = 1;
        p.cur.sx = H->rdf32(fp + PP_FT_STICK);
        p.cur.sy = H->rdf32(fp + PP_FT_STICK + 4);
        if (s == PP_ST_LANDING_FALL_SPECIAL) {
          p.cur.speed = std::fabs(H->rdf32(fp + PP_FT_GROUND_VEL));
          grade_landed(p);
          p.phase = LAG; p.t = 0;
        } else {
          p.phase = DODGE; p.t = 0;
        }
      } else if (is_jump(s)) {
        p.phase = AIR; p.t = 1;
      } else {
        p.phase = IDLE;
      }
      break;
    case AIR:
      if (s == PP_ST_ESCAPE_AIR || s == PP_ST_LANDING_FALL_SPECIAL) {
        p.cur.dodge = p.t + 1;
        p.cur.sx = H->rdf32(fp + PP_FT_STICK);
        p.cur.sy = H->rdf32(fp + PP_FT_STICK + 4);
        if (s == PP_ST_LANDING_FALL_SPECIAL) {
          p.cur.speed = std::fabs(H->rdf32(fp + PP_FT_GROUND_VEL));
          grade_landed(p);
          p.phase = LAG; p.t = 0;
        } else {
          p.phase = DODGE; p.t = 0;
        }
      } else if (is_jump(s) && p.t < MAX_AIR) {
        ++p.t;
      } else {
        if (p.cur.early > 0) grade_early(p);   // pressed in jumpsquat, then just jumped
        p.phase = IDLE;
      }
      break;
    case DODGE:
      ++p.t;
      if (s == PP_ST_LANDING_FALL_SPECIAL) {
        p.cur.dodge_to_land = p.t;
        p.cur.speed = std::fabs(H->rdf32(fp + PP_FT_GROUND_VEL));
        if (p.t <= MAX_DODGE) {
          grade_landed(p);
          p.phase = LAG; p.t = 0;
        } else {
          grade_floated(p);
        }
      } else if (s != PP_ST_ESCAPE_AIR || p.t > MAX_DODGE) {
        grade_floated(p);
      }
      break;
    case LAG:
      if (s != PP_ST_LANDING_FALL_SPECIAL) { p.since_lag = 0; p.phase = IDLE; }
      break;
  }

  // a new attempt starts with jumpsquat (also out of shield)
  if (entered && s == PP_ST_KNEE_BEND && p.phase == IDLE) {
    p.cur = Attempt{};
    p.cur.squat = 1;
    p.cur.out_of_shield = pp_state_is_shield(p.prev);
    if (pressed_lr(fp)) p.cur.early = 1;
    p.gap = p.since_lag;
    p.since_lag = -1;
    p.phase = SQUAT;
    p.t = 0;
  }
}

// ---------------------------------------------------------------- drawing

uint32_t grade_rgb(Grade g) { return g == G_PERFECT || g == G_GOOD ? GOOD : g == G_OK ? OK : BAD; }

// Splits `text` into lines of at most `width` characters at spaces; returns how many.
int wrap(const char* text, int width, char out[][96], int max_lines) {
  int n = 0;
  const char* p = text;
  while (*p && n < max_lines) {
    while (*p == ' ') ++p;
    int len = (int)std::strlen(p), cut = len;
    if (len > width) {
      cut = width;
      while (cut > 0 && p[cut] != ' ') --cut;
      if (cut == 0) cut = width;
    }
    if (cut > 95) cut = 95;
    std::memcpy(out[n], p, (size_t)cut);
    out[n][cut] = 0;
    ++n;
    p += cut;
  }
  return n;
}

void seg(float x0, float y0, float x1, float y1, float w, uint32_t rgba) {
  if (PP_HOST_HAS(H, hud_capsule)) { H->hud_capsule(x0, y0, w, x1, y1, w, rgba, 1); return; }
  for (int i = 0; i <= 8; ++i) {   // 0.3: a dotted line
    float t = (float)i / 8;
    H->hud_circle(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, w, rgba, 1);
  }
}

// The stick gauge: the flat band (red), the ideal notches (green) and the stick at the air dodge.
void gauge(float cx, float cy, float r, const Attempt& a, float op) {
  H->hud_circle(cx, cy, r, pp_rgba(0x0B1030, 0.8f * op), 1);
  H->hud_circle(cx, cy, r, pp_rgba(0xFFFFFF, 0.35f * op), 0);
  float band = STICK_DEADZONE * r;
  H->hud_rect(cx - r * 0.96f, cy - band, cx + r * 0.96f, cy + band, pp_rgba(0xFF6B5E, 0.4f * op), 0, 1);
  float bx = std::cos(BEST_DEG / 57.29578f) * r, by = std::sin(BEST_DEG / 57.29578f) * r;
  seg(cx, cy, cx + bx, cy + by, 0.8f, pp_rgba(GOOD, 0.9f * op));
  seg(cx, cy, cx - bx, cy + by, 0.8f, pp_rgba(GOOD, 0.9f * op));
  if (a.dodge > 0) {
    float sx = cx + a.sx * r, sy = cy - a.sy * r;
    seg(cx, cy, sx, sy, 1.0f, pp_rgba(0xF2D25B, op));
    H->hud_circle(sx, sy, 3.0f, pp_rgba(0xF2D25B, op), 1);
  }
}

// One box per frame: jumpsquat, airborne frames, the air dodge, landing lag.
void strip(float x, float y, const Attempt& a, float op) {
  const float w = 7, h = 9, gap = 1.5f;
  int n = 0;
  auto box = [&](uint32_t rgb, float alpha) {
    H->hud_rect(x + n * (w + gap), y, x + n * (w + gap) + w, y + h, pp_rgba(rgb, alpha * op), 1.5f, 1);
    ++n;
  };
  for (int i = 1; i <= a.squat; ++i) {
    box(C_SQUAT, 0.9f);
    if (i == a.early) {   // the ignored press
      float bx = x + (n - 1) * (w + gap) + w / 2;
      seg(bx, y - 5, bx, y + h + 3, 1.0f, pp_rgba(BAD, op));
    }
  }
  if (a.dodge > 0) {
    for (int i = 1; i < a.dodge; ++i) box(C_AIR, 0.9f);
    box(grade_rgb(a.grade), 1.0f);   // the air dodge
    int lag = a.grade == G_FAIL ? 0 : 10;
    for (int i = 0; i < lag; ++i) box(C_LAG, 0.55f);
  }
}

// A spot for a panel: stacked in its corner with other plugins' panels on PascalPatch 0.5
// (hud_place), or the fixed spot given on older runtimes.
void place(int corner, float w, float h, float fixed_x, float fixed_y, float& x, float& y) {
  if (PP_HOST_HAS(H, hud_place)) H->hud_place(corner, w, h, &x, &y);
  else { x = fixed_x; y = fixed_y; }
}

void draw(int port, Player& p, float op) {
  if (!p.has_last) {
    if (on("panel")) {
      float x0, y0;
      place(port % 2 == 0 ? PP_CORNER_TOP_RIGHT : PP_CORNER_TOP_LEFT, 180, 34, 452, 70, x0, y0);   // across from the port's own panels
      H->hud_rect(x0, y0, x0 + 180, y0 + 34, pp_rgba(0x111838, 0.78f * op), 6, 1);
      H->hud_rect(x0, y0, x0 + 3, y0 + 34, pp_rgba(pp_port_rgb[port], op), 1, 1);
      char t[40];
      fmt(t, sizeof t, "P%d  Wavedash trainer", port + 1);
      H->hud_text(x0 + 9, y0 + 4, pp_rgba(DIM, op), 12, t);
      H->hud_text(x0 + 9, y0 + 19, pp_rgba(INFO, op), 11, "Jump, then L/R + down-angle");
    }
    return;
  }
  const Attempt& a = p.last;
  if (on("panel")) {
    // the verdict and the tip go under the breakdown, wrapped to the panel
    char head[2][96], tip[3][96];
    int nh = wrap(a.headline, 36, head, 2), nt = wrap(a.tip, 44, tip, 3);
    float x0, y0, w = 250, h = 128 + 6 + nh * 15.0f + nt * 12.5f;
    place(port % 2 == 0 ? PP_CORNER_TOP_RIGHT : PP_CORNER_TOP_LEFT, w, h, 382, 70, x0, y0);
    H->hud_rect(x0, y0, x0 + w, y0 + h, pp_rgba(0x111838, 0.82f * op), 6, 1);
    H->hud_rect(x0, y0, x0 + 3, y0 + h, pp_rgba(pp_port_rgb[port], op), 1, 1);
    char t[64];
    fmt(t, sizeof t, "P%d  Wavedash   perfect %d  clean %d/%d", port + 1, p.perfect, p.clean, p.all);
    H->hud_text(x0 + 9, y0 + 4, pp_rgba(DIM, op), 11, t);
    strip(x0 + 9, y0 + 22, a, op);
    if (a.dodge > 0) fmt(t, sizeof t, "jumpsquat %d  |  air dodge on frame %d", a.squat, a.dodge);
    else fmt(t, sizeof t, "jumpsquat %d  |  no air dodge", a.squat);
    H->hud_text(x0 + 9, y0 + 34, pp_rgba(0xC8CCD8, op), 10, t);
    gauge(x0 + 32, y0 + 72, 20, a, op);
    float tx = x0 + 60, ty = y0 + 52;
    if (a.dodge > 0 && a.grade != G_FAIL) {
      float deg = std::atan2(-a.sy, std::fabs(a.sx)) * 57.29578f;
      fmt(t, sizeof t, "Angle  %.0f deg  (best %.0f)", deg, BEST_DEG);
      H->hud_text(tx, ty, pp_rgba(INFO, op), 11, t);
      int pct = a.best > 0 ? (int)(100 * a.length / a.best + 0.5f) : 0;
      fmt(t, sizeof t, "Length %d%%  (%.0f of %.0f)", pct, a.length, a.best);
      H->hud_text(tx, ty + 14, pp_rgba(pct >= 97 ? GOOD : pct >= 85 ? INFO : OK, op), 11, t);
    }
    if (p.gap >= 0) {
      fmt(t, sizeof t, "Gap    %d frame%s after lag", p.gap, plural(p.gap));
      H->hud_text(tx, ty + 28, pp_rgba(p.gap <= 2 ? GOOD : INFO, op), 11, t);
    }
    // history: newest on the left
    for (int i = 0; i < p.hist_n; ++i)
      H->hud_rect(x0 + 9 + i * 10.5f, y0 + 114, x0 + 17 + i * 10.5f, y0 + 122, pp_rgba(grade_rgb(p.history[i]), (i ? 0.7f : 1) * op), 1.5f, 1);
    float vy = y0 + 132;
    for (int i = 0; i < nh; ++i, vy += 15) H->hud_text(x0 + 9, vy, pp_rgba(grade_rgb(a.grade), op), 12.5f, head[i]);
    for (int i = 0; i < nt; ++i, vy += 12.5f) H->hud_text(x0 + 9, vy, pp_rgba(INFO, 0.9f * op), 10.5f, tip[i]);
    return;   // the verdict is in the panel
  }
  // no panel: the verdict and the tip, above the damage meters on this port's side
  if (p.age < 240) {
    float fade = p.age < 200 ? 1.0f : (float)(240 - p.age) / 40.0f;
    float x = port % 2 == 0 ? 20.0f : 620.0f, y = 330 - (float)(port / 2) * 60;
    int align = port % 2 == 0 ? 0 : 2;
    H->hud_label(x, y, pp_rgba(grade_rgb(a.grade), op * fade), 16, align, a.headline);
    H->hud_label(x, y + 18, pp_rgba(INFO, 0.9f * op * fade), 12, align, a.tip);
  }
}

void reset() {
  for (auto& p : pl) { Player fresh; fresh.fp = p.fp; fresh.state = fresh.prev = p.state; p = fresh; }
}

void frame(void*) {
  if (!pp_in_match(H)) { was_in_match = false; return; }
  if (!was_in_match) { for (auto& p : pl) p = Player{}; }
  was_in_match = true;

  static bool key_was_down = false;
  int vk = (int)H->setting_number(ID, "reset_key");
  bool key_down = vk > 0 && H->key_down(vk);
  if (key_down && !key_was_down) { reset(); H->toast(ID, "Wavedash trainer: reset"); }
  key_was_down = key_down;

  int which = (int)H->setting_number(ID, "watch");   // 0 the chosen port, 1 every human player
  int focus = (int)H->setting_number(ID, "port");
  float op = (float)H->setting_number(ID, "opacity");
  if (op <= 0) op = 0.95f;
  for (int i = 0; i < 4; ++i) {
    Player& p = pl[i];
    uint32_t fp = pp_fighter(H, i);
    bool watched = fp && (which == 1 ? pp_player_type(H, i) == 0 : i == focus);
    if (!watched) { p.fp = 0; continue; }
    if (fp != p.fp) { p = Player{}; p.fp = fp; p.state = p.prev = H->rd32(fp + PP_FT_STATE); }
    p.state = H->rd32(fp + PP_FT_STATE);
    step(p);
    p.prev = p.state;
    draw(i, p, op);
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
  H->declare_setting(ID, R"({"key":"port","type":"choice","label":"Player","default":"1","options":[{"value":"1","label":"Port 1"},{"value":"2","label":"Port 2"},{"value":"3","label":"Port 3"},{"value":"4","label":"Port 4"}]})");
  H->declare_setting(ID, R"({"key":"watch","type":"choice","label":"Coach","default":"player","options":[{"value":"player","label":"Just that player"},{"value":"humans","label":"Every human player"}]})");
  H->declare_setting(ID, R"({"key":"panel","type":"bool","label":"Breakdown panel","default":true})");
  H->declare_setting(ID, R"({"key":"reset_key","type":"key","label":"Reset the count","default":"F12"})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.95,"min":0.3,"max":1.0})");
  H->set_status(ID, "Breaking down every wavedash: timing, angle, length and the gap to the next one.");
  H->on_frame(frame, nullptr);
  return 0;
}
