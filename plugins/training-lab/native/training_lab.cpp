// training-lab: practice tools for any match (PascalPatch native plugin).
//
//   Pause / frame advance   hold the game on a frame and step it one frame at a time
//   Slow motion             half or quarter speed, toggled with a key
//   Percent                 reset to 0% with a key, or lock a player at a set percent
//   Infinite shield         shields never shrink or break
//   Endless stocks          nobody runs out of stocks, so a practice match never ends
//   Dummy (PascalPatch 0.5) a practice partner on a human port: it stands, crouches, shields or
//                           jumps; DIs, SDIs, techs and gets up the way you set; answers your hits
//                           and shield pressure with an action; and plays back inputs you record
//
// Pause and slow motion hold the simulation thread inside the frame callback, which the port
// tolerates: its pacer resumes at 60 Hz afterwards. Only the game stops; the window, the F2
// overlay and the other plugins' HUD stay live. Writes game memory (percent, shield, stocks) only
// while those options are on. The dummy plays through the port's controller (pad_set), so it is
// held to the game's own rules; only teching is done by marking a well-timed L/R press in the
// fighter's tech timer, since when a tumble ends depends on stage collision.
// Needs PascalPatch 0.3 (hotkeys and toasts); the dummy needs 0.5.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* ID = "training-lab";

const pp_host* H = nullptr;

struct Key {
  const char* setting;
  bool was = false;
  bool pressed() {   // a fresh press this frame
    int vk = (int)H->setting_number(ID, setting);
    bool down = vk > 0 && H->key_down(vk);
    bool edge = down && !was;
    was = down;
    return edge;
  }
};

Key k_pause{"pause_key"}, k_step{"step_key"}, k_slow{"slow_key"}, k_reset{"reset_key"};
Key k_dummy{"dummy_key"}, k_record{"record_key"}, k_play{"play_key"}, k_save{"save_key"}, k_load{"load_key"};
bool paused = false, slow = false;
bool shown = false;   // the PAUSED badge has been on screen for a frame (the HUD is published when a frame ends)

bool on(const char* k) { return H->setting_number(ID, k) > 0.5; }
int choice(const char* k) { return (int)H->setting_number(ID, k); }

bool port_selected(int port) {
  int who = choice("who");   // 0 P1, 1 P2, 2 everyone, 3 CPUs only
  if (who == 2) return true;
  if (who == 3) return pp_player_type(H, port) == 1;
  return port == who;
}

void set_percent(uint32_t fp, int port, float pct) {
  H->wrf32(fp + PP_FT_PERCENT, pct);
  H->wr16(pp_player_block(port) + PP_PL_DAMAGE, (uint16_t)(pct + 0.5f));
}

void badge(const char* text, uint32_t rgb) {
  H->hud_rect(262, 322, 378, 344, pp_rgba(0x111838, 0.8f), 6, 1);
  H->hud_label(320, 325, pp_rgba(rgb, 1.0f), 16, 1, text);
}

// ---------------------------------------------------------------- the dummy


enum Stance { STANCE_STAND, STANCE_CROUCH, STANCE_SHIELD, STANCE_JUMP };
enum Di { DI_NONE, DI_SURVIVAL, DI_COMBO, DI_IN, DI_OUT, DI_RANDOM };
enum Sdi { SDI_NONE, SDI_SOME, SDI_MAX };
enum Tech { TECH_MISS, TECH_IN_PLACE, TECH_TOWARD, TECH_AWAY, TECH_RANDOM };
enum Getup { GETUP_STAND, GETUP_TOWARD, GETUP_AWAY, GETUP_ATTACK, GETUP_RANDOM };
enum Act {
  ACT_NONE, ACT_JUMP, ACT_NAIR, ACT_GRAB, ACT_SPOT_DODGE, ACT_ROLL_AWAY, ACT_ROLL_TOWARD, ACT_UP_B, ACT_DOWN_B,
  ACT_AIR_DODGE, ACT_ATTACK, ACT_RANDOM
};
const char* const ACT_NAMES[] = {"nothing", "jump", "nair", "grab", "spot dodge", "roll away", "roll in", "up-B",
                                 "down-B", "air dodge", "attack", "random"};
constexpr const char* ACTIONS = R"J([{"value":"none","label":"Nothing"},{"value":"jump","label":"Jump"},{"value":"nair","label":"Nair"},{"value":"grab","label":"Grab"},{"value":"spot_dodge","label":"Spot dodge"},{"value":"roll_away","label":"Roll away"},{"value":"roll_toward","label":"Roll in"},{"value":"up_b","label":"Up-B"},{"value":"down_b","label":"Down-B (shine)"},{"value":"air_dodge","label":"Air dodge"},{"value":"attack","label":"Attack (A)"},{"value":"random","label":"Random"}])J";
const char* const STANCE_NAMES[] = {"stand", "crouch", "shield", "jump"};
const char* const DI_NAMES[] = {"no DI", "survival DI", "combo DI", "DI in", "DI out", "random DI"};
const char* const TECH_NAMES[] = {"no tech", "tech in place", "tech in", "tech away", "random tech"};

struct Macro {
  int act = ACT_NONE;
  int t = 0;
};

