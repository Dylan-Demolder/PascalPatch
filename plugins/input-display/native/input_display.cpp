// input-display: draws one controller's sticks, buttons and triggers over the game (PascalPatch
// native plugin). It reads the pad state the game itself reads each frame, HSD_PadMasterStatus,
// and draws with the runtime's HUD calls, so it needs PascalPatch 0.2 or newer. Read-only: it
// never writes game memory, and it is the example the plugin site ships first.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/plugin.h"

#include <cstdint>
#include <cstring>

namespace {

constexpr const char* ID = "input-display";
constexpr uint32_t PAD_STATUS = 0x804C1FAC;   // HSD_PadMasterStatus[4], NTSC 1.02
constexpr uint32_t PAD_SIZE = 0x44;
// offsets in HSD_PadStatus
constexpr uint32_t BUTTON = 0x00, STICK_X = 0x20, STICK_Y = 0x24, CSTICK_X = 0x28, CSTICK_Y = 0x2C, ANALOG_L = 0x30, ANALOG_R = 0x34;
enum : uint32_t { DL = 0x1, DR = 0x2, DD = 0x4, DU = 0x8, Z = 0x10, R = 0x20, L = 0x40, A = 0x100, B = 0x200, X = 0x400, Y = 0x800, START = 0x1000 };
// Pascal UI port colours (P1-P4)
constexpr uint32_t PORT_RGB[4] = {0xE5322D, 0x2E6BF0, 0xF2C200, 0x2DB84D};

const pp_host* H = nullptr;

uint32_t col(uint32_t rgb, float alpha) {
  if (alpha < 0) alpha = 0;
  if (alpha > 1) alpha = 1;
  return (rgb << 8) | (uint32_t)(alpha * 255.0f + 0.5f);
}

void frame(void*) {
  int port = (int)H->setting_number(ID, "port");          // a choice gives its index: 0..3
  int corner = (int)H->setting_number(ID, "corner");      // bottom-left, bottom-right, top-left, top-right
  float s = (float)H->setting_number(ID, "scale");
  float op = (float)H->setting_number(ID, "opacity");
  if (port < 0 || port > 3) port = 0;
  if (s <= 0) s = 1;
  if (op <= 0) op = 0.85f;

  uint32_t pad = PAD_STATUS + PAD_SIZE * (uint32_t)port;
  uint32_t b = H->rd32(pad + BUTTON);
  float sx = H->rdf32(pad + STICK_X), sy = H->rdf32(pad + STICK_Y);
  float cx = H->rdf32(pad + CSTICK_X), cy = H->rdf32(pad + CSTICK_Y);
  float al = H->rdf32(pad + ANALOG_L), ar = H->rdf32(pad + ANALOG_R);

  const float w = 156 * s, h = 84 * s, margin = 14;
  float ox = (corner == 1 || corner == 3) ? 640 - margin - w : margin;
  float oy = (corner >= 2) ? margin : 480 - margin - h;
  auto X_ = [&](float x) { return ox + x * s; };
  auto Y_ = [&](float y) { return oy + y * s; };
  const uint32_t white = 0xF2F4FA, dim = 0x626C93, navy = 0x111838;

  H->hud_rect(X_(0), Y_(0), X_(156), Y_(84), col(navy, 0.72f * op), 8 * s, 1);
  H->hud_rect(X_(0), Y_(0), X_(156), Y_(84), col(PORT_RGB[port], 0.9f * op), 8 * s, 0);
  char label[4] = {'P', (char)('1' + port), 0, 0};
  H->hud_text(X_(8), Y_(64), col(PORT_RGB[port], op), 16 * s, label);

  // triggers: the analog travel as a bar, bright when the button clicks in
  auto trigger = [&](float x0, float v, bool digital) {
    H->hud_rect(X_(x0), Y_(6), X_(x0 + 52), Y_(13), col(dim, 0.6f * op), 2 * s, 0);
    if (v > 0.01f || digital) H->hud_rect(X_(x0), Y_(6), X_(x0 + 52 * (digital ? 1.0f : v)), Y_(13), col(digital ? white : 0xA0A8C8, op), 2 * s, 1);
  };
  trigger(8, al, (b & L) != 0);
  trigger(96, ar, (b & R) != 0);
  H->hud_rect(X_(96), Y_(16), X_(148), Y_(21), col(0x7B3FE4, (b & Z) ? op : 0.35f * op), 2 * s, (b & Z) ? 1 : 0);

  // control stick and C-stick
  H->hud_circle(X_(32), Y_(44), 20 * s, col(dim, 0.8f * op), 0);
  H->hud_circle(X_(32 + sx * 16), Y_(44 - sy * 16), 7 * s, col(white, op), 1);
  H->hud_circle(X_(72), Y_(58), 12 * s, col(dim, 0.8f * op), 0);
  H->hud_circle(X_(72 + cx * 9), Y_(58 - cy * 9), 5 * s, col(0xF2C200, op), 1);

  // face buttons in their GameCube places
  auto button = [&](float x, float y, float r, uint32_t rgb, bool down) { H->hud_circle(X_(x), Y_(y), r * s, col(rgb, down ? op : 0.45f * op), down ? 1 : 0); };
  button(126, 48, 11, 0x2DB84D, (b & A) != 0);
  button(108, 62, 7, 0xE5322D, (b & B) != 0);
  button(145, 38, 6, white, (b & X) != 0);
  button(122, 30, 6, white, (b & Y) != 0);
  button(72, 32, 4, white, (b & START) != 0);
  // D-pad as four ticks
  const float dx = 52, dy = 72;
  auto dpad = [&](float x0, float y0, float x1, float y1, bool down) { H->hud_rect(X_(x0), Y_(y0), X_(x1), Y_(y1), col(down ? white : dim, down ? op : 0.5f * op), 1, 1); };
  dpad(dx - 2, dy - 9, dx + 2, dy - 3, (b & DU) != 0);
  dpad(dx - 2, dy + 3, dx + 2, dy + 9, (b & DD) != 0);
  dpad(dx - 9, dy - 2, dx - 3, dy + 2, (b & DL) != 0);
  dpad(dx + 3, dy - 2, dx + 9, dy + 2, (b & DR) != 0);
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char*) {
  if (!host || host->abi != PP_PLUGIN_ABI) return 1;
  if (!PP_HOST_HAS(host, hud_circle)) {
    if (host->log) host->log(ID, "needs PascalPatch 0.2 or newer (HUD drawing)");
    return 2;
  }
  H = host;
  // Downloaded copies get these from plugin.json already; a copy staged by a profile declares them here.
  H->declare_setting(ID, R"({"key":"port","type":"choice","label":"Controller","default":"1","options":[{"value":"1","label":"Port 1"},{"value":"2","label":"Port 2"},{"value":"3","label":"Port 3"},{"value":"4","label":"Port 4"}]})");
  H->declare_setting(ID, R"({"key":"corner","type":"choice","label":"Position","default":"bottom-left","options":[{"value":"bottom-left","label":"Bottom left"},{"value":"bottom-right","label":"Bottom right"},{"value":"top-left","label":"Top left"},{"value":"top-right","label":"Top right"}]})");
  H->declare_setting(ID, R"({"key":"scale","type":"float","label":"Size","default":1.0,"min":0.5,"max":2.0})");
  H->declare_setting(ID, R"({"key":"opacity","type":"float","label":"Opacity","default":0.85,"min":0.2,"max":1.0})");
  H->set_status(ID, "Showing a controller on screen.");
  H->on_frame(frame, nullptr);
  return 0;
}
