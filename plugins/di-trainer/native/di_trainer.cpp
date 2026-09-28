// di-trainer: while a fighter is frozen in hitlag, draws where the hit will send it (PascalPatch
// native plugin, read-only).
//
//   grey    the path with no DI
//   yellow  the path with the DI being held right now (follows the stick until hitlag ends)
//   green   the full-tilt DI that survives best (or, if every DI dies, dies last)
//   red X   where a path crosses a blast zone while the fighter is still launched: a KO
//
// A small stick gauge beside the fighter shows the held stick (yellow) and the best DI (green).
//
// The prediction repeats what the game does when hitlag ends (ftCo_Damage_OnExitHitlag): ASDI
// moves the fighter 3 units along the stick if it is pushed past 0.7, then DI turns the launch by
// 18 degrees x (the stick's part across the launch)^2 towards the stick's side (ftCo_8008E5A4).
// The flight then runs as Fighter_procUpdate moves a fighter: knockback velocity loses 0.051 a
// frame along its direction, gravity pulls the fighter's own speed down to its fall speed, and
// position gains both. The constants are PlCo.dat's; both steps were checked against launches
// logged frame by frame in game. Blast zones and the main floor of the tournament stages come from
// the SDK (pp_stage_blast_zones, pp_stage_floor_edge). Needs PascalPatch 0.3; 0.4 draws the paths as lines.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/camera.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr const char* ID = "di-trainer";

constexpr float DI_DEGREES = 18.0f, ASDI_MIN = 0.7f, ASDI_DISTANCE = 3.0f;
constexpr float KB_DECAY = 0.051f, KB_TO_SPEED = 0.03f, HITSTUN_PER_KB = 0.4f;
constexpr int MAX_FRAMES = 240, BEST_SAMPLES = 64;


const pp_host* H = nullptr;

// ---------------------------------------------------------------- the stage

struct Stage { float left, right, top, bottom, edge; bool floor; };

bool read_stage(Stage& s) {
  s.edge = pp_stage_floor_edge(pp_stage_kind(H));
  s.floor = s.edge > 0;
  return pp_stage_blast_zones(H, &s.left, &s.right, &s.top, &s.bottom) != 0;
}

// ---------------------------------------------------------------- the flight

struct Body { float gravity, fall, traction; };

struct Path {
  float x[MAX_FRAMES + 1], y[MAX_FRAMES + 1];
  int n = 0;            // points in x/y
  int ko_at = -1;       // index of the point past a blast zone, or -1
  float margin = 0;     // closest approach to a blast zone while launched (surviving paths)
};

// ASDI and DI for stick (sx, sy), then the flight from (x, y) with launch velocity (kx, ky).
void fly(float x, float y, float kx, float ky, float sx, float sy, bool grounded, float stun,
         const Body& b, const Stage& st, Path& p) {
  if (sx * sx + sy * sy >= ASDI_MIN * ASDI_MIN) {
    x += sx * ASDI_DISTANCE;
    y += sy * ASDI_DISTANCE;
  }
  if (sx != 0 || sy != 0) {
    float mag2 = kx * kx + ky * ky;
    if (mag2 >= 0.00001f) {
      float across = ky * sx - kx * sy;
      float turn = across * across / mag2;
      if (kx * sy - ky * sx < 0) turn = -turn;   // (kb x stick).z: which side the stick is on
      float a = std::atan2(ky, kx) + DI_DEGREES * 3.14159265f / 180.0f * turn, m = std::sqrt(mag2);
      kx = m * std::cos(a);
      ky = m * std::sin(a);
    }
  }
  bool on_ground = grounded && ky <= 0 && st.floor && std::fabs(x) <= st.edge;
  if (on_ground) ky = 0;
  float vy = 0, prev_y = y;
  p.n = 0; p.ko_at = -1; p.margin = 1e9f;
  p.x[p.n] = x; p.y[p.n] = y; ++p.n;
  for (int f = 1; f <= MAX_FRAMES; ++f) {
    if (on_ground) {
      if (kx != 0) {
        float s = std::fabs(kx) - b.traction;
        kx = s > 0 ? std::copysign(s, kx) : 0.0f;
      }
      x += kx;
      if (std::fabs(x) > st.edge) on_ground = false;
    } else {
      if (kx != 0 || ky != 0) {
        float m = std::sqrt(kx * kx + ky * ky);
        if (m < KB_DECAY) kx = ky = 0;
        else { kx -= KB_DECAY * kx / m; ky -= KB_DECAY * ky / m; }
      }
      vy = std::fmax(vy - b.gravity, -b.fall);
      x += kx;
      y += ky + vy;
      if (st.floor && y < 0 && prev_y >= 0 && std::fabs(x) <= st.edge) {
        y = 0; vy = 0; if (ky < 0) ky = 0;
        on_ground = true;
      }
    }
    prev_y = y;
    p.x[p.n] = x; p.y[p.n] = y; ++p.n;
    bool carried = f < stun || kx != 0 || ky != 0;
    bool out = x < st.left || x > st.right || y > st.top || y < st.bottom;
    if (out) {
      if (carried) p.ko_at = p.n - 1;
      return;
    }
    if (carried) {
      float d = std::fmin(std::fmin(x - st.left, st.right - x), std::fmin(st.top - y, y - st.bottom));
      if (d < p.margin) p.margin = d;
    } else if (on_ground || vy <= -b.fall + 1e-6f) {
      return;
    }
  }
}