constexpr int REC_MAX = 60 * 20;   // 20 seconds

struct Dummy {
  bool active = false;       // on (the key toggles it)
  bool was_in_match = false;
  int held = -1;             // the port whose controller the plugin holds, or -1
  uint32_t rng = 0x9E3779B9u;
  pp_pad_state last{};       // what was sent last frame (presses need a frame off in between)

  // reactions
  uint32_t prev_state = 0;
  float prev_hitlag = 0;
  int hitlag_frames = 0;     // frames of this hitlag seen so far
  int di = DI_NONE;          // this hit's DI (random is picked per hit)
  float di_x = 0, di_y = 0;
  int tech = TECH_MISS, getup = GETUP_STAND;
  bool after_hit = false;    // waiting to act once hitstun ends
  bool after_shield = false; // waiting to act once shield stun ends
  Macro macro;
  char last_action[48] = "";
  int last_action_age = 999;

  // recording and playback
  pp_pad_state rec[REC_MAX];
  int rec_n = 0;
  bool recording = false;
  float rec_facing = 1;
  bool playing = false;
  int play_i = 0;
  bool mirror = false;
} D;

uint32_t rnd(uint32_t n) {
  D.rng ^= D.rng << 13; D.rng ^= D.rng >> 17; D.rng ^= D.rng << 5;
  return n ? D.rng % n : 0;
}

bool has_pad() { return PP_HOST_HAS(H, pad_set); }
int dummy_port() { return choice("dummy_port"); }

void release() {
  if (D.held >= 0 && has_pad()) H->pad_release(D.held);
  D.held = -1;
  D.last = pp_pad_state{};
}

float fx(uint32_t fp) { return H->rdf32(fp + PP_FT_POS); }

// The fighter the dummy plays against: whoever hit it last, else the nearest other fighter.
uint32_t opponent(int port, uint32_t fp) {
  int src = (int)H->rd32(fp + PP_FT_HIT_SOURCE);
  if (src >= 0 && src < 4 && src != port)
    if (uint32_t o = pp_fighter(H, src)) return o;
  uint32_t best = 0;
  float bd = 1e9f;
  for (int i = 0; i < 4; ++i) {
    uint32_t o = i == port ? 0 : pp_fighter(H, i);
    if (!o) continue;
    float d = std::fabs(fx(o) - fx(fp));
    if (d < bd) { bd = d; best = o; }
  }
  return best;
}

// +1 when the opponent is to the right, -1 to the left (towards the middle of the stage if alone)
float toward(int port, uint32_t fp) {
  uint32_t o = opponent(port, fp);
  float d = o ? fx(o) - fx(fp) : -fx(fp);
  return d >= 0 ? 1.0f : -1.0f;
}

// A button pressed this frame: held only if it was up last frame, so every call makes a new press
// at most every other frame (a button held down is not pressed again).
void press(pp_pad_state& out, uint32_t btn) {
  if (!(D.last.buttons & btn)) out.buttons |= btn;
}
// The stick pushed hard from the middle (a "smash" input: rolls, spot dodges); every other frame.
void flick(pp_pad_state& out, float x, float y) {
  bool was = std::fabs(D.last.stick_x - x) < 0.01f && std::fabs(D.last.stick_y - y) < 0.01f;
  if (!was) { out.stick_x = x; out.stick_y = y; }
}
void hold_shield(pp_pad_state& out) {
  out.buttons |= PP_BTN_R;
  out.trigger_r = 1.0f;
}

// The first joint in the skeleton that carries an animation (lbFindJObjWithAObj), or 0.
uint32_t animated_joint(uint32_t jobj, int depth) {
  if (!pp_is_ptr(jobj) || depth > 64) return 0;
  if (pp_is_ptr(H->rd32(jobj + 0x7C))) return jobj;                 // HSD_JObj.aobj
  if (uint32_t j = animated_joint(H->rd32(jobj + 0x10), depth + 1)) return j;   // child
  return animated_joint(H->rd32(jobj + 0x08), depth + 1);           // next
}
// Frames left in the current animation, at its speed (-1 if unknown). GuardSetOff's animation is
// sped up to last exactly the shield stun, so this is also the shield stun left.
float anim_left(uint32_t fp) {
  uint32_t gobj = H->rd32(fp + PP_FT_GOBJ), jobj = pp_is_ptr(gobj) ? animated_joint(H->rd32(gobj + 0x28), 0) : 0;
  uint32_t aobj = jobj ? H->rd32(jobj + 0x7C) : 0;
  if (!pp_is_ptr(aobj)) return -1;
  float end = H->rdf32(aobj + 0xC), cur = H->rdf32(fp + PP_FT_ANIM_FRAME), rate = H->rdf32(fp + PP_FT_ANIM_SPEED);
  if (!(rate > 0) || !(end > 0)) return -1;
  return (end - cur) / rate;
}

// The animation ends on the next frame (it ends once its frame reaches the end): a press sent now
// lands on the first frame after it, as a perfect player's would.
bool ends_next_frame(uint32_t fp) {
  float left = anim_left(fp);
  return left >= 0 && left <= 1.0001f;
}

