// State the runtime shares between the simulation thread (plugins, runtime.cpp) and the render
// thread (the F2 overlay, overlay.cpp). Everything here is guarded by pp::mutex().
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace pp {

constexpr const char* VERSION = "0.5.0";

struct Setting {
  std::string key, label, type = "bool", help;   // type: bool int float choice text
  double num = 0, def = 0, min = 0, max = 1, step = 0;
  std::vector<std::string> options, option_labels;
  std::string text, def_text;
};

struct Plugin {
  std::string id, name, version, file, source = "profile", status;
  bool loaded = false;
  std::vector<Setting> settings;
  std::deque<std::string> log;   // this plugin's recent log lines
};

struct HudCmd {
  enum Kind : uint8_t { Text, Rect, Circle, Capsule } kind;
  float a, b, c, d, f;   // text: x y - - size; rect: x0 y0 x1 y1 rounding; circle: x y r - -;
                         // capsule: x0 y0 r0 x1 y1, and r1 in g
  uint32_t rgba;
  bool filled;
  std::string text;
  uint8_t align = 0;      // text: 0 left, 1 centre, 2 right
  bool outline = false;   // text: dark outline (hud_label)
  float g = 0;            // capsule: r1
};

struct Toast {
  std::string plugin, text;
  uint64_t shown = 0;   // GetTickCount64 when it was posted
};

// Key names for "key" settings ("F5", "P", "Numpad4"); 0 / "" for none.
int key_vk(const std::string& name);
std::string key_name(int vk);

std::mutex& mutex();
std::vector<Plugin>& plugins();            // in load order
Plugin* find(const std::string& id);        // caller holds mutex()
std::deque<std::string>& console();         // every runtime log line (recent)
void save_settings(const Plugin& p);        // caller holds mutex(); writes PASCALPATCH_SETTINGS/<id>.json
bool set_next_launch(const std::string& id, bool enabled);   // downloaded plugins: plugins.json
bool next_launch(const std::string& id, bool* enabled);

// HUD: plugins draw during a frame (sim thread); hud_publish() at the frame's end hands the list to
// the overlay, which draws the latest published list every present.
std::vector<HudCmd>& hud_building();        // sim thread only
void hud_publish();
std::vector<HudCmd> hud_latest();
std::deque<Toast>& toasts();                // caller holds mutex(); newest last

void log(const char* fmt, ...);
std::string settings_dir();

}  // namespace pp

namespace overlay {
void install();   // hooks presentation once the port has a window; safe to call more than once
bool is_open();   // the F2 window is showing
}
