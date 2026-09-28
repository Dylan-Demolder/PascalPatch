// tech-trainer: feedback on the timing-heavy techniques, the moment you do them (PascalPatch
// native plugin, read-only).
//
//   L-cancel   every aerial landing: cancelled, or how many frames early or late L/R/Z came
//   Hops       short hop or full hop, off every jump
//   Wavedash   how many frames after jumpsquat the air dodge came, and its angle
//   Techs      teched, or missed and by how much
//   Out of shield  the first action after shield stun and how many frames it took (1 = no delay)
//   Powershield    a hit during the powershield window (GuardReflect, entered by a digital L/R press),
//                  or how early the press came
//   Ledgedash  intangibility left once the landing lag ends ("GALINT"), off a ledge drop
//   SDI        smash DI inputs during hitlag and how far they moved you
//   Fastfall   how many frames after the peak of the jump the fastfall came (1 = the first frame)
//
// Each result shows as an outlined line above the damage meters and adds to a running tally
// (L-cancel 14/17, 82%). The tally resets with a hotkey or at the start of each match.
// Needs PascalPatch 0.3.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* ID = "tech-trainer";
constexpr int LCANCEL_WINDOW = 7;     // L/R/Z within 7 frames before landing halves the lag
constexpr int TECH_WINDOW = 20;       // L/R within 20 frames before hitting the ground techs
constexpr int LATE_WATCH = 12;        // how long after a missed landing a late press still counts
constexpr int SHOW = 150;             // frames a result stays up
constexpr uint32_t GOOD = PP_RGB_GOOD, BAD = PP_RGB_BAD, INFO = PP_RGB_TEXT, WARN = PP_RGB_WARN;
constexpr int POWERSHIELD_FRAMES = 8;  // GuardReflect lasts 8 frames: a hit in them is a powershield
constexpr int OOS_WATCH = 30;         // frames after shield stun to wait for an action
constexpr int LEDGE_WATCH = 60;       // frames after letting go of the ledge to wait for the landing
constexpr int ESCAPE_AIR_LAG = 10;    // LandingFallSpecial after an air dodge (PlCo 0x344)

const pp_host* H = nullptr;

struct Line { char text[96] = ""; uint32_t rgb = INFO; int age = SHOW; };
struct Tally { int ok = 0, all = 0; };

struct Player {
  uint32_t fp = 0, state = 0, prev_state = 0;
  bool air = false, prev_air = false;
  // L-cancel: a landing that missed the window, still waiting to see whether a late press comes
  int late_watch = -1;
  // wavedash: frames since jumpsquat ended, while watching for an air dodge
  int since_jump = -1;
  bool jump_short = false;
  float stick_x = 0, stick_y = 0;
  Line line[3];      // newest first
  Line hop;          // the last jump, on a quieter line of its own
  Tally lc, wd, tech, sh, ps, ld;
  // out of shield: frames since shield stun ended (-1 not watching); jumpsquat out of shield waits
  // to see whether it becomes a jump or a jump-cancelled move
  int oos = -1, oos_squat = -1;
  int reflect = -1;          // frames since the powershield window (GuardReflect) opened
  // ledgedash: frames since letting go of the ledge, and when the jump and the air dodge came
  int ledge = -1, ledge_jump = -1, ledge_dodge = -1;
  // SDI: hitlag frames so far, inputs (frames the fighter moved while frozen) and distance
  int hitlag = 0, sdi = 0;
  float sdi_dist = 0, last_x = 0, last_y = 0;
  bool sdi_shield = false;   // the hitlag is a shield's
  // fastfall: frames since the jump's peak (-1 rising or grounded)
  int falling = -1;
  bool fastfell = false;
};

Player pl[4];
bool was_in_match = false;

void say(Player& p, uint32_t rgb, const char* fmt, ...) {
  for (int i = 2; i > 0; --i) p.line[i] = p.line[i - 1];
  Line& l = p.line[0];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(l.text, sizeof l.text, fmt, ap);
  va_end(ap);
  l.rgb = rgb;
  l.age = 0;
  char msg[120];
  std::snprintf(msg, sizeof msg, "P%d %s", (int)(&p - pl) + 1, l.text);
  H->log(ID, msg);
}