bool is_jump(uint32_t s) { return s >= PP_ST_KNEE_BEND && s <= PP_ST_JUMP_AERIAL_B; }

void start(int act, bool air, const char* why) {
  if (act == ACT_RANDOM) {
    static const int ground[] = {ACT_JUMP, ACT_NAIR, ACT_GRAB, ACT_SPOT_DODGE, ACT_ROLL_AWAY, ACT_ROLL_TOWARD,
                                 ACT_UP_B, ACT_DOWN_B, ACT_ATTACK};
    static const int in_air[] = {ACT_JUMP, ACT_NAIR, ACT_UP_B, ACT_DOWN_B, ACT_AIR_DODGE};
    act = air ? in_air[rnd(5)] : ground[rnd(9)];
  }
  D.macro.act = act;
  D.macro.t = 0;
  if (act != ACT_NONE) {
    std::snprintf(D.last_action, sizeof D.last_action, "%s %s", why, ACT_NAMES[act]);
    D.last_action_age = 0;
  }
}

// One frame of the running action. Each action presses until the fighter is in the state it asks
// for, so a press that lands a frame early is simply made again.
void run_macro(pp_pad_state& out, uint32_t s, bool air, float dir) {
  Macro& m = D.macro;
  bool shielding = pp_state_is_shield(s);
  bool done = false;
  switch (m.act) {
    case ACT_JUMP:
      if (is_jump(s) && m.t > 0) done = true;
      else { press(out, PP_BTN_X); if (shielding) hold_shield(out); }
      break;
    case ACT_NAIR:
      if (s == PP_ST_ATTACK_AIR_N) done = true;
      else if (!air || s == PP_ST_KNEE_BEND) { if (s != PP_ST_KNEE_BEND) press(out, PP_BTN_X); if (shielding) hold_shield(out); }
      else press(out, PP_BTN_A);
      break;
    case ACT_UP_B:
    case ACT_DOWN_B: {
      float y = m.act == ACT_UP_B ? 1.0f : -1.0f;
      if (s >= PP_STATE_COMMON && m.t > 0) done = true;
      else if (shielding) { press(out, PP_BTN_X); hold_shield(out); }   // out of shield: through jump squat
      else { out.stick_y = y; press(out, PP_BTN_B); }
      break;
    }
    case ACT_GRAB:
      if (air || (s >= PP_ST_CATCH && s <= PP_ST_CATCH_WAIT)) done = true;
      else if (shielding) { hold_shield(out); press(out, PP_BTN_A); }   // shield grab
      else press(out, PP_BTN_Z);
      break;
    case ACT_SPOT_DODGE:
    case ACT_ROLL_AWAY:
    case ACT_ROLL_TOWARD: {
      uint32_t want = m.act == ACT_SPOT_DODGE ? PP_ST_ESCAPE : 0;
      if (air) { m.act = ACT_AIR_DODGE; break; }
      if (s == want || (!want && (s == PP_ST_ESCAPE_F || s == PP_ST_ESCAPE_B))) { done = true; break; }
      hold_shield(out);
      if (shielding) {
        float x = m.act == ACT_SPOT_DODGE ? 0 : m.act == ACT_ROLL_TOWARD ? dir : -dir;
        flick(out, x, m.act == ACT_SPOT_DODGE ? -1.0f : 0.0f);
      }
      break;
    }
    case ACT_AIR_DODGE:
      if (s == PP_ST_ESCAPE_AIR || !air) done = true;
      else press(out, PP_BTN_R);
      break;
    case ACT_ATTACK:
      if ((s >= PP_ST_ATTACK_11 && s <= PP_ST_ATTACK_AIR_LW) || s >= PP_STATE_COMMON) done = true;
      else press(out, PP_BTN_A);
      break;
    default:
      done = true;
  }
  if (++m.t > 24) done = true;   // it did not take: give up rather than hold the dummy
  if (done) m.act = ACT_NONE;
}

// ---- recovering ----

// Main-floor half width of the stage (the floor is at y = 0); 0 off the tournament stages.
float stage_edge() { return pp_stage_floor_edge(pp_stage_kind(H)); }

// Off the stage and free to act: drift back, double jump towards the stage, then up-B once
// falling below the ledge. True while it is steering.
bool recover(uint32_t fp, uint32_t s, bool free_now, pp_pad_state& out) {
  float edge = stage_edge(), x = fx(fp), y = H->rdf32(fp + PP_FT_POS + 4);
  if (!on("recover") || edge <= 0 || std::fabs(x) <= edge) return false;
  float in = x > 0 ? -1.0f : 1.0f;
  out.stick_x = in;
  if (!free_now) return s >= PP_STATE_COMMON || (s >= PP_ST_FALL_SPECIAL && s <= PP_ST_FALL_SPECIAL_B);
  float vy = H->rdf32(fp + PP_FT_SELF_VEL + 4);
  int used = H->rd8(fp + PP_FT_JUMPS_USED), jumps = (int)H->rd32(fp + PP_FT_MAX_JUMPS);
  if (used < jumps && (vy <= 0 || s == PP_ST_DAMAGE_FALL)) press(out, PP_BTN_X);
  else if (used >= jumps && vy <= 0 && y < 15) {
    out.stick_x = 0.6f * in;
    out.stick_y = 0.8f;
    press(out, PP_BTN_B);
  }
  return true;
}

