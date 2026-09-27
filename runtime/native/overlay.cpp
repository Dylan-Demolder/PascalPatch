// The F2 overlay: PascalPatch's in-game plugin manager, drawn over the port's own frame the way
// BakkesMod's F2 window sits over Rocket League. Nothing in the port changes: the runtime finds
// the D3D12 swap chain and command queue classes through throwaway objects of its own, and stands
// in front of their Present and ExecuteCommandLists entries (the vtables are shared by every object
// of the class). Each present, it records one command list that draws Dear ImGui onto the back
// buffer, on the port's own queue, just before the port's present goes out.
//
// Threads: plugins run on the simulation thread; presents happen on the render thread; window
// messages arrive on the window thread. Messages for the overlay are queued and replayed on the
// render thread, which is the only one that touches ImGui. Plugin state crosses via pp_runtime.h.
//
// D3D11 (the port's --renderer d3d11) is not covered yet: the overlay stays off and plugins run.
// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"
#include "pascal_theme.h"
#include "pascalpatch/plugin.h"
#include "pp_runtime.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

using Present_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using Resize_t = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using Execute_t = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

// vtable slots (IUnknown 0-2, IDXGIObject 3-6, IDXGIDeviceSubObject 7, IDXGISwapChain 8-17, IDXGISwapChain1 18-28)
constexpr int kPresent = 8, kResizeBuffers = 13, kPresent1 = 22;
constexpr int kExecuteCommandLists = 10;   // IUnknown 0-2, ID3D12Object 3-6, ID3D12DeviceChild 7, ID3D12Pageable -, ID3D12CommandQueue 8..

Present_t o_present = nullptr;
Present1_t o_present1 = nullptr;
Resize_t o_resize = nullptr;
Execute_t o_execute = nullptr;

std::atomic<ID3D12CommandQueue*> g_queue{nullptr};   // the port's direct queue, seen executing work

// ---- window input ----
std::atomic<bool> g_open{false};
HWND g_hwnd = nullptr;
WNDPROC g_wndproc = nullptr;
struct Msg { UINT m; WPARAM w; LPARAM l; };
std::mutex g_msg_mutex;
std::vector<Msg> g_msgs;

bool overlay_input(UINT m) {
  switch (m) {
    case WM_MOUSEMOVE: case WM_MOUSELEAVE: case WM_NCMOUSEMOVE: case WM_NCMOUSELEAVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_LBUTTONUP:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: case WM_RBUTTONUP:
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: case WM_MBUTTONUP:
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: case WM_XBUTTONUP:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
    case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
      return true;
  }
  return false;
}

void queue_msg(UINT m, WPARAM w, LPARAM l) {
  std::lock_guard<std::mutex> lock(g_msg_mutex);
  if (g_msgs.size() < 512) g_msgs.push_back({m, w, l});
}

LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if ((m == WM_KEYDOWN || m == WM_SYSKEYDOWN) && w == VK_F2) {
    if (!(l & (1 << 30))) {
      bool open = !g_open.load();
      g_open.store(open);
      // Keys held while it opens would stay held in the game; the port clears them on focus loss.
      if (open) CallWindowProcW(g_wndproc, h, WM_KILLFOCUS, 0, 0);
    }
    return 0;
  }
  if ((m == WM_KEYUP || m == WM_SYSKEYUP) && w == VK_F2) return 0;
  if (g_open.load()) {
    if (m == WM_SETCURSOR && LOWORD(l) == HTCLIENT) { SetCursor(nullptr); return TRUE; }   // ImGui draws its own
    if (overlay_input(m)) {
      queue_msg(m, w, l);
      // Escape closes the overlay, like backing out of a Melee menu.
      if (m == WM_KEYDOWN && w == VK_ESCAPE && !(l & (1 << 30))) g_open.store(false);
      return 0;   // the game does not see input meant for the overlay
    }
    if (m == WM_SETFOCUS || m == WM_KILLFOCUS) queue_msg(m, w, l);
  }
  return CallWindowProcW(g_wndproc, h, m, w, l);
}