bool on(const char* key) { return H->setting_number(ID, key) > 0.5; }

bool pressed_lrz(uint32_t fp) { return (H->rd32(fp + PP_FT_PRESSED) & (PP_BTN_L | PP_BTN_R | PP_BTN_Z)) != 0; }
bool pressed_lr(uint32_t fp) { return (H->rd32(fp + PP_FT_PRESSED) & (PP_BTN_L | PP_BTN_R)) != 0; }
const char* plural(int n) { return n == 1 ? "" : "s"; }
bool in_shield(uint32_t s) { return s == PP_ST_GUARD_ON || s == PP_ST_GUARD || s == PP_ST_GUARD_SET_OFF || s == PP_ST_GUARD_REFLECT; }

// What a player did, in words: "Jump", "Roll", or the move's name.
const char* action_name(uint32_t kind, uint32_t s) {
  if (s == PP_ST_KNEE_BEND || s == PP_ST_JUMP_F || s == PP_ST_JUMP_F + 1) return "Jump";
  if (s == PP_ST_ESCAPE_F || s == PP_ST_ESCAPE_B) return "Roll";
  if (s == PP_ST_ESCAPE) return "Spot dodge";
  if (s == PP_ST_GUARD_OFF) return "Shield drop";
  return pp_move_name(kind, s);
}

void out_of_shield(Player& p, uint32_t kind, uint32_t s, int frame, bool jc) {
  if (s == PP_ST_GUARD_OFF) {
    say(p, frame <= 1 ? INFO : WARN, "Out of shield: shield drop on frame %d (15 frames of lag)", frame);
    return;
  }
  say(p, frame <= 1 ? GOOD : frame <= 3 ? WARN : BAD, "Out of shield: %s%s on frame %d%s", action_name(kind, s),
      jc ? " (jump-cancelled)" : "", frame, frame <= 1 ? " (no delay)" : "");
}