// The DI (and SDI) direction for this hit, from the knockback about to be applied.
void pick_di(int port, uint32_t fp) {
  D.di = choice("di");
  if (D.di == DI_RANDOM) D.di = (int)rnd(5);
  float kx = H->rdf32(fp + PP_FT_KB_VEL), ky = H->rdf32(fp + PP_FT_KB_VEL + 4);
  float len = std::sqrt(kx * kx + ky * ky), in = toward(port, fp);
  D.di_x = D.di_y = 0;
  if (D.di == DI_IN || D.di == DI_OUT || len < 1e-4f) {
    if (D.di != DI_NONE) D.di_x = D.di == DI_OUT ? -in : in;
    return;
  }
  if (D.di == DI_NONE) return;
  // the two directions square to the knockback; survival DI takes the one that turns the launch
  // back over the stage (against its sideways motion), combo DI the other
  float ax = -ky / len, ay = kx / len;
  float back = std::fabs(kx) > 0.1f * len ? -kx : -fx(fp);   // straight up: back towards the middle
  bool a_back = ax * back > 0 || (std::fabs(ax) < 1e-3f && ay > 0);
  bool use_a = D.di == DI_SURVIVAL ? a_back : !a_back;
  D.di_x = use_a ? ax : -ax;
  D.di_y = use_a ? ay : -ay;
}

void react(int port, uint32_t fp, pp_pad_state& out) {
  uint32_t s = H->rd32(fp + PP_FT_STATE);
  bool air = H->rd32(fp + PP_FT_AIRBORNE) != 0;
  float hitlag = pp_hitlag(H, fp), dir = toward(port, fp);
  bool damaged = pp_state_is_damage(s) || s == PP_ST_DAMAGE_FALL;

  // a new hit: pick this hit's DI, tech and getup, and wait for hitstun to end
  if (hitlag > 0 && pp_state_is_damage(s) && (hitlag > D.prev_hitlag + 0.5f || !pp_state_is_damage(D.prev_state))) {
    pick_di(port, fp);
    D.hitlag_frames = 0;
    D.tech = choice("tech");
    if (D.tech == TECH_RANDOM) D.tech = (int)rnd(4);
    D.getup = choice("getup");
    if (D.getup == GETUP_RANDOM) D.getup = (int)rnd(4);
    D.after_hit = true;
    D.after_shield = false;
    D.macro.act = ACT_NONE;
  }
  if (s == PP_ST_GUARD_SET_OFF && hitlag > 0) { D.after_shield = true; D.macro.act = ACT_NONE; }
  D.prev_state = s;
  D.prev_hitlag = hitlag;

  // hitlag: DI, with SDI inputs while there are frames to spare
  if (hitlag > 0 && pp_state_is_damage(s)) {
    ++D.hitlag_frames;
    out.stick_x = D.di_x;
    out.stick_y = D.di_y;
    int sdi = choice("sdi");
    if (sdi != SDI_NONE && hitlag > 2) {
      float sx = D.di_x, sy = D.di_y;
      if (sx == 0 && sy == 0) sx = -dir;   // no DI: SDI away
      int every = sdi == SDI_MAX ? 2 : 4;
      bool tap = D.hitlag_frames % every == 1 % every;
      out.stick_x = tap ? sx : 0;
      out.stick_y = tap ? sy : 0;
    }
    return;
  }
  // shield stun: keep the shield up
  // shield stun: keep the shield up; on its last frame, act (so the action comes out on the first
  // frame the shield is free, like a perfect player)
  if (s == PP_ST_GUARD_SET_OFF) {
    if (D.after_shield && hitlag <= 0 && ends_next_frame(fp)) {
      D.after_shield = false;
      start(choice("counter_shield"), air, "Out of shield:");
    }
    if (D.macro.act != ACT_NONE) { run_macro(out, s, air, dir); return; }
    hold_shield(out);
    return;
  }

  // grabbed: mash out
  if (pp_state_is_grabbed(s) && on("mash")) {
    bool odd = (D.last.stick_x <= 0);
    out.stick_x = odd ? 1.0f : -1.0f;
    press(out, PP_BTN_A);
    return;
  }

  // the counter action, as soon as the dummy can act
  float stun = pp_state_is_damage(s) ? H->rdf32(fp + PP_FT_HITSTUN) : 0;
  bool free_now = pp_state_is_actionable(s) || s == PP_ST_DAMAGE_FALL || (pp_state_is_damage(s) && stun <= 1);
  if (D.after_hit && free_now) {
    D.after_hit = false;
    start(choice("counter_hit"), air, "After the hit:");
  }
  if (D.after_shield && pp_state_is_shield(s) && s != PP_ST_GUARD_SET_OFF) {
    D.after_shield = false;
    start(choice("counter_shield"), air, "Out of shield:");
  }
  if (D.macro.act != ACT_NONE) { run_macro(out, s, air, dir); return; }
  if (air && recover(fp, s, free_now && !(pp_state_is_damage(s) && stun > 0), out)) return;

  // flying or tumbling: tech on landing (the tech timer sees a well-timed L/R that is not locked
  // out), holding the direction to tech in
  if (air && damaged && D.tech != TECH_MISS) {
    H->wr8(fp + PP_FT_SINCE_TECH, 0);
    H->wr8(fp + PP_FT_TECH_GAP, 0xFF);
    if (D.tech == TECH_TOWARD) out.stick_x = dir;
    if (D.tech == TECH_AWAY) out.stick_x = -dir;
    return;
  }

  // missed the tech: get up
  bool bounce_ends = (s == PP_ST_DOWN_BOUND_U || s == PP_ST_DOWN_BOUND_D) && ends_next_frame(fp);
  if (s == PP_ST_DOWN_WAIT_U || s == PP_ST_DOWN_WAIT_D || bounce_ends) {
    switch (D.getup) {
      case GETUP_STAND: flick(out, 0, 1); break;
      case GETUP_TOWARD: flick(out, dir, 0); break;
      case GETUP_AWAY: flick(out, -dir, 0); break;
      default: press(out, PP_BTN_A); break;
    }
    return;
  }
  if (damaged || air) return;   // nothing to hold in the air

  // standing around: the stance
  switch (choice("stance")) {
    case STANCE_CROUCH: out.stick_y = -1; break;
    case STANCE_SHIELD: hold_shield(out); break;
    case STANCE_JUMP: if (pp_state_is_actionable(s) || s == PP_ST_LANDING) press(out, PP_BTN_X); break;
    default: break;
  }
}