// ---- D3D12 state for drawing ----
struct Gfx {
  bool ready = false, failed = false;
  ID3D12Device* device = nullptr;
  ID3D12CommandQueue* queue = nullptr;
  ID3D12DescriptorHeap* rtv = nullptr;
  ID3D12DescriptorHeap* srv = nullptr;
  UINT rtv_size = 0, srv_size = 0;
  std::vector<bool> srv_used;
  ID3D12GraphicsCommandList* list = nullptr;
  ID3D12Fence* fence = nullptr;
  HANDLE event = nullptr;
  UINT64 fence_next = 0;
  ID3D12CommandAllocator* alloc[DXGI_MAX_SWAP_CHAIN_BUFFERS] = {};
  UINT64 alloc_fence[DXGI_MAX_SWAP_CHAIN_BUFFERS] = {};
  DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
  ImFont* body = nullptr;
  ImFont* display = nullptr;
  ULONGLONG started = 0;   // GetTickCount64 when the overlay came up
} g;

constexpr UINT kSrvCount = 64;

void srv_alloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
  for (UINT i = 0; i < kSrvCount; ++i)
    if (!g.srv_used[i]) {
      g.srv_used[i] = true;
      cpu->ptr = g.srv->GetCPUDescriptorHandleForHeapStart().ptr + (SIZE_T)i * g.srv_size;
      gpu->ptr = g.srv->GetGPUDescriptorHandleForHeapStart().ptr + (UINT64)i * g.srv_size;
      return;
    }
  cpu->ptr = 0; gpu->ptr = 0;
}
void srv_free(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE) {
  UINT i = (UINT)((cpu.ptr - g.srv->GetCPUDescriptorHandleForHeapStart().ptr) / g.srv_size);
  if (i < kSrvCount) g.srv_used[i] = false;
}

bool is_port_window(HWND h) {
  wchar_t cls[64] = {};
  return h && GetClassNameW(h, cls, 64) && !wcscmp(cls, L"MeleePortWindow");
}

ImFont* add_font(const char* file, float size) {
  char path[MAX_PATH];
  UINT n = GetWindowsDirectoryA(path, MAX_PATH);
  if (!n || n > MAX_PATH - 40) return nullptr;
  std::snprintf(path + n, MAX_PATH - n, "\\Fonts\\%s", file);
  if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return nullptr;
  return ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size);
}

std::string ui_select_on_open;

