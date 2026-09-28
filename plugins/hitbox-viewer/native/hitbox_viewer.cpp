// hitbox-viewer: draws every fighter's hitboxes and hurtboxes over the game (PascalPatch native
// plugin, read-only).
//
//   Hitboxes   red (grabs purple), drawn as the capsule each one sweeps this frame
//   Hurtboxes  yellow; green while invincible, blue while intangible (dodges, ledge, respawn)
//
// World points are projected with the game's own camera: the main camera's HSD_CObj keeps the
// view matrix it last rendered with, its field of view, aspect and viewport (decomp: cm/camera.c
// game_camera at 0x80452C68, sysdolphin cobj.h). Capsule layouts come from lb/types.h (HitCapsule
// 0x138 bytes, four per fighter at fp+0x914; FighterHurtCapsule 0x4C bytes at fp+0x11A0).
// A key toggles the view. Needs PascalPatch 0.3; 0.4 draws smoother capsules.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/melee.h"

#include <cmath>

namespace {

constexpr const char* ID = "hitbox-viewer";

constexpr uint32_t GAME_CAMERA = 0x80452C68;   // Camera game_camera; +0 its gobj
constexpr uint32_t GOBJ_HSD_OBJ = 0x28;         // HSD_GObj.hsd_obj: the camera gobj's HSD_CObj
constexpr uint32_t COBJ_VIEWPORT = 0x0C, COBJ_NEAR = 0x38, COBJ_FOV = 0x40, COBJ_ASPECT = 0x44, COBJ_PROJ = 0x50,
                   COBJ_VIEW = 0x54;
constexpr uint32_t FT_HIT = 0x914, HIT_SIZE = 0x138, HIT_SCALE = 0x1C, HIT_ELEMENT = 0x30, HIT_POS = 0x4C, HIT_PREV = 0x58;
constexpr uint32_t FT_HURT_LEN = 0x119E, FT_HURT = 0x11A0, HURT_SIZE = 0x4C, HURT_SCALE = 0x1C, HURT_A = 0x28, HURT_B = 0x34;
constexpr uint32_t ELEMENT_CATCH = 8;

const pp_host* H = nullptr;

struct Camera {
  float m[12];
  float cot, aspect, near_z, left, right, top, bottom;
};

bool read_camera(Camera& c) {
  uint32_t gobj = H->rd32(GAME_CAMERA);
  if (!pp_is_ptr(gobj)) return false;
  uint32_t cobj = H->rd32(gobj + GOBJ_HSD_OBJ);
  if (!pp_is_ptr(cobj) || H->rd8(cobj + COBJ_PROJ) != 1) return false;   // 1: perspective
  for (int i = 0; i < 12; ++i) c.m[i] = H->rdf32(cobj + COBJ_VIEW + 4 * i);
  float fov = H->rdf32(cobj + COBJ_FOV);
  c.aspect = H->rdf32(cobj + COBJ_ASPECT);
  c.near_z = H->rdf32(cobj + COBJ_NEAR);
  c.left = H->rdf32(cobj + COBJ_VIEWPORT);
  c.right = H->rdf32(cobj + COBJ_VIEWPORT + 4);
  c.top = H->rdf32(cobj + COBJ_VIEWPORT + 8);
  c.bottom = H->rdf32(cobj + COBJ_VIEWPORT + 12);
  if (!(fov > 1 && fov < 179) || !(c.aspect > 0.1f) || c.right <= c.left || c.bottom <= c.top) return false;
  c.cot = 1.0f / std::tan(fov * 3.14159265f / 360.0f);
  return true;
}

struct Screen { float x, y, px_per_unit; bool ok; };

// GX perspective (C_MTXPerspective) of a world point, in the HUD's 640 x 480 space.
Screen project(const Camera& c, float x, float y, float z) {
  float vx = c.m[0] * x + c.m[1] * y + c.m[2] * z + c.m[3];
  float vy = c.m[4] * x + c.m[5] * y + c.m[6] * z + c.m[7];
  float vz = c.m[8] * x + c.m[9] * y + c.m[10] * z + c.m[11];
  if (vz > -c.near_z) return {0, 0, 0, false};   // behind the camera
  float w = -vz;
  float nx = c.cot / c.aspect * vx / w, ny = c.cot * vy / w;
  Screen s;
  s.x = c.left + (nx + 1) * 0.5f * (c.right - c.left);
  s.y = c.top + (1 - ny) * 0.5f * (c.bottom - c.top);
  s.px_per_unit = c.cot / w * 0.5f * (c.bottom - c.top);
  s.ok = std::isfinite(s.x) && std::isfinite(s.y);
  return s;
}

void read_vec(uint32_t a, float v[3]) {
  for (int i = 0; i < 3; ++i) v[i] = H->rdf32(a + 4 * i);
}

// A capsule from a to b with radius r (world units): a translucent fill and a solid outline.
// PascalPatch 0.4 draws true capsules; on 0.3 it is faked with discs along the segment.
void capsule(const Camera& cam, const float a[3], const float b[3], float r, uint32_t rgb, float alpha) {
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

void draw_fighter(const Camera& cam, uint32_t fp, float alpha) {
  float a[3], b[3];
  if (on("hurtboxes")) {
    int n = H->rd8(fp + FT_HURT_LEN);
    if (n > 15) n = 15;
    for (int i = 0; i < n; ++i) {
      uint32_t h = fp + FT_HURT + HURT_SIZE * i;
      uint32_t state = H->rd32(h);
      read_vec(h + HURT_A, a);
      read_vec(h + HURT_B, b);
      uint32_t rgb = state == 1 ? 0x5BD68A : state == 2 ? 0x5B9BFF : 0xF2D25B;
      capsule(cam, a, b, H->rdf32(h + HURT_SCALE), rgb, alpha * 0.55f);
    }
  }
  if (on("hitboxes")) {
    for (int i = 0; i < 4; ++i) {
      uint32_t h = fp + FT_HIT + HIT_SIZE * i;
      if (H->rd32(h) == 0) continue;   // HitCapsule_Disabled
      read_vec(h + HIT_PREV, a);
      read_vec(h + HIT_POS, b);
      uint32_t rgb = H->rd32(h + HIT_ELEMENT) == ELEMENT_CATCH ? 0xB36BFF : 0xFF3B3B;
      capsule(cam, a, b, H->rdf32(h + HIT_SCALE), rgb, alpha);
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
  Camera cam;
  if (!read_camera(cam)) return;
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
  H->declare_setting(ID, R"({"key":"toggle_key","type":"key","label":"Show / hide","default":"F3"})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.9,"min":0.2,"max":1.0})");
  H->set_status(ID, "Drawing hitboxes (red) and hurtboxes (yellow). F3 shows or hides them.");
  H->on_frame(frame, nullptr);
  return 0;
}