// ---- recording and playback ----

pp_pad_state read_pad(int port) {
  uint32_t p = PP_PAD_STATUS + PP_PAD_STRIDE * (uint32_t)port;
  pp_pad_state s{};
  s.buttons = H->rd32(p) & PP_BTN_DIGITAL;
  s.stick_x = H->rdf32(p + 0x20);
  s.stick_y = H->rdf32(p + 0x24);
  s.cstick_x = H->rdf32(p + 0x28);
  s.cstick_y = H->rdf32(p + 0x2C);
  s.trigger_l = H->rdf32(p + 0x30);
  s.trigger_r = H->rdf32(p + 0x34);
  return s;
}

void recording_keys(int dport, uint32_t dfp) {
  int rport = choice("record_port");
  if (k_record.pressed()) {
    if (D.recording) {
      D.recording = false;
      char msg[80];
      std::snprintf(msg, sizeof msg, "Recorded %.1f s: play it on the dummy with the play key", D.rec_n / 60.0f);
      H->toast(ID, msg);
    } else if (rport == dport) {
      H->toast(ID, "Record from a port other than the dummy's");
    } else if (uint32_t rfp = pp_fighter(H, rport)) {
      D.recording = true;
      D.playing = false;
      D.rec_n = 0;
      D.rec_facing = H->rdf32(rfp + PP_FT_FACING);
      H->toast(ID, "Recording your inputs: press the record key again to stop");
    }
  }
  if (k_play.pressed()) {
    if (D.playing) { D.playing = false; H->toast(ID, "Playback stopped"); }
    else if (D.rec_n == 0) H->toast(ID, "Nothing recorded yet: record with the record key first");
    else if (dfp) {
      D.recording = false;
      D.playing = true;
      D.play_i = 0;
      // play it facing the way the dummy faces now: mirrored if the recording faced the other way
      D.mirror = (H->rdf32(dfp + PP_FT_FACING) > 0) != (D.rec_facing > 0);
      H->toast(ID, on("loop") ? "Playing the recording on a loop" : "Playing the recording");
    }
  }
  if (D.recording) {
    if (D.rec_n < REC_MAX) D.rec[D.rec_n++] = read_pad(rport);
    else { D.recording = false; H->toast(ID, "Recording full (20 s)"); }
  }
}

void dummy_badge(int port) {
  char line[120];
  if (D.recording) std::snprintf(line, sizeof line, "REC  %.1f s", D.rec_n / 60.0f);
  else if (D.playing) std::snprintf(line, sizeof line, "DUMMY P%d  playing %.1f / %.1f s", port + 1, D.play_i / 60.0f, D.rec_n / 60.0f);
  else if (D.last_action_age < 90) std::snprintf(line, sizeof line, "DUMMY P%d  %s", port + 1, D.last_action);
  else std::snprintf(line, sizeof line, "DUMMY P%d  %s, %s, %s", port + 1, STANCE_NAMES[choice("stance") & 3],
                     DI_NAMES[choice("di") % 6], TECH_NAMES[choice("tech") % 5]);
  float w = 18 + 4.3f * (float)std::strlen(line), h = 18, x = 8, y = 300 - h;
  if (PP_HOST_HAS(H, hud_place)) H->hud_place(PP_CORNER_BOTTOM_LEFT, w, h, &x, &y);
  H->hud_rect(x, y, x + w, y + h, pp_rgba(0x111838, 0.75f), 5, 1);
  H->hud_rect(x, y, x + 3, y + h, pp_rgba(D.recording ? 0xFF4D4D : pp_port_rgb[port], 1.0f), 1.5f, 1);
  H->hud_text(x + 9, y + 3, pp_rgba(D.recording ? 0xFF8080 : 0xF2F4FA, 1.0f), 11, line);
}