bool init(IDXGISwapChain3* sc) {
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(sc->GetDesc(&desc)) || !is_port_window(desc.OutputWindow)) return false;
  ID3D12CommandQueue* q = g_queue.load();
  if (!q) return false;   // no work seen yet: try again next present
  if (FAILED(sc->GetDevice(IID_PPV_ARGS(&g.device)))) { g.failed = true; return false; }
  g.queue = q;
  g.format = desc.BufferDesc.Format;
  D3D12_DESCRIPTOR_HEAP_DESC hd{};
  hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = DXGI_MAX_SWAP_CHAIN_BUFFERS;
  if (FAILED(g.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g.rtv)))) { g.failed = true; return false; }
  hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.NumDescriptors = kSrvCount; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if (FAILED(g.device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&g.srv)))) { g.failed = true; return false; }
  g.rtv_size = g.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  g.srv_size = g.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  g.srv_used.assign(kSrvCount, false);
  for (auto& a : g.alloc)
    if (FAILED(g.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)))) { g.failed = true; return false; }
  if (FAILED(g.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g.alloc[0], nullptr, IID_PPV_ARGS(&g.list)))) { g.failed = true; return false; }
  g.list->Close();
  if (FAILED(g.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g.fence)))) { g.failed = true; return false; }
  g.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
  PascalUI::Apply();
  ImGuiStyle& st = ImGui::GetStyle();
  // Selected tabs: navy with a gold bar over them, as the site's slanted tabs; plain gold made light text unreadable.
  st.Colors[ImGuiCol_TabSelected] = ImVec4(0.169f, 0.216f, 0.439f, 1.0f);
  st.Colors[ImGuiCol_TabSelectedOverline] = PascalUI::Accent();
  st.TabBarOverlineSize = 3;
  g.body = add_font("segoeui.ttf", 18.0f);
  g.display = add_font("bahnschrift.ttf", 18.0f);
  if (!g.body) g.body = io.Fonts->AddFontDefault();
  if (!g.display) g.display = g.body;
  io.FontDefault = g.body;

  g_hwnd = desc.OutputWindow;
  ImGui_ImplWin32_Init(g_hwnd);
  ImGui_ImplDX12_InitInfo info;
  info.Device = g.device;
  info.CommandQueue = g.queue;
  info.NumFramesInFlight = 8;
  info.RTVFormat = g.format;
  info.DSVFormat = DXGI_FORMAT_UNKNOWN;
  info.SrvDescriptorHeap = g.srv;
  info.SrvDescriptorAllocFn = srv_alloc;
  info.SrvDescriptorFreeFn = srv_free;
  if (!ImGui_ImplDX12_Init(&info)) { g.failed = true; return false; }
  g_wndproc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)&wndproc);
  g.started = GetTickCount64();
  // For tests and screenshots: start open, optionally on one plugin's tab.
  if (GetEnvironmentVariableW(L"PASCALPATCH_OVERLAY_OPEN", nullptr, 0)) g_open.store(true);
  char tab[64] = {};
  if (GetEnvironmentVariableA("PASCALPATCH_OVERLAY_TAB", tab, sizeof tab) && tab[0]) ui_select_on_open = tab;
  g.ready = true;
  pp::log("[pascalpatch] overlay ready (D3D12, %ux%u) - press F2", desc.BufferDesc.Width, desc.BufferDesc.Height);
  return true;
}

// ---- the overlay's UI ----
ImU32 rgba(uint32_t c) { return IM_COL32(c >> 24, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); }

void draw_hud(const std::vector<pp::HudCmd>& cmds) {
  if (cmds.empty()) return;
  ImVec2 ds = ImGui::GetIO().DisplaySize;
  float s = std::min(ds.x / 640.0f, ds.y / 480.0f);
  ImVec2 o((ds.x - 640 * s) * 0.5f, (ds.y - 480 * s) * 0.5f);   // the game's 4:3 picture, centred
  auto P = [&](float x, float y) { return ImVec2(o.x + x * s, o.y + y * s); };
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  for (const auto& c : cmds) {
    switch (c.kind) {
      case pp::HudCmd::Text: dl->AddText(g.display, c.f * s, P(c.a, c.b), rgba(c.rgba), c.text.c_str()); break;
      case pp::HudCmd::Rect:
        if (c.filled) dl->AddRectFilled(P(c.a, c.b), P(c.c, c.d), rgba(c.rgba), c.f * s);
        else dl->AddRect(P(c.a, c.b), P(c.c, c.d), rgba(c.rgba), c.f * s, 0, std::max(1.0f, s));
        break;
      case pp::HudCmd::Circle:
        if (c.filled) dl->AddCircleFilled(P(c.a, c.b), c.c * s, rgba(c.rgba));
        else dl->AddCircle(P(c.a, c.b), c.c * s, rgba(c.rgba), 0, std::max(1.0f, s));
        break;
    }
  }
}

void heading(const char* text, float size = 26.0f) {
  ImGui::PushFont(g.display, size);
  ImGui::TextColored(PascalUI::Accent(), "%s", text);
  ImGui::PopFont();
}

void draw_toast(size_t loaded) {
  double t = (GetTickCount64() - g.started) / 1000.0;
  if (t > 7.0 || g_open.load()) return;
  float alpha = t < 6.0 ? 1.0f : (float)(7.0 - t);
  ImVec2 ds = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(18, ds.y - 18), ImGuiCond_Always, ImVec2(0, 1));
  ImGui::SetNextWindowBgAlpha(0.92f * alpha);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
  ImGui::Begin("##pp_toast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PushFont(g.display, 20.0f);
  ImGui::TextColored(PascalUI::Accent(), "PASCALPATCH");
  ImGui::PopFont();
  ImGui::SameLine();
  ImGui::TextDisabled("%s", pp::VERSION);
  ImGui::Text("%zu plugin%s loaded  \xC2\xB7  press F2", loaded, loaded == 1 ? "" : "s");
  ImGui::End();
  ImGui::PopStyleVar();
}