void techniques(Player& p, uint32_t kind) {
  const uint32_t fp = p.fp, s = p.state, prev = p.prev_state;
  const bool entered = s != prev;

  // ---- powershield: the hit lands while the window from a digital L/R press is open ----
  if (on("powershield")) {
    if (entered && s == PP_ST_GUARD_REFLECT) p.reflect = 0;
    else if (p.reflect >= 0 && ++p.reflect > POWERSHIELD_FRAMES + 12) p.reflect = -1;   // then it is just a shield
    if (entered && s == PP_ST_GUARD_SET_OFF) {
      if (prev == PP_ST_GUARD_REFLECT) {
        ++p.ps.all;
        ++p.ps.ok;
        say(p, GOOD, "Powershield! (frame %d of %d)  %d/%d", p.reflect + 1, POWERSHIELD_FRAMES, p.ps.ok, p.ps.all);
      } else if (p.reflect >= 0) {
        ++p.ps.all;
        int early = p.reflect + 1 - POWERSHIELD_FRAMES;
        say(p, BAD, "Powershield: L/R %d frame%s too early  %d/%d", early, plural(early), p.ps.ok, p.ps.all);
      }   // otherwise an ordinary shield, not a powershield attempt
      p.reflect = -1;
    }
  }

  // ---- out of shield: the first action once shield stun ends ----
  if (on("oos")) {
    if (prev == PP_ST_GUARD_SET_OFF && entered) { p.oos = 0; p.oos_squat = -1; }
    if (p.oos >= 0) {
      ++p.oos;
      if (p.oos_squat >= 0) {   // jumpsquat out of shield: a jump, or a jump-cancelled move
        if (entered && s != PP_ST_KNEE_BEND) {
          out_of_shield(p, kind, s, p.oos_squat, s != PP_ST_JUMP_F && s != PP_ST_JUMP_F + 1);
          p.oos = p.oos_squat = -1;
        }
      } else if (!in_shield(s)) {
        if (s == PP_ST_KNEE_BEND) p.oos_squat = p.oos;
        else { out_of_shield(p, kind, s, p.oos, false); p.oos = -1; }
      } else if (s == PP_ST_GUARD_SET_OFF || p.oos > OOS_WATCH) {
        p.oos = -1;   // hit again, or just held shield
      }
    }
  }

  // ---- ledgedash: let go of the ledge, jump, air dodge onto the stage ----
  if (on("ledgedash")) {
    if (prev == PP_ST_CLIFF_WAIT && entered && !pp_state_is_damage(s)) { p.ledge = 0; p.ledge_jump = p.ledge_dodge = -1; }
    if (p.ledge >= 0) {
      ++p.ledge;
      if (entered && (s == PP_ST_JUMP_AERIAL_F || s == PP_ST_JUMP_AERIAL_F + 1)) p.ledge_jump = p.ledge;
      if (entered && s == PP_ST_ESCAPE_AIR) p.ledge_dodge = p.ledge;
      bool landed = entered && (s == PP_ST_LANDING_FALL_SPECIAL || s == PP_ST_LANDING);
      if (landed && p.ledge_jump > 0) {
        int intang = (int)H->rd32(fp + PP_FT_INTANGIBLE);
        int lag = s == PP_ST_LANDING_FALL_SPECIAL ? ESCAPE_AIR_LAG : (int)H->rdf32(fp + PP_FT_LANDING_LAG);
        int galint = intang - lag;
        char how[40];
        if (p.ledge_dodge > 0)
          std::snprintf(how, sizeof how, "jump f%d, dodge f%d", p.ledge_jump, p.ledge_dodge);
        else
          std::snprintf(how, sizeof how, "jump f%d, no air dodge", p.ledge_jump);
        ++p.ld.all;
        if (galint > 0) {
          ++p.ld.ok;
          say(p, GOOD, "Ledgedash: %d frame%s intangible after the lag (%s)  %d/%d", galint, plural(galint), how, p.ld.ok, p.ld.all);
        } else {
          // no intangibility left while the landing lag still holds you: -galint frames open to a hit
          say(p, BAD, "Ledgedash: open to hits for %d frame%s of the landing (%s)  %d/%d", -galint, plural(-galint), how, p.ld.ok, p.ld.all);
        }
        p.ledge = -1;
      } else if (p.ledge > LEDGE_WATCH || s == PP_ST_CLIFF_WAIT || pp_state_is_damage(s) || (landed && p.ledge_jump < 0)) {
        p.ledge = -1;
      }
    }
  }

  // ---- SDI: count the frames a fighter frozen in hitlag moves ----
  if (on("sdi")) {
    float x = H->rdf32(fp + PP_FT_POS), y = H->rdf32(fp + PP_FT_POS + 4);
    bool frozen = H->rdf32(fp + PP_FT_HITLAG) > 0 && (pp_state_is_damage(s) || s == PP_ST_GUARD_SET_OFF);
    if (frozen && p.hitlag == 0) p.sdi_shield = s == PP_ST_GUARD_SET_OFF;
    if (frozen) {
      if (p.hitlag > 0) {
        float d = std::hypot(x - p.last_x, y - p.last_y);
        if (d > 0.01f) { ++p.sdi; p.sdi_dist += d; }
      }
      ++p.hitlag;
    } else if (p.hitlag > 0) {
      if (p.sdi)
        say(p, GOOD, "SDI: %d input%s in %d frames of hitlag, moved %.1f", p.sdi, plural(p.sdi), p.hitlag, p.sdi_dist);
      else if (p.hitlag >= 6 && !p.sdi_shield)
        say(p, WARN, "No SDI: %d frames of hitlag to wiggle in", p.hitlag);
      p.hitlag = p.sdi = 0;
      p.sdi_dist = 0;
    }
    p.last_x = x;
    p.last_y = y;
  }

  // ---- fastfall: frames from the peak of the jump to the fastfall ----
  if (on("fastfall")) {
    bool air = H->rd32(fp + PP_FT_AIRBORNE) != 0 && !pp_state_is_damage(s) && s != PP_ST_ESCAPE_AIR;
    float vy = H->rdf32(fp + PP_FT_SELF_VEL + 4), ff = H->rdf32(fp + PP_FT_FASTFALL_SPEED);
    if (!air || vy > 0) {
      p.falling = -1;
      p.fastfell = false;
    } else if (!p.fastfell) {
      ++p.falling;   // 0 on the first frame falling
      if (ff > 0 && std::fabs(vy + ff) < 0.001f && p.falling > 0) {
        int f = p.falling;
        say(p, f <= 1 ? GOOD : f <= 3 ? INFO : WARN, "Fastfall: frame %d after the peak%s", f, f <= 1 ? " (earliest)" : "");
        p.fastfell = true;
      }
    }
  }
}