void dummy_frame(bool in_match) {
  if (!has_pad()) return;
  if (!in_match) {
    release();
    D.was_in_match = false;
    D.recording = D.playing = false;
    D.macro.act = ACT_NONE;
    return;
  }
  if (!D.was_in_match) {   // a new match: start as the settings say
    D.was_in_match = true;
    D.active = on("dummy");
    D.after_hit = D.after_shield = false;
  }
  if (k_dummy.pressed()) {
    D.active = !D.active;
    H->toast(ID, D.active ? "Dummy on" : "Dummy off: the port is back on its controller");
  }
  int port = dummy_port();
  uint32_t fp = pp_fighter(H, port);
  recording_keys(port, fp);
  if (!D.active || !fp) { release(); return; }
  if (pp_player_type(H, port) != 0) {
    static int warned = -1;
    if (warned != port) { warned = port; H->toast(ID, "The dummy's port is a CPU: make it a human player (HMN) for the dummy to take it over"); }
    release();
    return;
  }
  if (D.held != port) release();
  D.held = port;

  pp_pad_state out{};
  if (D.playing) {
    out = D.rec[D.play_i];
    if (D.mirror) { out.stick_x = -out.stick_x; out.cstick_x = -out.cstick_x; }
    if (++D.play_i >= D.rec_n) {
      if (on("loop")) D.play_i = 0;
      else D.playing = false;
    }
  } else {
    react(port, fp, out);
  }
  H->pad_set(port, &out);
  D.last = out;
  if (D.last_action_age < 999) ++D.last_action_age;
  dummy_badge(port);
}

// ---------------------------------------------------------------- savestates (0.5)

int match_id = 0;       // counts matches, so a state is only loaded into the match it came from
int saved_in = -1;      // the match the saved state belongs to
bool was_in_match = false;

bool has_states() { return PP_HOST_HAS(H, state_load); }

// D-pad right / left on any player's controller but the dummy's (UnclePunch's buttons), or the keys.
void dpad(bool& save, bool& load) {
  if (!on("state_dpad")) return;
  int dummy = D.active ? dummy_port() : -1;
  for (int i = 0; i < 4; ++i) {
    if (i == dummy || pp_player_type(H, i) != 0) continue;
    uint32_t pressed = H->rd32(PP_PAD_STATUS + PP_PAD_STRIDE * (uint32_t)i + PP_PAD_PRESSED);
    if (pressed & PP_BTN_DR) save = true;
    if (pressed & PP_BTN_DL) load = true;
  }
}

// automatic reloads: an exchange with the dummy's port, over and over
struct Drill {
  int since_load = 0;   // frames since the state was saved or loaded
  bool engaged = false; // the partner has been hit (or hit on shield) since
  int free_for = 0;     // frames the partner has been free since
  int dead_for = 0;
  int attempts = 0;
} R;

void request_load(bool quiet = false) {
  if (saved_in != match_id) { if (!quiet) H->toast(ID, "No state saved in this match: save one first (D-pad right or End)"); return; }
  if (H->state_load(0)) {
    if (!quiet) H->toast(ID, "State loaded");
    ++R.attempts;
    R.since_load = R.free_for = R.dead_for = 0;
    R.engaged = false;
    D.macro.act = ACT_NONE;
    D.after_hit = D.after_shield = false;
    D.prev_state = 0;
    D.prev_hitlag = 0;
  }
}

// Loads the state again once the exchange is over: the partner (the dummy's port) was hit and has
// been free for 40 frames, or was KO'd; or after a set time.
void auto_reload() {
  int mode = choice("reload");   // 0 off, 1 when the exchange ends, 2 after 3 s, 3 after 5 s
  if (!mode || saved_in != match_id) return;
  ++R.since_load;
  bool go = (mode == 2 && R.since_load >= 180) || (mode == 3 && R.since_load >= 300);
  if (uint32_t fp = pp_fighter(H, dummy_port())) {
    uint32_t s = H->rd32(fp + PP_FT_STATE);
    bool pressed = pp_state_is_punished(s) || pp_hitlag(H, fp) > 0 || s == PP_ST_GUARD_SET_OFF;
    if (pp_state_is_dead(s) || s == PP_ST_REBIRTH || s == PP_ST_REBIRTH_WAIT) {
      if (++R.dead_for >= 40) go = true;
    } else if (pressed) {
      R.engaged = true;
      R.free_for = 0;
    } else if (R.engaged && ++R.free_for >= 40 && mode == 1) {
      go = true;
    }
  }
  if (go) request_load(true);
  // the attempt count, bottom left
  char line[48];
  std::snprintf(line, sizeof line, "DRILL  try %d", R.attempts + 1);
  float w = 18 + 4.3f * (float)std::strlen(line), h = 18, x = 8, y = 300 - h;
  if (PP_HOST_HAS(H, hud_place)) H->hud_place(PP_CORNER_BOTTOM_LEFT, w, h, &x, &y);
  H->hud_rect(x, y, x + w, y + h, pp_rgba(0x111838, 0.75f), 5, 1);
  H->hud_rect(x, y, x + 3, y + h, pp_rgba(0xF2C200, 1.0f), 1.5f, 1);
  H->hud_text(x + 9, y + 3, pp_rgba(0xF2F4FA, 1.0f), 11, line);
}

