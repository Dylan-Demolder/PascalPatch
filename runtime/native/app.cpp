// PascalPatch.exe: the desktop app's front door. It sits in the notification area (tray), runs the
// app's local server (python -m pascalpatch.cli app --tray) hidden, and opens the app's window.
//
//   PascalPatch.exe           start (or show the running copy's window)
//   PascalPatch.exe --tray    start in the tray without opening the window (Start with Windows)
//
// Closing the window leaves PascalPatch running in the tray; Quit in the tray menu stops it. The
// window is Microsoft Edge (or Chrome) in app mode with a private profile, as the app has always
// used. The tray menu also plays a profile and opens Character Studio, through the server's API.
// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app_resource.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

constexpr wchar_t kClass[] = L"PascalPatchTray";
constexpr wchar_t kMutex[] = L"Local\\PascalPatch.App";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"PascalPatch";
constexpr wchar_t kWindowTitle[] = L"PascalPatch";   // the app page's <title>

enum : UINT {
  WM_TRAY = WM_APP + 1,      // notification-area icon events
  WM_READY,                  // the server printed its address
  WM_SERVER_EXIT,            // the server process ended
  WM_SHOW_WINDOW,            // another PascalPatch.exe asked us to show the window
  WM_WINDOW_CLOSED,          // the app window closed
};
enum : UINT { ID_OPEN = 100, ID_STUDIO, ID_AUTOSTART, ID_QUIT, ID_PLAY_FIRST = 1000 };

HINSTANCE g_inst;
HWND g_wnd;
NOTIFYICONDATAW g_nid{};
UINT g_taskbar_created;
fs::path g_root, g_exe;
HANDLE g_server = nullptr;
std::mutex g_mu;                 // guards the fields below, written by the reader thread
std::deque<std::string> g_tail;  // the server's last lines, for an error report
std::string g_url, g_data, g_version;
int g_http_port = 0;
bool g_ready = false, g_open_when_ready = false, g_quitting = false;
std::atomic<bool> g_watching{false};
std::vector<std::pair<std::string, std::string>> g_profiles;   // (id, name) of the last Play menu

std::wstring widen(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
  return w;
}

std::string narrow(const std::wstring& w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}

std::wstring quote(const std::wstring& a) {
  if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
  std::wstring q = L"\"";
  size_t bs = 0;
  for (wchar_t ch : a) {
    if (ch == L'\\') { ++bs; continue; }
    if (ch == L'"') q.append(bs * 2 + 1, L'\\'); else q.append(bs, L'\\');
    bs = 0; q += ch;
  }
  q.append(bs * 2, L'\\');
  return q + L"\"";
}