struct UiState {
  std::map<std::string, int> next;   // downloaded plugins' "load next launch", read when the overlay opens
  std::map<std::string, std::vector<char>> text;   // edit buffers for text settings, by plugin/key
  bool was_open = false;
  std::string select;   // a tab to bring to front (from the manager's "Settings" buttons)
} ui;

bool setting_widget(pp::Plugin& p, pp::Setting& s) {
  ImGui::PushID(s.key.c_str());
  bool changed = false, commit = false;
  const char* label = s.label.empty() ? s.key.c_str() : s.label.c_str();
  if (s.type == "bool") {
    bool v = s.num != 0;
    if (ImGui::Checkbox(label, &v)) { s.num = v ? 1 : 0; changed = commit = true; }
  } else if (s.type == "int") {
    int v = (int)s.num;
    ImGui::SetNextItemWidth(280);
    if (s.max - s.min <= 1000 ? ImGui::SliderInt(label, &v, (int)s.min, (int)s.max) : ImGui::InputInt(label, &v)) {
      s.num = std::clamp((double)v, s.min, s.max); changed = true;
    }
    commit = ImGui::IsItemDeactivatedAfterEdit();
  } else if (s.type == "float") {
    float v = (float)s.num;
    ImGui::SetNextItemWidth(280);
    if (ImGui::SliderFloat(label, &v, (float)s.min, (float)s.max, "%.2f")) { s.num = v; changed = true; }
    commit = ImGui::IsItemDeactivatedAfterEdit();
  } else if (s.type == "choice") {
    int v = (int)s.num;
    ImGui::SetNextItemWidth(280);
    const char* preview = v >= 0 && v < (int)s.option_labels.size() ? s.option_labels[v].c_str() : "";
    if (ImGui::BeginCombo(label, preview)) {
      for (int i = 0; i < (int)s.options.size(); ++i)
        if (ImGui::Selectable(s.option_labels[i].c_str(), i == v)) { s.num = i; s.text = s.options[i]; changed = commit = true; }
      ImGui::EndCombo();
    }
  } else {   // text, key
    auto& buf = ui.text[p.id + "/" + s.key];
    if (buf.empty()) { buf.assign(256, 0); std::snprintf(buf.data(), buf.size(), "%s", s.text.c_str()); }
    ImGui::SetNextItemWidth(280);
    if (ImGui::InputText(label, buf.data(), buf.size())) { s.text = buf.data(); changed = true; }
    commit = ImGui::IsItemDeactivatedAfterEdit();
  }
  if (!s.help.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", s.help.c_str());
  }
  ImGui::PopID();
  (void)changed;
  return commit;
}