void states(bool in_match) {
  if (!has_states()) return;
  if (in_match && !was_in_match) ++match_id;
  was_in_match = in_match;
  if (!in_match) return;
  bool save = k_save.pressed(), load = k_load.pressed();
  dpad(save, load);
  if (save && H->state_save(0)) {
    saved_in = match_id;
    R = Drill{};
    H->toast(ID, choice("reload") ? "State saved: it comes back on its own after each try" : "State saved: D-pad left (or Delete) to load it");
  } else if (load) request_load();
  auto_reload();
}

// ---------------------------------------------------------------- the frame

// Holds the game here while paused. Returns when the player resumes or steps one frame.
void hold() {
  for (;;) {
    if (k_pause.pressed()) { paused = false; H->toast(ID, "Resumed"); return; }
    if (k_step.pressed()) return;                       // run exactly one frame, stay paused
    if (has_states() && k_load.pressed()) { request_load(); return; }   // load, and show it
    if (!pp_in_match(H)) { paused = false; return; }
    Sleep(4);
  }
}

void frame(void*) {
  bool in_match = pp_in_match(H);
  states(in_match);
  dummy_frame(in_match);
  if (!in_match) { paused = false; return; }

  if (k_pause.pressed()) {
    paused = !paused;
    shown = false;
    H->toast(ID, paused ? "Paused: step with the frame advance key" : "Resumed");
  }
  if (k_slow.pressed()) {
    slow = !slow;
    H->toast(ID, slow ? (H->setting_number(ID, "slow_speed") > 0.5 ? "Quarter speed" : "Half speed") : "Full speed");
  }
  if (!paused && k_step.pressed()) { paused = true; shown = false; H->toast(ID, "Paused: step with the frame advance key"); }
  bool reset = k_reset.pressed();

  for (int i = 0; i < 4; ++i) {
    uint32_t fp = pp_fighter(H, i);
    if (!fp || !port_selected(i)) continue;
    if (reset) set_percent(fp, i, 0);
    if (on("lock_percent")) set_percent(fp, i, (float)H->setting_number(ID, "percent"));
    if (on("infinite_shield")) H->wrf32(fp + PP_FT_SHIELD, 60.0f);
    if (on("endless_stocks")) {
      uint32_t b = pp_player_block(i);
      if ((int8_t)H->rd8(b + PP_PL_STOCKS) < 2) H->wr8(b + PP_PL_STOCKS, 4);
    }
  }
  if (reset) H->toast(ID, "Percent reset");

  if (paused) {
    badge("PAUSED", 0xF2C200);
    // let the first paused frame finish so the badge reaches the screen, then hold
    if (shown) hold();
    shown = true;
  } else if (slow) {
    badge(H->setting_number(ID, "slow_speed") > 0.5 ? "1/4 SPEED" : "1/2 SPEED", 0x9FB4FF);
    // one extra frame's time (or three) per frame: the pacer then runs at a half (quarter) rate
    Sleep(H->setting_number(ID, "slow_speed") > 0.5 ? 50 : 17);
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
  H->declare_setting(ID, R"J({"key":"pause_key","group":"Hotkeys","type":"key","label":"Pause / resume","default":"F5"})J");
  H->declare_setting(ID, R"J({"key":"step_key","group":"Hotkeys","type":"key","label":"Frame advance","default":"F6"})J");
  H->declare_setting(ID, R"J({"key":"slow_key","group":"Hotkeys","type":"key","label":"Slow motion on / off","default":"F7"})J");
  H->declare_setting(ID, R"J({"key":"slow_speed","group":"Hotkeys","type":"choice","label":"Slow motion speed","default":"half","options":[{"value":"half","label":"Half speed"},{"value":"quarter","label":"Quarter speed"}]})J");
  H->declare_setting(ID, R"J({"key":"reset_key","group":"Hotkeys","type":"key","label":"Reset percent to 0","default":"F9"})J");
  H->declare_setting(ID, R"J({"key":"who","group":"Practice","type":"choice","label":"Percent, shield and stock options apply to","default":"p2","options":[{"value":"p1","label":"Port 1"},{"value":"p2","label":"Port 2"},{"value":"all","label":"Everyone"},{"value":"cpu","label":"CPU players"}]})J");
  H->declare_setting(ID, R"J({"key":"lock_percent","group":"Practice","type":"bool","label":"Lock percent","default":false})J");
  H->declare_setting(ID, R"J({"key":"percent","group":"Practice","type":"int","label":"Locked percent","default":60,"min":0,"max":999})J");
  H->declare_setting(ID, R"J({"key":"infinite_shield","group":"Practice","type":"bool","label":"Infinite shield","default":false})J");
  H->declare_setting(ID, R"J({"key":"endless_stocks","group":"Practice","type":"bool","label":"Endless stocks","default":false})J");
  if (PP_HOST_HAS(H, state_load)) {
    H->declare_setting(ID, R"J({"key":"save_key","group":"Savestates","type":"key","label":"Save state","default":"End"})J");
    H->declare_setting(ID, R"J({"key":"load_key","group":"Savestates","type":"key","label":"Load state","default":"Delete"})J");
    H->declare_setting(ID, R"J({"key":"reload","group":"Savestates","type":"choice","label":"Load the state again on its own","default":"off","options":[{"value":"off","label":"Never"},{"value":"exchange","label":"When the exchange is over (the dummy's port is free again, or KO'd)"},{"value":"3s","label":"After 3 seconds"},{"value":"5s","label":"After 5 seconds"}]})J");
    H->declare_setting(ID, R"J({"key":"state_dpad","group":"Savestates","type":"bool","label":"D-pad right saves, D-pad left loads (as in UnclePunch)","default":true})J");
  }
  if (PP_HOST_HAS(H, pad_set)) {
    H->declare_setting(ID, R"J({"key":"dummy","group":"Dummy","type":"bool","label":"Dummy on when a match starts","default":false})J");
    H->declare_setting(ID, R"J({"key":"dummy_key","group":"Dummy","type":"key","label":"Dummy on / off","default":"Home"})J");
    H->declare_setting(ID, R"J({"key":"dummy_port","group":"Dummy","type":"choice","label":"Dummy port (set it to a human player)","default":"p2","options":[{"value":"p1","label":"Port 1"},{"value":"p2","label":"Port 2"},{"value":"p3","label":"Port 3"},{"value":"p4","label":"Port 4"}]})J");
    H->declare_setting(ID, R"J({"key":"stance","group":"Dummy","type":"choice","label":"Dummy stands","default":"stand","options":[{"value":"stand","label":"Standing"},{"value":"crouch","label":"Crouching"},{"value":"shield","label":"Shielding"},{"value":"jump","label":"Jumping"}]})J");
    H->declare_setting(ID, R"J({"key":"di","group":"Dummy reactions","type":"choice","label":"DI","default":"survival","options":[{"value":"none","label":"None"},{"value":"survival","label":"Survival (up and in)"},{"value":"combo","label":"Combo (down and away)"},{"value":"in","label":"In"},{"value":"out","label":"Out"},{"value":"random","label":"Random"}]})J");
    H->declare_setting(ID, R"J({"key":"sdi","group":"Dummy reactions","type":"choice","label":"SDI","default":"none","options":[{"value":"none","label":"None"},{"value":"some","label":"Some"},{"value":"max","label":"As much as possible"}]})J");
    H->declare_setting(ID, R"J({"key":"tech","group":"Dummy reactions","type":"choice","label":"Tech","default":"random","options":[{"value":"miss","label":"Never (miss the tech)"},{"value":"in_place","label":"In place"},{"value":"toward","label":"Toward you"},{"value":"away","label":"Away from you"},{"value":"random","label":"Random (misses too)"}]})J");
    H->declare_setting(ID, R"J({"key":"getup","group":"Dummy reactions","type":"choice","label":"Get up (after a missed tech)","default":"random","options":[{"value":"stand","label":"Stand"},{"value":"toward","label":"Roll toward you"},{"value":"away","label":"Roll away"},{"value":"attack","label":"Getup attack"},{"value":"random","label":"Random"}]})J");
    H->declare_setting(ID, (std::string(R"J({"key":"counter_hit","group":"Dummy reactions","type":"choice","label":"When hitstun ends","default":"none","options":)J") + ACTIONS + "}").c_str());
    H->declare_setting(ID, (std::string(R"J({"key":"counter_shield","group":"Dummy reactions","type":"choice","label":"When shield stun ends","default":"none","options":)J") + ACTIONS + "}").c_str());
    H->declare_setting(ID, R"J({"key":"recover","group":"Dummy reactions","type":"bool","label":"Recover to the stage when knocked off","default":true})J");
    H->declare_setting(ID, R"J({"key":"mash","group":"Dummy reactions","type":"bool","label":"Mash out of grabs","default":false})J");
    H->declare_setting(ID, R"J({"key":"record_key","group":"Recording","type":"key","label":"Record your inputs (start / stop)","default":"PageUp"})J");
    H->declare_setting(ID, R"J({"key":"play_key","group":"Recording","type":"key","label":"Dummy plays the recording (start / stop)","default":"PageDown"})J");
    H->declare_setting(ID, R"J({"key":"record_port","group":"Recording","type":"choice","label":"Record the inputs of","default":"p1","options":[{"value":"p1","label":"Port 1"},{"value":"p2","label":"Port 2"},{"value":"p3","label":"Port 3"},{"value":"p4","label":"Port 4"}]})J");
    H->declare_setting(ID, R"J({"key":"loop","group":"Recording","type":"bool","label":"Loop the playback","default":true})J");
    H->set_status(ID, "F5 pause, F6 step, F7 slow, F9 0%. D-pad right / left: save / load. Home: dummy. PageUp record, PageDown play.");
  } else {
    H->set_status(ID, "F5 pause, F6 frame advance, F7 slow motion, F9 reset percent. (The dummy needs PascalPatch 0.5.)");
  }
  H->on_frame(frame, nullptr);
  return 0;
}
