// hitbox-viewer: draws every fighter's hitboxes and hurtboxes over the game (PascalPatch native
// plugin, read-only).
//
//   Hitboxes   red (grabs purple), drawn as the capsule each one sweeps this frame
//   Hurtboxes  yellow; green while invincible, blue while intangible (dodges, ledge, respawn)
//
// World points are projected with the game's own camera (pascalpatch/camera.h). Capsule layouts
// are the SDK's PP_HIT_* and PP_HURT_* (lb/types.h). A key toggles the view. Needs PascalPatch
// 0.3; 0.4 draws smoother capsules.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/camera.h"

#include <cmath>

namespace {

constexpr const char* ID = "hitbox-viewer";

const pp_host* H = nullptr;

struct Screen { float x, y, px_per_unit; bool ok; };

Screen project(const pp_camera& c, float x, float y, float z) {
  Screen s{};
  s.ok = pp_project(&c, x, y, z, &s.x, &s.y, &s.px_per_unit) != 0;
  return s;
}

void read_vec(uint32_t a, float v[3]) {
  for (int i = 0; i < 3; ++i) v[i] = H->rdf32(a + 4 * i);
}

// A capsule from a to b with radius r (world units): a translucent fill and a solid outline.
// PascalPatch 0.4 draws true capsules; on 0.3 it is faked with discs along the segment.
void capsule(const pp_camera& cam, const float a[3], const float b[3], float r, uint32_t rgb, float alpha) {
  Screen sa = project(cam, a[0], a[1], a[2]), sb = project(cam, b[0], b[1], b[2]);
  if (!sa.ok || !sb.ok) return;
  float ra = r * sa.px_per_unit, rb = r * sb.px_per_unit;
  if (ra < 0.5f || ra > 400) return;
  if (PP_HOST_HAS(H, hud_capsule)) {
    H->hud_capsule(sa.x, sa.y, ra, sb.x, sb.y, rb, pp_rgba(rgb, alpha * 0.45f), 1);
    H->hud_capsule(sa.x, sa.y, ra, sb.x, sb.y, rb, pp_rgba(rgb, alpha), 0);
    return;
  }
  float dx = sb.x - sa.x, dy = sb.y - sa.y, len = std::sqrt(dx * dx + dy * dy);
  int n = len < 0.5f ? 0 : (int)std::ceil(len / (0.6f * (ra < rb ? ra : rb) + 0.5f));
  if (n > 24) n = 24;
  for (int i = 0; i <= n; ++i) {
    float t = n ? (float)i / (float)n : 0;
    H->hud_circle(sa.x + dx * t, sa.y + dy * t, ra + (rb - ra) * t, pp_rgba(rgb, alpha * (n ? 0.35f : 0.45f)), 1);
  }
  H->hud_circle(sa.x, sa.y, ra, pp_rgba(rgb, alpha), 0);
  if (n) H->hud_circle(sb.x, sb.y, rb, pp_rgba(rgb, alpha), 0);
}

bool on(const char* k) { return H->setting_number(ID, k) > 0.5; }

void draw_fighter(const pp_camera& cam, uint32_t fp, float alpha) {
  float a[3], b[3];
  if (on("hurtboxes")) {
    int n = H->rd8(fp + PP_FT_HURT_COUNT);
    if (n > 15) n = 15;
    for (int i = 0; i < n; ++i) {
      uint32_t h = fp + PP_FT_HURTBOXES + PP_HURT_STRIDE * i;
      uint32_t state = H->rd32(h + PP_HURT_STATE);
      read_vec(h + PP_HURT_A, a);
      read_vec(h + PP_HURT_B, b);
      uint32_t rgb = state == PP_HURT_INVINCIBLE ? 0x5BD68A : state == PP_HURT_INTANGIBLE ? 0x5B9BFF : 0xF2D25B;
      capsule(cam, a, b, H->rdf32(h + PP_HURT_RADIUS), rgb, alpha * 0.55f);
    }
  }
  if (on("hitboxes")) {
    for (int i = 0; i < 4; ++i) {
      uint32_t h = fp + PP_FT_HITBOXES + PP_HIT_STRIDE * i;
      if (H->rd32(h + PP_HIT_STATE) == 0) continue;   // HitCapsule_Disabled
      read_vec(h + PP_HIT_PREV_POS, a);
      read_vec(h + PP_HIT_POS, b);
      uint32_t rgb = H->rd32(h + PP_HIT_ELEMENT) == PP_ELEMENT_CATCH ? 0xB36BFF : 0xFF3B3B;
      capsule(cam, a, b, H->rdf32(h + PP_HIT_SIZE), rgb, alpha);
    }
  }
}

bool was_down = false, shown = true;

void frame(void*) {
  int vk = (int)H->setting_number(ID, "toggle_key");
  bool down = vk > 0 && H->key_down(vk);
  if (down && !was_down) {
    shown = !shown;
    H->toast(ID, shown ? "Hitboxes on" : "Hitboxes off");
  }
  was_down = down;
  if (!shown || !pp_in_match(H)) return;
  pp_camera cam;
  if (!pp_camera_read(H, &cam)) return;
  float alpha = (float)H->setting_number(ID, "opacity");
  for (int port = 0; port < 4; ++port)
    for (int sub = 0; sub < 2; ++sub) {   // sub 1: Nana
      uint32_t fp = pp_fighter_ex(H, port, sub);
      if (fp) draw_fighter(cam, fp, alpha);
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
  H->declare_setting(ID, R"({"key":"hitboxes","type":"bool","label":"Hitboxes","default":true})");
  H->declare_setting(ID, R"({"key":"hurtboxes","type":"bool","label":"Hurtboxes","default":true})");
  H->declare_setting(ID, R"({"key":"toggle_key","type":"key","label":"Show / hide","default":"Numpad1"})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.9,"min":0.2,"max":1.0})");
  H->set_status(ID, "Drawing hitboxes (red) and hurtboxes (yellow). Numpad1 (or the key you choose) shows or hides them.");
  H->on_frame(frame, nullptr);
  return 0;
}