// The full-tilt stick direction that survives with the most room (or dies last).
void best_di(float x, float y, float kx, float ky, bool grounded, float stun, const Body& b, const Stage& st,
             Path& best, float& bx, float& by) {
  static Path trial;
  bool have = false;
  for (int i = 0; i < BEST_SAMPLES; ++i) {
    float a = 6.2831853f * (float)i / BEST_SAMPLES, sx = std::cos(a), sy = std::sin(a);
    fly(x, y, kx, ky, sx, sy, grounded, stun, b, st, trial);
    bool better;
    if (!have) better = true;
    else if ((trial.ko_at < 0) != (best.ko_at < 0)) better = trial.ko_at < 0;
    else if (trial.ko_at < 0) better = trial.margin > best.margin;
    else better = trial.ko_at > best.ko_at;
    if (better) { best = trial; bx = sx; by = sy; have = true; }
  }
}

// ---------------------------------------------------------------- drawing

void segment(float x0, float y0, float x1, float y1, float w, uint32_t rgba) {
  if (PP_HOST_HAS(H, hud_capsule)) H->hud_capsule(x0, y0, w, x1, y1, w, rgba, 1);
  else H->hud_circle(x1, y1, w, rgba, 1);
}

void draw_path(const pp_camera& cam, const Path& p, uint32_t rgb, float alpha, float width) {
  float px = 0, py = 0;
  bool have = false;
  for (int i = 0; i < p.n; ++i) {
    float sx, sy;
    if (!pp_project(&cam, p.x[i], p.y[i], 0, &sx, &sy, nullptr)) { have = false; continue; }
    if (have) segment(px, py, sx, sy, width, pp_rgba(rgb, alpha));
    px = sx; py = sy; have = true;
  }
  float sx, sy;
  if (p.ko_at >= 0 && pp_project(&cam, p.x[p.ko_at], p.y[p.ko_at], 0, &sx, &sy, nullptr)) {
    const float r = 6;
    segment(sx - r, sy - r, sx + r, sy + r, 1.6f, pp_rgba(0xFF3B3B, alpha));
    segment(sx - r, sy + r, sx + r, sy - r, 1.6f, pp_rgba(0xFF3B3B, alpha));
  }
}

// ---------------------------------------------------------------- per fighter

struct Track {
  bool live = false;     // a prediction is on screen
  int since_exit = -1;   // frames since hitlag ended (-1: still in hitlag)
  float stun = 0;
  Path none, yours, best;
  float best_x = 0, best_y = 0, stick_x = 0, stick_y = 0;
};
Track tracks[4];

bool on(const char* k) { return H->setting_number(ID, k) > 0.5; }