void plugin_tab(pp::Plugin& p) {
  heading(p.name.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("%s%s%s", p.version.empty() ? "" : "v", p.version.c_str(), p.source == "store" ? "  \xC2\xB7  downloaded" : "  \xC2\xB7  from profile");
  if (!p.status.empty()) ImGui::TextColored(PascalUI::Ok(), "%s", p.status.c_str());
  if (!p.loaded) ImGui::TextColored(PascalUI::Danger(), "Not running: it failed to load. See Console.");
  ImGui::Separator();
  if (p.settings.empty()) {
    ImGui::TextDisabled("This plugin has no settings.");
  } else {
    bool commit = false;
    for (auto& s : p.settings) commit |= setting_widget(p, s);
    ImGui::Spacing();
    if (ImGui::Button("Reset to defaults")) {
      for (auto& s : p.settings) { s.num = s.def; s.text = s.def_text; ui.text.erase(p.id + "/" + s.key); }
      commit = true;
    }
    if (commit) pp::save_settings(p);
    if (pp::settings_dir().empty()) ImGui::TextDisabled("Changes last until the game closes (no settings folder was given).");
  }
  if (!p.log.empty()) {
    ImGui::Spacing();
    ImGui::TextDisabled("Recent messages");
    ImGui::BeginChild("log", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (auto& line : p.log) ImGui::TextUnformatted(line.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
  }
}

void manager_tab(std::vector<pp::Plugin>& plugins) {
  heading("Plugins");
  ImGui::TextDisabled("Settings apply at once. Turning a downloaded plugin on or off takes effect next launch.");
  ImGui::Spacing();
  if (ImGui::BeginTable("plugins", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_WidthStretch, 3);
    ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthStretch, 1);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 3);
    ImGui::TableSetupColumn("Next launch", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 90);
    ImGui::TableHeadersRow();
    for (auto& p : plugins) {
      ImGui::PushID(p.id.c_str());
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(p.name.c_str());
      ImGui::TextDisabled("%s", p.file.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(p.version.empty() ? "-" : p.version.c_str());
      ImGui::TableNextColumn();
      if (!p.loaded) ImGui::TextColored(PascalUI::Danger(), "failed to load");
      else ImGui::TextColored(PascalUI::Ok(), "%s", p.status.empty() ? "running" : p.status.c_str());
      ImGui::TableNextColumn();
      auto it = ui.next.find(p.id);
      if (it != ui.next.end()) {
        bool on = it->second != 0;
        if (ImGui::Checkbox("Load", &on) && pp::set_next_launch(p.id, on)) it->second = on;
      } else {
        ImGui::TextDisabled("profile");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Part of the profile's build; change it in the PascalPatch app.");
      }
      ImGui::TableNextColumn();
      if (!p.settings.empty() && ImGui::SmallButton("Settings")) ui.select = p.id;
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  if (plugins.empty()) ImGui::TextDisabled("No plugins are loaded. Add some in the PascalPatch app's Browse page.");
}

void console_tab() {
  std::vector<std::string> lines;
  {
    std::lock_guard<std::mutex> lock(pp::mutex());
    lines.assign(pp::console().begin(), pp::console().end());
  }
  heading("Console");
  ImGui::BeginChild("console", ImVec2(0, 0), ImGuiChildFlags_Borders);
  for (auto& l : lines) {
    bool bad = l.find("cannot") != std::string::npos || l.find("refused") != std::string::npos || l.find("warning") != std::string::npos;
    if (bad) ImGui::TextColored(PascalUI::Highlight(), "%s", l.c_str());
    else ImGui::TextUnformatted(l.c_str());
  }
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
  ImGui::EndChild();
}

void about_tab() {
  heading("PascalPatch");
  ImGui::Text("Runtime %s  \xC2\xB7  plugin ABI %u  \xC2\xB7  Dear ImGui %s", pp::VERSION, PP_PLUGIN_ABI, IMGUI_VERSION);
  ImGui::Spacing();
  ImGui::TextWrapped("PascalPatch loads plugins into an unmodified melee_port.exe at run time. Nothing on disk is changed, "
                     "and the game stays offline: every network request it makes is refused.");
  ImGui::Spacing();
  ImGui::TextDisabled("F2  open or close this window        Esc  close");
  std::string dir = pp::settings_dir();
  ImGui::TextDisabled("Settings folder: %s", dir.empty() ? "(none: changes are not saved)" : dir.c_str());
}

void draw_window() {
  bool open = g_open.load();
  if (open && !ui.was_open) {   // re-read what next launch will load, once per opening
    ui.next.clear();
    std::vector<std::string> ids;
    {
      std::lock_guard<std::mutex> lock(pp::mutex());
      for (auto& p : pp::plugins()) if (p.source == "store") ids.push_back(p.id);
    }
    for (auto& id : ids) { bool on; if (pp::next_launch(id, &on)) ui.next[id] = on; }
  }
  ui.was_open = open;
  if (open && !ui_select_on_open.empty()) { ui.select = ui_select_on_open; ui_select_on_open.clear(); }
  ImGuiIO& io = ImGui::GetIO();
  io.MouseDrawCursor = open;
  if (!open) return;

  ImVec2 ds = io.DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(std::min(820.0f, ds.x - 40), std::min(560.0f, ds.y - 40)), ImGuiCond_FirstUseEver);
  bool keep = true;
  ImGui::PushFont(g.display, 0.0f);
  bool shown = ImGui::Begin("PASCALPATCH   \xC2\xB7   F2###pascalpatch", &keep, ImGuiWindowFlags_NoCollapse);
  ImGui::PopFont();
  if (shown && ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
    {
      std::lock_guard<std::mutex> lock(pp::mutex());   // no pp::log inside: it takes the same lock
      auto& plugins = pp::plugins();
      if (ImGui::BeginTabItem("Plugins")) { manager_tab(plugins); ImGui::EndTabItem(); }
      for (auto& p : plugins) {
        ImGuiTabItemFlags f = ui.select == p.id ? ImGuiTabItemFlags_SetSelected : 0;
        if (ImGui::BeginTabItem((p.name + "###" + p.id).c_str(), nullptr, f)) { plugin_tab(p); ImGui::EndTabItem(); }
      }
      ui.select.clear();
    }
    if (ImGui::BeginTabItem("Console")) { console_tab(); ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem("About")) { about_tab(); ImGui::EndTabItem(); }
    ImGui::EndTabBar();
  }
  ImGui::End();
  if (!keep) g_open.store(false);
}

void render(IDXGISwapChain* raw) {
  if (g.failed) return;
  IDXGISwapChain3* sc = nullptr;
  if (FAILED(raw->QueryInterface(IID_PPV_ARGS(&sc)))) return;
  struct Rel { IUnknown* p; ~Rel() { p->Release(); } } rel{sc};
  if (!g.ready && !init(sc)) return;
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(sc->GetDesc(&desc)) || desc.OutputWindow != g_hwnd) return;

  std::vector<Msg> msgs;
  { std::lock_guard<std::mutex> lock(g_msg_mutex); msgs.swap(g_msgs); }
  std::vector<pp::HudCmd> hud = pp::hud_latest();
  bool toast = GetTickCount64() - g.started < 7000;
  if (!g_open.load() && !ui.was_open && hud.empty() && !toast) return;   // nothing to draw: leave the frame alone

  for (auto& m : msgs) ImGui_ImplWin32_WndProcHandler(g_hwnd, m.m, m.w, m.l);
  size_t loaded = 0;
  {
    std::lock_guard<std::mutex> lock(pp::mutex());
    for (auto& p : pp::plugins()) loaded += p.loaded;
  }
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  draw_hud(hud);
  draw_toast(loaded);
  draw_window();
  ImGui::Render();

  UINT idx = sc->GetCurrentBackBufferIndex() % DXGI_MAX_SWAP_CHAIN_BUFFERS;
  if (g.fence->GetCompletedValue() < g.alloc_fence[idx]) {
    g.fence->SetEventOnCompletion(g.alloc_fence[idx], g.event);
    WaitForSingleObject(g.event, 1000);
  }
  ID3D12Resource* buffer = nullptr;
  if (FAILED(sc->GetBuffer(idx, IID_PPV_ARGS(&buffer)))) return;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = g.rtv->GetCPUDescriptorHandleForHeapStart();
  rtv.ptr += (SIZE_T)idx * g.rtv_size;
  g.device->CreateRenderTargetView(buffer, nullptr, rtv);
  g.alloc[idx]->Reset();
  g.list->Reset(g.alloc[idx], nullptr);
  D3D12_RESOURCE_BARRIER b{};
  b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.pResource = buffer;
  b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  g.list->ResourceBarrier(1, &b);
  g.list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  g.list->SetDescriptorHeaps(1, &g.srv);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g.list);
  std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
  g.list->ResourceBarrier(1, &b);
  g.list->Close();
  ID3D12CommandList* lists[] = {g.list};
  o_execute(g.queue, 1, lists);
  g.queue->Signal(g.fence, ++g.fence_next);
  g.alloc_fence[idx] = g.fence_next;
  buffer->Release();
}

std::atomic<bool> g_in_render{false};
void guarded_render(IDXGISwapChain* sc) {
  if (g_in_render.exchange(true)) return;
  render(sc);
  g_in_render.store(false);
}

HRESULT STDMETHODCALLTYPE hk_present(IDXGISwapChain* sc, UINT sync, UINT flags) {
  if (!(flags & DXGI_PRESENT_TEST)) guarded_render(sc);
  return o_present(sc, sync, flags);
}
HRESULT STDMETHODCALLTYPE hk_present1(IDXGISwapChain1* sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* pp_) {
  if (!(flags & DXGI_PRESENT_TEST)) guarded_render(sc);
  return o_present1(sc, sync, flags, pp_);
}
HRESULT STDMETHODCALLTYPE hk_resize(IDXGISwapChain* sc, UINT n, UINT w, UINT h, DXGI_FORMAT f, UINT flags) {
  // Our command lists hold no buffer references past their frame, so the port's own GPU wait covers them.
  if (g.ready && g.fence && g.fence->GetCompletedValue() < g.fence_next) {
    g.fence->SetEventOnCompletion(g.fence_next, g.event);
    WaitForSingleObject(g.event, 1000);
  }
  return o_resize(sc, n, w, h, f, flags);
}
void STDMETHODCALLTYPE hk_execute(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* lists) {
  if (!g_queue.load() && q->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
    q->AddRef();   // kept for the life of the process
    ID3D12CommandQueue* none = nullptr;
    if (!g_queue.compare_exchange_strong(none, q)) q->Release();
  }
  o_execute(q, n, lists);
}

template <class T> T patch(void** vtbl, int i, void* fn) {
  DWORD old;
  if (!VirtualProtect(&vtbl[i], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return nullptr;
  T prev = (T)vtbl[i];
  vtbl[i] = fn;
  VirtualProtect(&vtbl[i], sizeof(void*), old, &old);
  return prev;
}

LRESULT CALLBACK dummy_proc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

}  // namespace

namespace overlay {
void install() {
  static std::atomic<bool> done{false};
  if (done.exchange(true)) return;
  if (!GetModuleHandleW(L"d3d12.dll")) {
    pp::log("[pascalpatch] overlay off: the port is not drawing with D3D12 (the D3D11 renderer is not supported yet)");
    return;
  }
  WNDCLASSEXW wc{sizeof wc};
  wc.lpfnWndProc = dummy_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"PascalPatchProbe";
  RegisterClassExW(&wc);
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
  ID3D12Device* dev = nullptr;
  ID3D12CommandQueue* q = nullptr;
  IDXGIFactory2* factory = nullptr;
  IDXGISwapChain1* sc = nullptr;
  bool ok = false;
  if (hwnd && SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
    D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (SUCCEEDED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&q))) && SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
      DXGI_SWAP_CHAIN_DESC1 sd{};
      sd.Width = 64; sd.Height = 64; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.SampleDesc.Count = 1;
      sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
      if (SUCCEEDED(factory->CreateSwapChainForHwnd(q, hwnd, &sd, nullptr, nullptr, &sc))) {
        void** sv = *(void***)sc;
        void** qv = *(void***)q;
        o_execute = patch<Execute_t>(qv, kExecuteCommandLists, (void*)&hk_execute);
        o_resize = patch<Resize_t>(sv, kResizeBuffers, (void*)&hk_resize);
        o_present1 = patch<Present1_t>(sv, kPresent1, (void*)&hk_present1);
        o_present = patch<Present_t>(sv, kPresent, (void*)&hk_present);
        ok = o_execute && o_resize && o_present1 && o_present;
      }
    }
  }
  if (sc) sc->Release();
  if (factory) factory->Release();
  if (q) q->Release();
  if (dev) dev->Release();
  if (hwnd) DestroyWindow(hwnd);
  pp::log(ok ? "[pascalpatch] overlay installed (F2)" : "[pascalpatch] overlay off: could not reach D3D12 presentation");
}
}  // namespace overlay