void step(Player& p) {
  const uint32_t fp = p.fp, s = p.state, prev = p.prev_state;
  const bool entered = s != prev;
  const uint32_t kind = H->rd32(fp + PP_FT_KIND);

  // ---- L-cancel ----
  if (on("lcancel")) {
    if (entered && pp_state_is_aerial_landing(s)) {
      unsigned since = H->rd8(fp + PP_FT_SINCE_LR);   // frames since L/R/Z, counted before this landing
      // the game's own rule; the landing animation's speed differs per move, so it proves nothing
      bool cancelled = since < LCANCEL_WINDOW;
      const char* move = pp_move_name(kind, s);
      ++p.lc.all;
      if (cancelled) {
        ++p.lc.ok;
        say(p, GOOD, "%s L-cancelled (%u/7)  %d/%d", move, since + 1, p.lc.ok, p.lc.all);
      } else if (since < 40) {
        say(p, BAD, "%s: L %d frame%s early  %d/%d", move, since + 1 - LCANCEL_WINDOW, since + 1 - LCANCEL_WINDOW == 1 ? "" : "s", p.lc.ok, p.lc.all);
      } else {
        p.late_watch = 0;   // no press yet: wait a few frames to see if it comes late
      }
    } else if (p.late_watch >= 0) {
      ++p.late_watch;
      if (pressed_lrz(fp)) {
        say(p, BAD, "L-cancel: %d frame%s late  %d/%d", p.late_watch, p.late_watch == 1 ? "" : "s", p.lc.ok, p.lc.all);
        p.late_watch = -1;
      } else if (p.late_watch > LATE_WATCH || !pp_state_is_aerial_landing(s)) {
        say(p, BAD, "Missed L-cancel  %d/%d", p.lc.ok, p.lc.all);
        p.late_watch = -1;
      }
    }
  }

  // ---- hops and wavedashes: both start when jumpsquat (KneeBend) ends ----
  if (prev == PP_ST_KNEE_BEND && entered) {
    if (s == PP_ST_JUMP_F || s == PP_ST_JUMP_F + 1) {           // JumpF / JumpB
      if (on("hops")) {
        // jump_short was read from KneeBend's is_short_hop while the jumpsquat lasted
        ++p.sh.all;
        if (p.jump_short) ++p.sh.ok;
        std::snprintf(p.hop.text, sizeof p.hop.text, "%s", p.jump_short ? "Short hop" : "Full hop");
        p.hop.age = 0;
      }
      p.since_jump = 0;
    } else if ((s == PP_ST_ESCAPE_AIR || s == PP_ST_LANDING_FALL_SPECIAL) && on("wavedash")) {
      p.since_jump = 0;   // air dodge on the first airborne frame (straight to the landing when it is low)
    }
  }
  if (s == PP_ST_KNEE_BEND) p.jump_short = H->rd32(fp + PP_FT_SHORT_HOP) != 0;

  if (on("wavedash") && p.since_jump >= 0) {
    if (entered && (s == PP_ST_ESCAPE_AIR || s == PP_ST_LANDING_FALL_SPECIAL)) {
      // air dodge out of a jump: frame 1 is the first airborne frame (a perfect wavedash)
      int f = p.since_jump + 1;
      float sx = H->rdf32(fp + PP_FT_STICK), sy = H->rdf32(fp + PP_FT_STICK + 4);
      float deg = std::atan2(-sy, std::fabs(sx)) * 57.2958f;     // degrees below horizontal
      ++p.wd.all;
      bool good = f <= 2 && sy < -0.2f;
      if (good) ++p.wd.ok;
      if (sy >= -0.2f)
        say(p, WARN, "Air dodge on jump frame %d (stick not down: no wavedash)", f);
      else
        say(p, good ? GOOD : BAD, "Wavedash: frame %d%s, %.0f deg  %d/%d", f, f == 1 ? " (perfect)" : "", deg, p.wd.ok, p.wd.all);
      p.since_jump = -1;
    } else if (++p.since_jump > 8 || !(s == PP_ST_JUMP_F || s == PP_ST_JUMP_F + 1 || s == PP_ST_ESCAPE_AIR)) {
      p.since_jump = -1;
    }
  }

  techniques(p, kind);

  // ---- techs: hitting the ground in tumble ----
  if (on("techs") && entered) {
    bool teched = s >= PP_ST_PASSIVE && s <= PP_ST_PASSIVE_CEIL;
    bool missed = s == PP_ST_DOWN_BOUND_U || s == PP_ST_DOWN_BOUND_D;
    if (teched || missed) {
      ++p.tech.all;
      if (teched) {
        ++p.tech.ok;
        static const char* const kinds[] = {"in place", "forward", "back", "on the wall", "wall jump", "on the ceiling"};
        say(p, GOOD, "Teched %s  %d/%d", kinds[s - PP_ST_PASSIVE], p.tech.ok, p.tech.all);
      } else {
        unsigned since = H->rd8(fp + PP_FT_SINCE_LR);
        if (since < 60 && since >= TECH_WINDOW)
          say(p, BAD, "Missed tech: L/R %u frames early  %d/%d", since + 1 - TECH_WINDOW, p.tech.ok, p.tech.all);
        else
          say(p, BAD, "Missed tech  %d/%d", p.tech.ok, p.tech.all);
      }
    }
  }
}