void error_box(const std::wstring& text) {
  MessageBoxW(nullptr, text.c_str(), L"PascalPatch", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

void balloon(const std::wstring& title, const std::wstring& text) {
  NOTIFYICONDATAW n = g_nid;
  n.uFlags = NIF_INFO;
  n.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
  n.hBalloonIcon = g_nid.hIcon;
  lstrcpynW(n.szInfoTitle, title.c_str(), ARRAYSIZE(n.szInfoTitle));
  lstrcpynW(n.szInfo, text.c_str(), ARRAYSIZE(n.szInfo));
  Shell_NotifyIconW(NIM_MODIFY, &n);
}

// ---- the server ----

// A request to the app's own server; returns (status, body), status 0 when it could not be reached.
std::pair<int, std::string> http(const wchar_t* method, const std::wstring& path, const std::string& body = {}) {
  int port;
  { std::lock_guard<std::mutex> l(g_mu); port = g_http_port; }
  std::pair<int, std::string> out{0, {}};
  if (!port) return out;
  HINTERNET s = WinHttpOpen(L"PascalPatch", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
  if (!s) return out;
  WinHttpSetTimeouts(s, 2000, 2000, 5000, 15000);
  if (HINTERNET c = WinHttpConnect(s, L"127.0.0.1", (INTERNET_PORT)port, 0)) {
    if (HINTERNET r = WinHttpOpenRequest(c, method, path.c_str(), nullptr, nullptr, nullptr, 0)) {
      const wchar_t* headers = L"Content-Type: application/json\r\n";
      if (WinHttpSendRequest(r, headers, (DWORD)-1, (void*)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) &&
          WinHttpReceiveResponse(r, nullptr)) {
        DWORD code = 0, len = sizeof code;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &len, nullptr);
        out.first = (int)code;
        char buf[8192];
        DWORD got = 0;
        while (WinHttpReadData(r, buf, sizeof buf, &got) && got) out.second.append(buf, got);
      }
      WinHttpCloseHandle(r);
    }
    WinHttpCloseHandle(c);
  }
  WinHttpCloseHandle(s);
  return out;
}

std::wstring error_of(const std::pair<int, std::string>& r) {
  if (r.first == 0) return L"PascalPatch is not responding.";
  try { return widen(json::parse(r.second).value("error", std::string("something went wrong"))); }
  catch (...) { return L"something went wrong"; }
}

fs::path find_python() {
  fs::path bundled = g_root / L"python" / L"python.exe";   // a release download carries its own
  if (fs::is_regular_file(bundled)) return bundled;
  wchar_t found[MAX_PATH];
  if (SearchPathW(nullptr, L"python.exe", nullptr, MAX_PATH, found, nullptr)) return found;
  return {};
}

void read_server(HANDLE out) {
  std::string pending;
  char buf[4096];
  DWORD got = 0;
  while (ReadFile(out, buf, sizeof buf, &got, nullptr) && got) {
    pending.append(buf, got);
    size_t nl;
    while ((nl = pending.find('\n')) != std::string::npos) {
      std::string line = pending.substr(0, nl);
      pending.erase(0, nl + 1);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      static const std::string kReady = "PASCALPATCH_READY ";
      if (line.rfind(kReady, 0) == 0) {
        try {
          json j = json::parse(line.substr(kReady.size()));
          std::lock_guard<std::mutex> l(g_mu);
          g_url = j.at("url").get<std::string>();
          g_data = j.value("data", std::string());
          g_version = j.value("version", std::string());
          size_t colon = g_url.rfind(':');
          g_http_port = colon == std::string::npos ? 0 : std::atoi(g_url.c_str() + colon + 1);
        } catch (...) {}
        PostMessageW(g_wnd, WM_READY, 0, 0);
        continue;
      }
      std::lock_guard<std::mutex> l(g_mu);
      g_tail.push_back(line);
      if (g_tail.size() > 25) g_tail.pop_front();
    }
  }
  CloseHandle(out);
  WaitForSingleObject(g_server, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(g_server, &code);
  PostMessageW(g_wnd, WM_SERVER_EXIT, code, 0);
}

bool start_server() {
  fs::path python = find_python();
  if (python.empty()) {
    error_box(L"PascalPatch cannot find its Python.\n\nUnzip the whole download and run PascalPatch.exe from the "
              L"unzipped folder: opening it from inside the zip leaves its Python behind.");
    return false;
  }
  // a source checkout uses the installed Python, which needs the host package on its path
  std::wstring pp = (g_root / L"host" / L"src").wstring();
  wchar_t old[8192];
  DWORD n = GetEnvironmentVariableW(L"PYTHONPATH", old, ARRAYSIZE(old));
  if (n && n < ARRAYSIZE(old)) pp += L";" + std::wstring(old);
  SetEnvironmentVariableW(L"PYTHONPATH", pp.c_str());
  SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");
  SetEnvironmentVariableW(L"PYTHONUNBUFFERED", L"1");

  SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
  HANDLE rd = nullptr, wr = nullptr;
  if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
  STARTUPINFOW si{sizeof si};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = nul; si.hStdOutput = wr; si.hStdError = wr;
  std::wstring cmd = quote(python.wstring()) + L" -m pascalpatch.cli --root " + quote(g_root.wstring()) + L" app --tray";
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                           g_root.c_str(), &si, &pi);
  CloseHandle(wr);
  if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
  if (!ok) {
    CloseHandle(rd);
    error_box(L"PascalPatch could not start its Python (" + python.wstring() + L").");
    return false;
  }
  CloseHandle(pi.hThread);
  g_server = pi.hProcess;
  std::thread(read_server, rd).detach();
  return true;
}

void stop_server() {
  if (!g_server) return;
  if (WaitForSingleObject(g_server, 0) == WAIT_TIMEOUT) {
    http(L"POST", L"/api/quit", "{}");
    if (WaitForSingleObject(g_server, 5000) == WAIT_TIMEOUT) TerminateProcess(g_server, 1);
  }
}

// ---- the window ----

fs::path find_browser() {
  const wchar_t* candidates[] = {
      L"%ProgramFiles(x86)%\\Microsoft\\Edge\\Application\\msedge.exe",
      L"%ProgramFiles%\\Microsoft\\Edge\\Application\\msedge.exe",
      L"%ProgramFiles%\\Google\\Chrome\\Application\\chrome.exe",
      L"%LocalAppData%\\Google\\Chrome\\Application\\chrome.exe",
  };
  for (const wchar_t* c : candidates) {
    wchar_t path[MAX_PATH];
    if (ExpandEnvironmentStringsW(c, path, MAX_PATH) && fs::is_regular_file(path)) return path;
  }
  return {};
}

HWND find_app_window() {
  HWND found = nullptr;
  EnumWindows([](HWND w, LPARAM out) -> BOOL {
    wchar_t cls[64], title[64];
    if (!IsWindowVisible(w) || !GetClassNameW(w, cls, 64) || wcscmp(cls, L"Chrome_WidgetWin_1") != 0) return TRUE;
    if (GetWindowTextW(w, title, 64) && wcscmp(title, kWindowTitle) == 0) { *(HWND*)out = w; return FALSE; }
    return TRUE;
  }, (LPARAM)&found);
  return found;
}

fs::path window_profile() {
  std::lock_guard<std::mutex> l(g_mu);
  return fs::path(widen(g_data)) / L"window-profile" / L"pascalpatch";   // the one app/window.py uses
}

// Chromium holds <profile>/lockfile open while any of that profile's windows is up.
bool profile_in_use(const fs::path& profile) {
  fs::path lock = profile / L"lockfile";
  if (!fs::exists(lock)) return false;
  HANDLE h = CreateFileW(lock.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_SHARING_VIOLATION;
  CloseHandle(h);   // a stale lock left by a crash
  return false;
}

void watch_window(fs::path profile) {
  for (int i = 0; i < 120 && !profile_in_use(profile); ++i) Sleep(250);
  while (profile_in_use(profile)) Sleep(1000);
  g_watching = false;
  PostMessageW(g_wnd, WM_WINDOW_CLOSED, 0, 0);
}

void open_window() {
  std::string url;
  {
    std::lock_guard<std::mutex> l(g_mu);
    if (!g_ready) { g_open_when_ready = true; return; }
    url = g_url;
  }
  if (HWND w = find_app_window()) {
    if (IsIconic(w)) ShowWindow(w, SW_RESTORE);
    SetForegroundWindow(w);
    return;
  }
  fs::path browser = find_browser();
  if (browser.empty()) {   // no Edge or Chrome: the default browser
    ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return;
  }
  fs::path profile = window_profile();
  std::error_code ec;
  fs::create_directories(profile, ec);
  std::wstring cmd = quote(browser.wstring()) + L" --app=" + widen(url) + L" --user-data-dir=" + quote(profile.wstring()) +
                     L" --window-size=1320,860 --no-first-run --no-default-browser-check --disable-extensions";
  STARTUPINFOW si{sizeof si};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return;
  }
  CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
  if (!g_watching.exchange(true)) std::thread(watch_window, profile).detach();
}

// The first time the window closes, say where PascalPatch went.
void window_closed() {
  if (g_quitting) return;
  fs::path marker;
  { std::lock_guard<std::mutex> l(g_mu); marker = fs::path(widen(g_data)) / L"tray-hint-shown"; }
  if (fs::exists(marker)) return;
  balloon(L"PascalPatch is still running",
          L"It stays here in the tray. Click the icon to open it again, or right-click it to quit.");
  HANDLE h = CreateFileW(marker.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
  if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

// ---- Start with Windows ----

std::wstring autostart_command() { return quote(g_exe.wstring()) + L" --tray"; }

bool autostart_on() {
  return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

void set_autostart(bool on) {
  HKEY k;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
  if (on) {
    std::wstring v = autostart_command();
    RegSetValueExW(k, kRunValue, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
  } else {
    RegDeleteValueW(k, kRunValue);
  }
  RegCloseKey(k);
}

// ---- the tray menu ----

void play(size_t i) {
  if (i >= g_profiles.size()) return;
  auto [id, name] = g_profiles[i];
  auto r = http(L"POST", L"/api/launch", json{{"id", id}}.dump());
  if (r.first == 200) balloon(L"Starting " + widen(name), L"Building the game with your plugins, then starting Melee.");
  else balloon(L"Could not start " + widen(name), error_of(r));
}

void show_menu() {
  bool ready;
  { std::lock_guard<std::mutex> l(g_mu); ready = g_ready; }
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING | (ready ? 0 : MF_GRAYED), ID_OPEN, ready ? L"Open PascalPatch" : L"PascalPatch is starting...");
  SetMenuDefaultItem(menu, ID_OPEN, FALSE);
  HMENU plays = CreatePopupMenu();
  g_profiles.clear();
  if (ready) {
    auto r = http(L"GET", L"/api/profiles");
    try {
      for (auto& p : json::parse(r.second)) g_profiles.emplace_back(p.at("id").get<std::string>(), p.value("name", p.at("id").get<std::string>()));
    } catch (...) {}
  }
  for (size_t i = 0; i < g_profiles.size() && i < 30; ++i) AppendMenuW(plays, MF_STRING, ID_PLAY_FIRST + i, widen(g_profiles[i].second).c_str());
  if (g_profiles.empty()) AppendMenuW(plays, MF_STRING | MF_GRAYED, 0, L"No profiles yet: make one in PascalPatch");
  AppendMenuW(menu, MF_POPUP | (ready ? 0 : MF_GRAYED), (UINT_PTR)plays, L"Play");
  AppendMenuW(menu, MF_STRING | (ready ? 0 : MF_GRAYED), ID_STUDIO, L"Character Studio");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (autostart_on() ? MF_CHECKED : 0), ID_AUTOSTART, L"Start with Windows");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, ID_QUIT, L"Quit PascalPatch");
  POINT pt;
  GetCursorPos(&pt);
  SetForegroundWindow(g_wnd);   // so the menu closes when the user clicks elsewhere
  UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, g_wnd, nullptr);
  PostMessageW(g_wnd, WM_NULL, 0, 0);
  DestroyMenu(menu);   // and its Play submenu
  if (cmd == ID_OPEN) open_window();
  else if (cmd == ID_STUDIO) {
    auto r = http(L"POST", L"/api/studio", "{}");
    if (r.first == 200) balloon(L"Character Studio", L"Opening Character Studio...");
    else balloon(L"Could not open Character Studio", error_of(r));
  } else if (cmd == ID_AUTOSTART) set_autostart(!autostart_on());
  else if (cmd == ID_QUIT) DestroyWindow(g_wnd);
  else if (cmd >= ID_PLAY_FIRST) play(cmd - ID_PLAY_FIRST);
}

void add_icon() {
  Shell_NotifyIconW(NIM_ADD, &g_nid);
  g_nid.uVersion = NOTIFYICON_VERSION_4;
  Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

void set_tip(const std::wstring& tip) {
  lstrcpynW(g_nid.szTip, tip.c_str(), ARRAYSIZE(g_nid.szTip));
  g_nid.uFlags = NIF_TIP | NIF_SHOWTIP;
  Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

LRESULT CALLBACK wndproc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == g_taskbar_created && g_taskbar_created) { add_icon(); return 0; }   // Explorer restarted
  switch (msg) {
    case WM_TRAY:
      switch (LOWORD(lp)) {
        case NIN_SELECT: case NIN_KEYSELECT: open_window(); break;
        case WM_CONTEXTMENU: show_menu(); break;
      }
      return 0;
    case WM_SHOW_WINDOW: open_window(); return 0;
    case WM_READY: {
      bool open;
      std::wstring version;
      {
        std::lock_guard<std::mutex> l(g_mu);
        g_ready = true; open = g_open_when_ready; version = widen(g_version);
      }
      set_tip(version.empty() ? L"PascalPatch" : L"PascalPatch " + version);
      if (open) open_window();
      return 0;
    }
    case WM_WINDOW_CLOSED: window_closed(); return 0;
    case WM_SERVER_EXIT: {
      if (g_quitting) return 0;
      std::wstring tail;
      {
        std::lock_guard<std::mutex> l(g_mu);
        for (auto& line : g_tail) tail += widen(line) + L"\n";
      }
      Shell_NotifyIconW(NIM_DELETE, &g_nid);
      error_box(L"PascalPatch stopped unexpectedly (exit code " + std::to_wstring((DWORD)wp) + L").\n\n" +
                (tail.empty() ? L"It printed nothing." : L"Its last lines:\n\n" + tail));
      g_quitting = true;
      DestroyWindow(w);
      return 0;
    }
    case WM_DESTROY:
      g_quitting = true;
      if (HWND app = find_app_window()) PostMessageW(app, WM_CLOSE, 0, 0);
      Shell_NotifyIconW(NIM_DELETE, &g_nid);
      stop_server();
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(w, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
  g_inst = inst;
  bool tray_only = false;
  for (int i = 1; i < __argc; ++i)
    if (_wcsicmp(__wargv[i], L"--tray") == 0) tray_only = true;

  // One PascalPatch at a time: a second start shows the first one's window.
  HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutex);
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    if (!tray_only) {
      for (int i = 0; i < 50; ++i) {   // the first copy may still be creating its tray window
        if (HWND other = FindWindowW(kClass, nullptr)) {
          AllowSetForegroundWindow(ASFW_ANY);
          PostMessageW(other, WM_SHOW_WINDOW, 0, 0);
          break;
        }
        Sleep(100);
      }
    }
    return 0;
  }

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  wchar_t self[MAX_PATH];
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  g_exe = self;
  g_root = g_exe.parent_path();
  if (autostart_on()) set_autostart(true);   // follow this copy after an update unzips to a new folder

  WNDCLASSEXW wc{sizeof wc};
  wc.lpfnWndProc = wndproc;
  wc.hInstance = inst;
  wc.lpszClassName = kClass;
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP));
  RegisterClassExW(&wc);
  // hidden, but top-level: a message-only window gets neither TaskbarCreated nor FindWindow
  g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClass, L"PascalPatch", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
  g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
  ChangeWindowMessageFilterEx(g_wnd, g_taskbar_created, MSGFLT_ALLOW, nullptr);

  g_nid.cbSize = sizeof g_nid;
  g_nid.hWnd = g_wnd;
  g_nid.uID = 1;
  g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
  g_nid.uCallbackMessage = WM_TRAY;
  g_nid.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
  lstrcpynW(g_nid.szTip, L"PascalPatch (starting...)", ARRAYSIZE(g_nid.szTip));
  add_icon();

  g_open_when_ready = !tray_only;
  if (!start_server()) {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    return 1;
  }
  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  if (g_server) CloseHandle(g_server);
  ReleaseMutex(mutex);
  CloseHandle(mutex);
  return 0;
}