void update(int port, uint32_t fp, const Stage& st) {
  Track& t = tracks[port];
  uint32_t state = H->rd32(fp + PP_FT_STATE);
  float hitlag = pp_hitlag(H, fp);
  float kx = H->rdf32(fp + PP_FT_KB_VEL), ky = H->rdf32(fp + PP_FT_KB_VEL + 4);
  if (hitlag > 0 && pp_state_is_damage(state) && (kx != 0 || ky != 0)) {
    float x = H->rdf32(fp + PP_FT_POS), y = H->rdf32(fp + PP_FT_POS + 4);
    Body b{H->rdf32(fp + PP_FT_GRAVITY), H->rdf32(fp + PP_FT_FALL_SPEED), H->rdf32(fp + PP_FT_TRACTION)};
    float kb = H->rdf32(fp + PP_FT_KB_APPLIED);
    if (!(kb > 0)) kb = std::sqrt(kx * kx + ky * ky) / KB_TO_SPEED;
    bool grounded = H->rd32(fp + PP_FT_AIRBORNE) == 0;
    t.stun = kb * HITSTUN_PER_KB;
    t.stick_x = H->rdf32(fp + PP_FT_STICK);
    t.stick_y = H->rdf32(fp + PP_FT_STICK + 4);
    fly(x, y, kx, ky, 0, 0, grounded, t.stun, b, st, t.none);
    fly(x, y, kx, ky, t.stick_x, t.stick_y, grounded, t.stun, b, st, t.yours);
    best_di(x, y, kx, ky, grounded, t.stun, b, st, t.best, t.best_x, t.best_y);
    t.live = true;
    t.since_exit = -1;
    return;
  }
  if (!t.live) return;
  if (t.since_exit < 0) t.since_exit = 0;
  ++t.since_exit;
  bool flying = pp_state_is_damage(state) && t.since_exit < t.stun + 30;
  if (!on("keep") || !flying) t.live = false;
}

void draw(int port, uint32_t fp, const pp_camera& cam) {
  const Track& t = tracks[port];
  if (!t.live) return;
  bool frozen = t.since_exit < 0;
  float a = frozen ? 0.95f : 0.55f;
  if (on("show_none")) draw_path(cam, t.none, 0xC8CCD8, a * 0.8f, 1.2f);
  if (on("show_best")) draw_path(cam, t.best, 0x5BD68A, a, 1.5f);
  draw_path(cam, t.yours, 0xF2D25B, a, 2.0f);
  if (!frozen) return;
  // A stick gauge beside the fighter: the held stick and the best DI.
  float fx, fy;
  if (!pp_project(&cam, H->rdf32(fp + PP_FT_POS), H->rdf32(fp + PP_FT_POS + 4), 0, &fx, &fy, nullptr)) return;
  float gx = fx + 34, gy = fy - 34, r = 13;
  H->hud_circle(gx, gy, r + 3, pp_rgba(0x0B1030, 0.65f), 1);
  H->hud_circle(gx, gy, r, pp_rgba(0xFFFFFF, 0.5f), 0);
  if (on("show_best")) H->hud_circle(gx + t.best_x * r, gy - t.best_y * r, 3.2f, pp_rgba(0x5BD68A, 1), 1);
  H->hud_circle(gx + t.stick_x * r, gy - t.stick_y * r, 2.6f, pp_rgba(0xF2D25B, 1), 1);
  const char* verdict = t.yours.ko_at >= 0 ? (t.best.ko_at >= 0 ? "KO whatever the DI" : "KO: DI green to live")
                                           : (t.none.ko_at >= 0 ? "Your DI saves you" : "Survives");
  uint32_t col = t.yours.ko_at >= 0 ? 0xFF6B6B : 0xF2D25B;
  char line[64];
  std::snprintf(line, sizeof line, "%s  (%d hitstun)", verdict, (int)t.stun);
  H->hud_label(gx, gy + r + 6, pp_rgba(col, 1), 11, 1, line);
}

bool was_down = false, shown = true;

void frame(void*) {
  int vk = (int)H->setting_number(ID, "toggle_key");
  bool down = vk > 0 && H->key_down(vk);
  if (down && !was_down) {
    shown = !shown;
    H->toast(ID, shown ? "DI paths on" : "DI paths off");
  }
  was_down = down;
  if (!pp_in_match(H)) {
    for (Track& t : tracks) t.live = false;
    return;
  }
  Stage st;
  if (!read_stage(st)) return;
  pp_camera cam;
  bool can_draw = shown && pp_camera_read(H, &cam);
  for (int port = 0; port < 4; ++port) {
    uint32_t fp = pp_fighter(H, port);
    if (!fp) { tracks[port].live = false; continue; }
    update(port, fp, st);
    if (can_draw) draw(port, fp, cam);
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
  H->declare_setting(ID, R"({"key":"show_none","type":"bool","label":"Path with no DI","default":true})");
  H->declare_setting(ID, R"({"key":"show_best","type":"bool","label":"Best survival DI","default":true})");
  H->declare_setting(ID, R"({"key":"keep","type":"bool","label":"Keep the path on screen while flying","default":true})");
  H->declare_setting(ID, R"({"key":"toggle_key","type":"key","label":"Show / hide","default":"F10"})");
  H->set_status(ID, "During hitlag: grey no DI, yellow your DI, green the best survival DI. F10 shows or hides them.");
  H->on_frame(frame, nullptr);
  return 0;
}