void draw(int port, Player& p, float op) {
  // lines stack upward from just above the damage meters, on this port's side
  float x = port % 2 == 0 ? 20.0f : 620.0f;
  int align = port % 2 == 0 ? 0 : 2;
  float y = 360 - (float)(port / 2) * 60;
  for (int i = 0; i < 3; ++i) {
    Line& l = p.line[i];
    if (l.age >= SHOW || !l.text[0]) continue;
    float a = (l.age < SHOW - 30 ? 1.0f : (float)(SHOW - l.age) / 30.0f) * (i == 0 ? 1.0f : 0.6f);
    H->hud_label(x, y - (float)i * 18, pp_rgba(l.rgb, op * a), i == 0 ? 17.0f : 14.0f, align, l.text);
  }
  if (p.hop.age < 60) H->hud_label(x, y - 3 * 18, pp_rgba(0x9FB4FF, op * 0.9f), 13, align, p.hop.text);
}

// A spot for a panel: stacked in its corner with other plugins' panels on PascalPatch 0.5
// (hud_place), or the fixed spot given on older runtimes.
void place(int corner, float w, float h, float fixed_x, float fixed_y, float& x, float& y) {
  if (PP_HOST_HAS(H, hud_place)) H->hud_place(corner, w, h, &x, &y);
  else { x = fixed_x; y = fixed_y; }
}

void draw_tally(float op) {
  // one compact box on the left for the focused port
  int port = (int)H->setting_number(ID, "port");
  if (port < 0 || port > 3 || !pl[port].fp) return;
  Player& p = pl[port];
  char row[6][40];
  int n = 0;
  auto pct = [](const Tally& t) { return t.all ? (100 * t.ok + t.all / 2) / t.all : 0; };
  if (on("lcancel")) std::snprintf(row[n++], 40, "L-cancel  %d/%d  %d%%", p.lc.ok, p.lc.all, pct(p.lc));
  if (on("hops")) std::snprintf(row[n++], 40, "Short hops  %d/%d", p.sh.ok, p.sh.all);
  if (on("wavedash")) std::snprintf(row[n++], 40, "Wavedash  %d/%d  %d%%", p.wd.ok, p.wd.all, pct(p.wd));
  if (on("techs")) std::snprintf(row[n++], 40, "Techs  %d/%d  %d%%", p.tech.ok, p.tech.all, pct(p.tech));
  if (on("powershield") && p.ps.all) std::snprintf(row[n++], 40, "Powershields  %d/%d", p.ps.ok, p.ps.all);
  if (on("ledgedash") && p.ld.all) std::snprintf(row[n++], 40, "Ledgedashes  %d/%d", p.ld.ok, p.ld.all);
  if (!n) return;
  // below where Frame Data puts port 1's panel, so the two can run together
  float x0, y0, w = 150, h = 22 + 14.0f * (float)n;
  place(PP_CORNER_TOP_LEFT, w, h, 8, 100, x0, y0);
  H->hud_rect(x0, y0, x0 + w, y0 + h, pp_rgba(0x111838, 0.78f * op), 6, 1);
  H->hud_rect(x0, y0, x0 + 3, y0 + h, pp_rgba(pp_port_rgb[port], op), 1, 1);
  char title[24];
  std::snprintf(title, sizeof title, "P%d  Tech trainer", port + 1);
  H->hud_text(x0 + 9, y0 + 4, pp_rgba(0x9FB4FF, op), 12, title);
  for (int i = 0; i < n; ++i) H->hud_text(x0 + 9, y0 + 19 + 14.0f * (float)i, pp_rgba(0xF2F4FA, op), 12, row[i]);
}

void reset() {
  for (auto& p : pl) { p.lc = p.wd = p.tech = p.sh = p.ps = p.ld = Tally{}; for (auto& l : p.line) l = Line{}; p.hop = Line{}; }
}

void frame(void*) {
  if (!pp_in_match(H)) { was_in_match = false; return; }
  if (!was_in_match) { reset(); for (auto& p : pl) { p.fp = 0; p.late_watch = p.since_jump = -1; } }
  was_in_match = true;

  static bool key_was_down = false;
  int vk = (int)H->setting_number(ID, "reset_key");
  bool key_down = vk > 0 && H->key_down(vk);
  if (key_down && !key_was_down) { reset(); H->toast(ID, "Tech trainer: tally reset"); }
  key_was_down = key_down;

  int which = (int)H->setting_number(ID, "watch");  // 0 the focused port, 1 every human player
  int focus = (int)H->setting_number(ID, "port");
  float op = (float)H->setting_number(ID, "opacity");
  if (op <= 0) op = 0.95f;
  for (int i = 0; i < 4; ++i) {
    Player& p = pl[i];
    uint32_t fp = pp_fighter(H, i);
    bool watched = fp && (which == 1 ? pp_player_type(H, i) == 0 : i == focus);
    if (!watched) { p.fp = 0; continue; }
    if (fp != p.fp) {
      p.fp = fp;
      p.prev_state = p.state = H->rd32(fp + PP_FT_STATE);
      p.late_watch = p.since_jump = p.oos = p.oos_squat = p.reflect = p.ledge = p.falling = -1;
      p.hitlag = p.sdi = 0;
    }
    p.state = H->rd32(fp + PP_FT_STATE);
    step(p);
    p.prev_state = p.state;
    for (auto& l : p.line) if (l.age < SHOW) ++l.age;
    if (p.hop.age < SHOW) ++p.hop.age;
    draw(i, p, op);
  }
  if (on("tally")) draw_tally(op);
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
  H->declare_setting(ID, R"({"key":"lcancel","type":"bool","label":"L-cancels","default":true})");
  H->declare_setting(ID, R"({"key":"hops","type":"bool","label":"Short hops and full hops","default":true})");
  H->declare_setting(ID, R"({"key":"wavedash","type":"bool","label":"Wavedashes","default":true})");
  H->declare_setting(ID, R"({"key":"techs","type":"bool","label":"Techs","default":true})");
  H->declare_setting(ID, R"({"key":"oos","type":"bool","label":"Out of shield timing","default":true})");
  H->declare_setting(ID, R"({"key":"powershield","type":"bool","label":"Powershields","default":true})");
  H->declare_setting(ID, R"({"key":"ledgedash","type":"bool","label":"Ledgedashes","default":true})");
  H->declare_setting(ID, R"({"key":"sdi","type":"bool","label":"SDI during hitlag","default":true})");
  H->declare_setting(ID, R"({"key":"fastfall","type":"bool","label":"Fastfall timing","default":false})");
  H->declare_setting(ID, R"({"key":"tally","type":"bool","label":"Success tally in the corner","default":true})");
  H->declare_setting(ID, R"({"key":"reset_key","type":"key","label":"Reset the tally","default":"F8"})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.95,"min":0.3,"max":1.0})");
  H->set_status(ID, "Coaching L-cancels, hops, wavedashes, techs, shields, ledgedashes, SDI and fastfalls.");
  H->on_frame(frame, nullptr);
  return 0;
}
