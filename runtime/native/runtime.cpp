// PascalPatch runtime: injected into an unmodified melee_port.exe by pascalpatch-launch.exe, the way
// BakkesMod injects into Rocket League. Nothing in the port is rebuilt or patched on disk.
//
// The port runs translated PowerPC code, and every guest call goes through one table of host
// function pointers indexed by guest address (ppc::g_dispatch, a std::vector in the exe's data).
// The runtime finds that table in memory, stands in front of the VI retrace handler (once per frame) to
// load plugins and run their frame callbacks, and serves the pp_host ABI (sdk/include/pascalpatch/
// plugin.h) on top of the same table, plus the F2 overlay (overlay.cpp). It also keeps the process offline: network imports are
// replaced with refusals, so nothing reaches Slippi or any other server.
// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>

#include "pascalpatch/plugin.h"
#include "pp_runtime.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

// ---- the port's layout (melee-unlocked port/runtime/ppc/ppc.h) ----
constexpr uint32_t RAM_BASE = 0x80000000u;
constexpr uint32_t RAM_SIZE = 0x01800000u;
constexpr uint32_t START = 0x8000522Cu;    // __start: always translated
// __VIRetraceHandler (NTSC 1.02): the port delivers the VI interrupt once per frame through the
// guest interrupt table, i.e. through dispatch. (PADRead is reached by direct calls, which bypass it.)
constexpr uint32_t FRAME_FN = 0x8034E964u;
struct FPR { double ps0, ps1; };
struct Context { uint32_t r[32]; FPR f[32]; uint8_t cr[8]; uint32_t lr; };  // leading fields of ppc::Context
using Fn = void (*)(Context&, uint8_t*);
struct VectorRep { Fn* begin; Fn* end; Fn* cap; };  // MSVC std::vector<Fn>

Fn* g_table = nullptr;
uint8_t* g_ram = nullptr;
Fn g_frame_original = nullptr;
std::atomic<bool> g_ready{false};
HANDLE g_log = INVALID_HANDLE_VALUE;

void vlog(const char* fmt, va_list ap) {
  char buf[1024];
  int n = std::vsnprintf(buf, sizeof buf - 2, fmt, ap);
  if (n < 0) return;
  n = std::min(n, (int)sizeof buf - 2);
  {
    std::lock_guard<std::mutex> lock(pp::mutex());
    auto& c = pp::console();
    c.emplace_back(buf, n);
    while (c.size() > 400) c.pop_front();
  }
  buf[n++] = '\n';
  DWORD w;
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out && out != INVALID_HANDLE_VALUE) WriteFile(out, buf, n, &w, nullptr);
  if (g_log != INVALID_HANDLE_VALUE) WriteFile(g_log, buf, n, &w, nullptr);
}
void log(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog(fmt, ap); va_end(ap); }

std::wstring env(const wchar_t* name) {
  wchar_t buf[2048];
  DWORD n = GetEnvironmentVariableW(name, buf, 2048);
  return n && n < 2048 ? std::wstring(buf, n) : std::wstring();
}

// ---- offline guard: the process's network imports refuse everything ----
int WSAAPI no_connect(SOCKET, const sockaddr*, int) { WSASetLastError(WSAEACCES); return SOCKET_ERROR; }
int WSAAPI no_wsaconnect(SOCKET, const sockaddr*, int, LPWSABUF, LPWSABUF, LPQOS, LPQOS) { WSASetLastError(WSAEACCES); return SOCKET_ERROR; }
int WSAAPI no_sendto(SOCKET, const char*, int, int, const sockaddr*, int) { WSASetLastError(WSAEACCES); return SOCKET_ERROR; }
int WSAAPI no_wsasendto(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, const sockaddr*, int, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE) { WSASetLastError(WSAEACCES); return SOCKET_ERROR; }
int WSAAPI no_getaddrinfo(PCSTR, PCSTR, const ADDRINFOA*, PADDRINFOA* r) { if (r) *r = nullptr; return WSAHOST_NOT_FOUND; }
int WSAAPI no_getaddrinfow(PCWSTR, PCWSTR, const ADDRINFOW*, PADDRINFOW* r) { if (r) *r = nullptr; return WSAHOST_NOT_FOUND; }
hostent* WSAAPI no_gethostbyname(const char*) { WSASetLastError(WSAHOST_NOT_FOUND); return nullptr; }
void* WINAPI no_session(...) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }  // WinHttpOpen / InternetOpen

using CreateFileW_t = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileW_t g_create_file = nullptr;
// Discord rich presence talks over a local pipe; it would announce the session, so it is refused too.
HANDLE WINAPI guarded_create_file(LPCWSTR name, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t) {
  if (name && std::wcsstr(name, L"discord-ipc")) { SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
  return g_create_file(name, a, s, sa, d, f, t);
}

struct Block { const char* dll; const char* fn; void* repl; };

// Replaces every import slot, in every loaded module, that points at one of the blocked functions.
// Slots are matched by the resolved address, so imports by ordinal (ws2_32's usual form) are covered.
int guard_imports() {
  const Block blocks[] = {
    {"ws2_32.dll", "connect", (void*)&no_connect}, {"ws2_32.dll", "WSAConnect", (void*)&no_wsaconnect},
    {"ws2_32.dll", "sendto", (void*)&no_sendto}, {"ws2_32.dll", "WSASendTo", (void*)&no_wsasendto},
    {"ws2_32.dll", "getaddrinfo", (void*)&no_getaddrinfo}, {"ws2_32.dll", "GetAddrInfoW", (void*)&no_getaddrinfow},
    {"ws2_32.dll", "gethostbyname", (void*)&no_gethostbyname},
    {"winhttp.dll", "WinHttpOpen", (void*)&no_session},
    {"wininet.dll", "InternetOpenA", (void*)&no_session}, {"wininet.dll", "InternetOpenW", (void*)&no_session},
    {"kernel32.dll", "CreateFileW", (void*)&guarded_create_file},
  };
  std::vector<std::pair<void*, void*>> targets;  // (real function, replacement)
  for (const Block& b : blocks) {
    HMODULE m = GetModuleHandleA(b.dll);
    void* real = m ? (void*)GetProcAddress(m, b.fn) : nullptr;
    if (!real) continue;
    targets.push_back({real, b.repl});
    // kernel32's CreateFileW forwards to kernelbase, which is also what the guard calls on.
    if (!std::strcmp(b.dll, "kernel32.dll")) {
      HMODULE kb = GetModuleHandleA("kernelbase.dll");
      void* k = kb ? (void*)GetProcAddress(kb, b.fn) : nullptr;
      g_create_file = (CreateFileW_t)(k ? k : real);
      if (k) targets.push_back({k, b.repl});
    }
  }
  HMODULE self = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&guard_imports, &self);
  int patched = 0;
  wchar_t windir[MAX_PATH]; UINT wl = GetWindowsDirectoryW(windir, MAX_PATH);
  HMODULE mods[1024]; DWORD need = 0;
  if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &need)) return 0;
  for (DWORD i = 0; i < need / sizeof(HMODULE); ++i) {
    auto* base = (uint8_t*)mods[i];
    if (mods[i] == self) continue;
    // Only the port and the libraries it ships; Windows' own modules forward among themselves.
    wchar_t path[MAX_PATH];
    if (wl && GetModuleFileNameW(mods[i], path, MAX_PATH) && !_wcsnicmp(path, windir, wl)) continue;
    auto* dos = (IMAGE_DOS_HEADER*)base;
    auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) continue;
    for (auto* d = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); d->Name; ++d) {
      for (auto* t = (IMAGE_THUNK_DATA*)(base + d->FirstThunk); t->u1.Function; ++t) {
        for (auto& tg : targets) {
          if ((void*)t->u1.Function != tg.first) continue;
          DWORD old;
          if (VirtualProtect(&t->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) {
            t->u1.Function = (ULONG_PTR)tg.second;
            VirtualProtect(&t->u1.Function, sizeof(void*), old, &old);
            ++patched;
          }
        }
      }
    }
  }
  return patched;
}

// ---- finding the dispatch table ----
bool in_image(const void* p, HMODULE exe) {
  auto* base = (uint8_t*)exe;
  auto* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
  return (uint8_t*)p >= base && (uint8_t*)p < base + nt->OptionalHeader.SizeOfImage;
}

// The table is the only vector in the exe's writable data spanning exactly RAM_SIZE / 4 entries.
Fn* find_table(HMODULE exe) {
  auto* base = (uint8_t*)exe;
  auto* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
  auto* sec = IMAGE_FIRST_SECTION(nt);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
    if (!(sec->Characteristics & IMAGE_SCN_MEM_WRITE)) continue;
    auto* p = base + sec->VirtualAddress;
    size_t n = std::max(sec->Misc.VirtualSize, sec->SizeOfRawData);
    for (size_t off = 0; off + sizeof(VectorRep) <= n; off += 8) {
      auto* v = (VectorRep*)(p + off);
      if (!v->begin || (uint8_t*)v->end - (uint8_t*)v->begin != (ptrdiff_t)(RAM_SIZE / 4 * sizeof(Fn)) || v->cap < v->end) continue;
      Fn s = v->begin[(START - RAM_BASE) / 4], pad = v->begin[(FRAME_FN - RAM_BASE) / 4];
      if (s && pad && in_image((void*)s, exe) && in_image((void*)pad, exe)) return v->begin;
    }
  }
  return nullptr;
}

Fn& entry(uint32_t addr) { return g_table[(addr - RAM_BASE) / 4]; }
bool code_addr(uint32_t addr) { return addr - RAM_BASE < RAM_SIZE && !(addr & 3); }

// ---- plugin host (same behaviour as the in-port loader it replaces) ----
constexpr uint32_t SLOT_BASE = 0x80001800u;
constexpr size_t SLOT_COUNT = 1024;
// Direct guest calls (`bl`) are translated into direct host calls that never read the dispatch
// table, so a hook also patches the translated function's own entry, in memory only: a 5-byte
// jump to a stub that jumps on to the hook's front thunk. The original runs by putting its first
// bytes back for the duration of the call; a recursive call made meanwhile skips the hook.
struct Inline { uint8_t* fn; uint8_t saved[5]; uint8_t* stub; int running = 0; };
struct Slot { pp_guest_fn fn = nullptr; void* user = nullptr; Fn original = nullptr; Inline* inl = nullptr; };
std::array<Slot, SLOT_COUNT> g_slots;
size_t g_used = 1;   // slot 0 is the return sentinel for host-initiated calls
std::vector<std::pair<pp_frame_fn, void*>> g_frame;
std::vector<HMODULE> g_modules;

template <size_t I> void thunk(Context& c, uint8_t* m) {
  Slot& s = g_slots[I];
  if (s.original) {
    if (!s.inl || s.inl->running) { s.original(c, m); return; }
    Inline& h = *s.inl;
    uint8_t patched[5]; std::memcpy(patched, h.fn, 5);
    std::memcpy(h.fn, h.saved, 5);
    ++h.running;
    struct Repatch { Inline& h; uint8_t* p; ~Repatch() { --h.running; std::memcpy(h.fn, p, 5); } } again{h, patched};
    s.original(c, m);   // guest longjmp unwinds through here as an exception; the guard repatches
  } else if (s.fn) s.fn(reinterpret_cast<pp_cpu*>(&c), s.user);
}
template <size_t... I> constexpr std::array<Fn, sizeof...(I)> make_thunks(std::index_sequence<I...>) { return {&thunk<I>...}; }
const std::array<Fn, SLOT_COUNT> g_thunks = make_thunks(std::make_index_sequence<SLOT_COUNT>{});
void sentinel(Context&, uint8_t*) {}

int claim(pp_guest_fn fn, void* user, Fn original) {
  if (g_used >= SLOT_COUNT) return -1;
  int i = (int)g_used++;
  g_slots[i] = {fn, user, original};
  entry(SLOT_BASE + 4u * i) = g_thunks[i];
  return i;
}

uint8_t* at(uint32_t a, uint32_t bytes) {
  uint32_t off = a & 0x3FFFFFFFu;
  if (off > RAM_SIZE - bytes) { log("[pascalpatch] guest access outside RAM: %08X", a); static uint8_t junk[8]; std::memset(junk, 0, 8); return junk; }
  return g_ram + off;
}
Context& cpu(pp_cpu* c) { return *reinterpret_cast<Context*>(c); }

void api_log(const char* plugin, const char* msg) {
  log("[mod:%s] %s", plugin ? plugin : "?", msg ? msg : "");
  std::lock_guard<std::mutex> lock(pp::mutex());
  if (pp::Plugin* p = pp::find(plugin ? plugin : "")) {
    p->log.emplace_back(msg ? msg : "");
    while (p->log.size() > 60) p->log.pop_front();
  }
}
uint8_t api_rd8(uint32_t a) { return *at(a, 1); }
uint16_t api_rd16(uint32_t a) { uint16_t v; std::memcpy(&v, at(a, 2), 2); return _byteswap_ushort(v); }
uint32_t api_rd32(uint32_t a) { uint32_t v; std::memcpy(&v, at(a, 4), 4); return _byteswap_ulong(v); }
float api_rdf32(uint32_t a) { uint32_t v = api_rd32(a); float f; std::memcpy(&f, &v, 4); return f; }
void api_wr8(uint32_t a, uint8_t v) { *at(a, 1) = v; }
void api_wr16(uint32_t a, uint16_t v) { v = _byteswap_ushort(v); std::memcpy(at(a, 2), &v, 2); }
void api_wr32(uint32_t a, uint32_t v) { v = _byteswap_ulong(v); std::memcpy(at(a, 4), &v, 4); }
void api_wrf32(uint32_t a, float f) { uint32_t v; std::memcpy(&v, &f, 4); api_wr32(a, v); }
uint32_t api_reg(pp_cpu* c, int n) { return cpu(c).r[n & 31]; }
void api_set_reg(pp_cpu* c, int n, uint32_t v) { cpu(c).r[n & 31] = v; }
double api_freg(pp_cpu* c, int n) { return cpu(c).f[n & 31].ps0; }
void api_set_freg(pp_cpu* c, int n, double v) { cpu(c).f[n & 31].ps0 = cpu(c).f[n & 31].ps1 = v; }
// Calls through the dispatch table. Code that exists only in RAM (dat-loaded routines) has no
// entry; the port interprets those, which the runtime cannot reach from outside, so it refuses.
void api_call(pp_cpu* c, uint32_t addr) {
  Fn fn = code_addr(addr) ? entry(addr) : nullptr;
  if (!fn) { log("[pascalpatch] call to %08X refused: no translated function there", addr); return; }
  Context& x = cpu(c);
  uint32_t saved = x.lr;
  x.lr = SLOT_BASE;   // a return lands on the sentinel, which does nothing
  fn(x, g_ram);
  x.lr = saved;
}
uint32_t api_trampoline(pp_guest_fn fn, void* user) { int i = claim(fn, user, nullptr); return i < 0 ? 0 : SLOT_BASE + 4u * i; }
std::map<uint32_t, Inline*> g_inline;   // guest address -> its patched translated function
uint8_t* g_stubs = nullptr; size_t g_stub_used = 0;
constexpr size_t STUB_PAGE = 0x10000, STUB_SIZE = 16;

// A page within +-2 GB of the exe, so a 5-byte relative jump from any translated function reaches it.
uint8_t* stub_page(uint8_t* near_to) {
  SYSTEM_INFO si; GetSystemInfo(&si);
  uintptr_t gran = si.dwAllocationGranularity, base = (uintptr_t)near_to & ~(gran - 1);
  for (uintptr_t d = gran; d < 0x70000000u; d += gran)
    for (uintptr_t a : {base + d, base - d})
      if (void* p = VirtualAlloc((void*)a, STUB_PAGE, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
  return nullptr;
}

void set_stub_target(Inline& h, Fn target) {
  uint8_t jmp[6] = {0xFF, 0x25, 0, 0, 0, 0};   // jmp [rip+0]
  std::memcpy(h.stub, jmp, 6);
  std::memcpy(h.stub + 6, &target, 8);
  FlushInstructionCache(GetCurrentProcess(), h.stub, STUB_SIZE);
}

Inline* patch_entry(uint32_t addr, Fn host, Fn target) {
  auto it = g_inline.find(addr);
  if (it != g_inline.end()) { set_stub_target(*it->second, target); return nullptr; }   // already patched: re-aim
  if (!in_image((void*)host, GetModuleHandleW(nullptr))) return nullptr;
  // HLE stand-ins can be shared by several guest addresses; patching one would move them all.
  for (size_t i = 0; i < RAM_SIZE / 4; ++i)
    if (g_table[i] == host && RAM_BASE + 4u * (uint32_t)i != addr) {
      log("[pascalpatch] hook %08X: host function shared with %08X, pointer calls only", addr, RAM_BASE + 4u * (uint32_t)i);
      return nullptr;
    }
  if (!g_stubs) g_stubs = stub_page((uint8_t*)host);
  if (!g_stubs || (g_stub_used + 1) * STUB_SIZE > STUB_PAGE) { log("[pascalpatch] hook %08X: no stub space, pointer calls only", addr); return nullptr; }
  auto* h = new Inline{(uint8_t*)host, {}, g_stubs + STUB_SIZE * g_stub_used++};
  set_stub_target(*h, target);
  DWORD old;
  if (!VirtualProtect(h->fn, 5, PAGE_EXECUTE_READWRITE, &old)) { log("[pascalpatch] hook %08X: code not writable", addr); return nullptr; }
  std::memcpy(h->saved, h->fn, 5);
  int32_t rel = (int32_t)(h->stub - (h->fn + 5));
  h->fn[0] = 0xE9; std::memcpy(h->fn + 1, &rel, 4);
  FlushInstructionCache(GetCurrentProcess(), h->fn, 5);
  g_inline[addr] = h;
  return h;
}

uint32_t api_hook(uint32_t addr, pp_guest_fn fn, void* user) {
  if (!code_addr(addr)) return 0;
  Fn previous = entry(addr);
  if (!previous) return 0;
  int orig = claim(nullptr, nullptr, previous);
  int front = claim(fn, user, nullptr);
  if (orig < 0 || front < 0) return 0;
  entry(addr) = g_thunks[front];
  g_slots[orig].inl = patch_entry(addr, previous, g_thunks[front]);
  return SLOT_BASE + 4u * orig;
}
void api_on_frame(pp_frame_fn fn, void* user) { if (fn) g_frame.push_back({fn, user}); }
constexpr uint32_t ARENA_END = 0x80003000u;   // shares the slot range: slots use dispatch entries, not RAM
uint32_t g_arena = SLOT_BASE;
uint32_t api_guest_alloc(uint32_t size) {
  uint32_t a = (g_arena + 31u) & ~31u;
  if (size == 0 || size > ARENA_END - a) return 0;
  std::memset(at(a, size), 0, size);
  g_arena = a + size;
  return a;
}

// ---- settings (shown and edited in the F2 overlay) ----
using json = nlohmann::json;

pp::Setting parse_setting(const json& j) {
  pp::Setting s;
  s.key = j.value("key", "");
  s.type = j.value("type", "bool");
  s.label = j.value("label", s.key);
  s.help = j.value("help", "");
  s.group = j.value("group", "");
  s.min = j.value("min", 0.0);
  s.max = j.value("max", s.type == "int" ? 100.0 : 1.0);
  s.step = j.value("step", 0.0);
  if (j.count("options") && j["options"].is_array())
    for (auto& o : j["options"]) {
      if (o.is_object()) {
        s.options.push_back(o.value("value", ""));
        s.option_labels.push_back(o.value("label", s.options.back()));
      } else if (o.is_string()) {
        s.options.push_back(o.get<std::string>());
        s.option_labels.push_back(s.options.back());
      }
    }
  const json def = j.count("default") ? j["default"] : json();
  if (s.type == "text" || s.type == "key") {
    s.def_text = def.is_string() ? def.get<std::string>() : "";
  } else if (s.type == "choice") {
    std::string v = def.is_string() ? def.get<std::string>() : (s.options.empty() ? "" : s.options[0]);
    auto it = std::find(s.options.begin(), s.options.end(), v);
    s.def = it == s.options.end() ? 0 : (double)(it - s.options.begin());
    s.def_text = s.options.empty() ? "" : s.options[(size_t)s.def];
  } else {
    s.def = def.is_boolean() ? (def.get<bool>() ? 1 : 0) : def.is_number() ? def.get<double>() : 0;
  }
  s.num = s.def;
  s.text = s.def_text;
  return s;
}

void apply_value(pp::Setting& s, const json& v) {
  if (s.type == "text" || s.type == "key") { if (v.is_string()) s.text = v.get<std::string>(); return; }
  if (s.type == "choice") {
    if (!v.is_string()) return;
    auto it = std::find(s.options.begin(), s.options.end(), v.get<std::string>());
    if (it != s.options.end()) { s.num = (double)(it - s.options.begin()); s.text = *it; }
    return;
  }
  double x = v.is_boolean() ? (v.get<bool>() ? 1 : 0) : v.is_number() ? v.get<double>() : s.num;
  s.num = s.type == "bool" ? (x != 0 ? 1 : 0) : std::clamp(x, s.min, s.max);
}

json read_json(const std::filesystem::path& p) {
  std::ifstream f(p);
  if (!f) return json();
  return json::parse(f, nullptr, false);
}

void write_json(const std::filesystem::path& p, const json& j) {
  std::error_code ec;
  std::filesystem::create_directories(p.parent_path(), ec);
  auto tmp = p; tmp += ".tmp";
  { std::ofstream f(tmp, std::ios::trunc); f << j.dump(2) << "\n"; }
  std::filesystem::rename(tmp, p, ec);
}

std::map<std::string, json> g_configs;   // plugin id -> its staged <id>.json

// A setting starts at its default, then the staged config's "settings", then what was saved last run.
void load_value(const pp::Plugin& p, pp::Setting& s) {
  const json& cfg = g_configs[p.id];
  if (cfg.is_object() && cfg.count("settings") && cfg["settings"].is_object() && cfg["settings"].count(s.key))
    apply_value(s, cfg["settings"][s.key]);
  std::string dir = pp::settings_dir();
  if (dir.empty()) return;
  json saved = read_json(std::filesystem::u8path(dir) / (p.id + ".json"));
  if (saved.is_object() && saved.count(s.key)) apply_value(s, saved[s.key]);
}

void api_declare_setting(const char* plugin, const char* spec) {
  if (!plugin || !spec) return;
  json j = json::parse(spec, nullptr, false);
  if (!j.is_object() || !j.count("key")) { log("[pascalpatch] %s: bad setting %s", plugin, spec); return; }
  pp::Setting s = parse_setting(j);
  std::lock_guard<std::mutex> lock(pp::mutex());
  pp::Plugin* p = pp::find(plugin);
  if (!p) return;
  for (auto& old : p->settings) if (old.key == s.key) return;   // its plugin.json declared it already
  load_value(*p, s);
  p->settings.push_back(std::move(s));
}

const pp::Setting* find_setting(const char* plugin, const char* key) {
  pp::Plugin* p = plugin ? pp::find(plugin) : nullptr;
  if (!p || !key) return nullptr;
  for (auto& s : p->settings) if (s.key == key) return &s;
  return nullptr;
}
double api_setting_number(const char* plugin, const char* key) {
  std::lock_guard<std::mutex> lock(pp::mutex());
  const pp::Setting* s = find_setting(plugin, key);
  if (s && s->type == "key") return pp::key_vk(s->text);
  return s ? s->num : 0;
}
const char* api_setting_text(const char* plugin, const char* key) {
  static std::map<std::string, std::string> held;   // one live string per plugin
  std::lock_guard<std::mutex> lock(pp::mutex());
  const pp::Setting* s = find_setting(plugin, key);
  std::string& out = held[plugin ? plugin : ""];
  out = s ? s->text : "";
  return out.c_str();
}
void api_set_status(const char* plugin, const char* text) {
  std::lock_guard<std::mutex> lock(pp::mutex());
  if (pp::Plugin* p = plugin ? pp::find(plugin) : nullptr) p->status = text ? text : "";
}
void api_hud_text(float x, float y, uint32_t rgba, float size, const char* text) {
  if (text) pp::hud_building().push_back({pp::HudCmd::Text, x, y, 0, 0, size > 0 ? size : 16, rgba, true, text});
}
void api_hud_rect(float x0, float y0, float x1, float y1, uint32_t rgba, float rounding, int filled) {
  pp::hud_building().push_back({pp::HudCmd::Rect, x0, y0, x1, y1, rounding, rgba, filled != 0, {}});
}
void api_hud_circle(float x, float y, float r, uint32_t rgba, int filled) {
  pp::hud_building().push_back({pp::HudCmd::Circle, x, y, r, 0, 0, rgba, filled != 0, {}});
}
void api_hud_capsule(float x0, float y0, float r0, float x1, float y1, float r1, uint32_t rgba, int filled) {
  pp::HudCmd c{pp::HudCmd::Capsule, x0, y0, r0, x1, y1, rgba, filled != 0, {}};
  c.g = r1;
  pp::hud_building().push_back(std::move(c));
}
void api_hud_label(float x, float y, uint32_t rgba, float size, int align, const char* text) {
  if (!text) return;
  pp::HudCmd c{pp::HudCmd::Text, x, y, 0, 0, size > 0 ? size : 16, rgba, true, text};
  c.align = (uint8_t)std::clamp(align, 0, 2);
  c.outline = true;
  pp::hud_building().push_back(std::move(c));
}
void api_toast(const char* plugin, const char* text) {
  if (!text || !*text) return;
  std::lock_guard<std::mutex> lock(pp::mutex());
  auto& t = pp::toasts();
  std::string name = plugin ? plugin : "";
  if (pp::Plugin* p = plugin ? pp::find(plugin) : nullptr) name = p->name;
  t.push_back({name, text, GetTickCount64()});
  while (t.size() > 3) t.pop_front();
}
HWND port_window() {
  static HWND h = nullptr;
  if (!h || !IsWindow(h)) h = FindWindowW(L"MeleePortWindow", nullptr);
  return h;
}
int api_overlay_open() { return overlay::is_open() ? 1 : 0; }

// ---- HUD layout (0.5): panels stack away from their corner, in the order asked for ----
// the top-right stack starts under the key hint the port draws in that corner
constexpr float HUD_W = 640, HUD_MARGIN = 8, HUD_GAP = 6, HUD_TOP = 8, HUD_TOP_RIGHT = 46, HUD_BOTTOM = 300;
float g_corner[4];
void hud_place_reset() {
  g_corner[0] = HUD_TOP;
  g_corner[1] = HUD_TOP_RIGHT;
  g_corner[2] = g_corner[3] = HUD_BOTTOM;
}
void api_hud_place(int corner, float w, float h, float* x, float* y) {
  corner = std::clamp(corner, 0, 3);
  float px = corner % 2 == 0 ? HUD_MARGIN : HUD_W - HUD_MARGIN - w, py;
  if (corner < 2) { py = g_corner[corner]; g_corner[corner] += h + HUD_GAP; }
  else { g_corner[corner] -= h; py = g_corner[corner]; g_corner[corner] -= HUD_GAP; }
  if (x) *x = px;
  if (y) *y = py;
}

// ---- controller override (0.5) ----
// HSD_PadRenewMasterStatus turns this frame's raw controller reading (a PADStatus in the pad
// queue) into what the game reads. Standing in front of it, the runtime writes the held state
// into the queue first, so the game's own clamping, scaling and button edges handle it.
constexpr uint32_t PAD_RENEW_MASTER = 0x8037750Cu;   // HSD_PadRenewMasterStatus
constexpr uint32_t PAD_LIB = 0x804C1F78u;            // HSD_PadLibData
constexpr uint32_t PAD_STATUS_SIZE = 12;             // PADStatus
struct PadHold { bool on = false; pp_pad_state s{}; };
PadHold g_pad[4];
uint32_t g_pad_original = 0;
bool g_pad_hooked = false;

// A -1..1 stick back to the raw value the clamp turns into it (HSD_PadClampCheck3 and HSD_PadScale).
void raw_stick(float x, float y, int8_t& rx, int8_t& ry) {
  float scale = (float)(int8_t)api_rd8(PAD_LIB + 0x26), mn = (float)(int8_t)api_rd8(PAD_LIB + 0x1F);
  float mx = (float)(int8_t)api_rd8(PAD_LIB + 0x1E);
  bool shift = api_rd8(PAD_LIB + 0x1D) == 1;
  if (scale <= 0) scale = 80;
  float len = std::sqrt(x * x + y * y);
  if (len < 1e-4f) { rx = ry = 0; return; }
  float want = len * scale;                      // radius the game should end up with
  float raw = shift ? want + mn : want;          // before the shift towards the centre
  if (mx > 0) raw = std::min(raw, mx);
  float k = raw / len;
  rx = (int8_t)std::lround(std::clamp(x * k, -127.f, 127.f));
  ry = (int8_t)std::lround(std::clamp(y * k, -127.f, 127.f));
}
uint8_t raw_trigger(float v) {
  float scale = (float)api_rd8(PAD_LIB + 0x27), mn = (float)api_rd8(PAD_LIB + 0x22), mx = (float)api_rd8(PAD_LIB + 0x21);
  if (scale <= 0) scale = 140;
  if (v <= 0) return 0;
  float raw = v * scale + (api_rd8(PAD_LIB + 0x20) == 1 ? mn : 0);
  if (mx > 0) raw = std::min(raw, mx);
  return (uint8_t)std::clamp(std::lround(raw), 0l, 255l);
}

// ---- savestates (0.5) ----
// The same memory Slippi's rollback captures (Dolphin's SlippiSavestate): the game's static data,
// the file area and the main heap, minus sound, video and other hardware-facing blocks; plus the
// controller state, left out so a load does not replay presses. Taken and put back at the start
// of a game frame (in front of HSD_PadRenewMasterStatus), where the game is between frames.
struct Span { uint32_t start, end; };
struct StateSlot { bool full = false; uint32_t heap_lo = 0, heap_hi = 0; std::vector<Span> spans; std::vector<uint8_t> data; };
StateSlot g_states[PP_STATE_SLOTS];
int g_state_save = -1, g_state_load = -1;   // requests for the next frame

std::vector<Span> state_spans(uint32_t heap_lo, uint32_t heap_hi) {
  std::vector<Span> keep = {{0x80005520, 0x80005940}, {0x803B7240, 0x804DEC00}, {0x8065C000, 0x8071B000}, {heap_lo, heap_hi}};
  const Span skip[] = {
      {0x804031A0, 0x24}, {0x80407FB4, 0x34C}, {0x80433C64, 0x1EE80}, {0x804A8D78, 0x17A68}, {0x804C28E0, 0x399C},
      {0x804D7474, 0x8}, {0x804D74F0, 0x50}, {0x804D7548, 0x4}, {0x804D7558, 0x24}, {0x804D7580, 0xC},
      {0x804D759C, 0x4}, {0x804D7720, 0x4}, {0x804D7744, 0x4}, {0x804D774C, 0x8}, {0x804D7758, 0x8},
      {0x804D7788, 0x10}, {0x804D77C8, 0x4}, {0x804D77D0, 0x4}, {0x804D77E0, 0x4}, {0x804DE358, 0x80},
      {0x804DE800, 0x70}, {0x804D6030, 0x4}, {0x804D603C, 0x4}, {0x804D7218, 0x4}, {0x804D7228, 0x8},
      {0x804D7740, 0x4}, {0x804D7754, 0x4}, {0x804D77BC, 0x4}, {0x804DE7F0, 0x10}, {0x804C0980, 0x15F8},
      {PAD_LIB, 0x144},   // HSD_PadLibData and HSD_PadMasterStatus[4]
  };   // {address, length}
  for (const Span& x : skip) {
    uint32_t a = x.start, b = x.start + x.end;
    std::vector<Span> out;
    for (const Span& k : keep) {
      if (b <= k.start || a >= k.end) { out.push_back(k); continue; }
      if (a > k.start) out.push_back({k.start, a});
      if (b < k.end) out.push_back({b, k.end});
    }
    keep.swap(out);
  }
  return keep;
}

bool state_heap(uint32_t& lo, uint32_t& hi) {
  lo = api_rd32(0x804D76B8);   // the main heap's bounds, as the game set them up
  hi = api_rd32(0x804D76BC);
  return lo >= 0x80000000u && hi > lo && hi <= RAM_BASE + RAM_SIZE;
}

void state_step() {
  if (g_state_save >= 0) {
    StateSlot& s = g_states[g_state_save];
    g_state_save = -1;
    if (state_heap(s.heap_lo, s.heap_hi)) {
      s.spans = state_spans(s.heap_lo, s.heap_hi);
      size_t n = 0;
      for (const Span& x : s.spans) n += x.end - x.start;
      s.data.resize(n);
      n = 0;
      for (const Span& x : s.spans) { std::memcpy(s.data.data() + n, at(x.start, x.end - x.start), x.end - x.start); n += x.end - x.start; }
      s.full = true;
    }
  }
  if (g_state_load >= 0) {
    StateSlot& s = g_states[g_state_load];
    g_state_load = -1;
    uint32_t lo, hi;
    if (s.full && state_heap(lo, hi) && lo == s.heap_lo && hi == s.heap_hi) {
      size_t n = 0;
      for (const Span& x : s.spans) { std::memcpy(at(x.start, x.end - x.start), s.data.data() + n, x.end - x.start); n += x.end - x.start; }
    } else {
      log("[pascalpatch] state_load: the saved state does not fit the game as it is now");
    }
  }
}

void pad_renew_master(pp_cpu* cpu, void*) {
  state_step();
  if (api_rd8(PAD_LIB + 3)) {   // qcount: a reading is waiting
    uint32_t queue = api_rd32(PAD_LIB + 8);
    uint32_t read = api_rd8(PAD_LIB + 1);
    for (int port = 0; port < 4; ++port) {
      if (!g_pad[port].on || !queue) continue;
      const pp_pad_state& s = g_pad[port].s;
      uint32_t st = queue + (read * 4 + (uint32_t)port) * PAD_STATUS_SIZE;
      int8_t sx, sy, cx, cy;
      raw_stick(s.stick_x, s.stick_y, sx, sy);
      raw_stick(s.cstick_x, s.cstick_y, cx, cy);
      api_wr16(st + 0, (uint16_t)(s.buttons & 0xFFFF));
      api_wr8(st + 2, (uint8_t)sx);
      api_wr8(st + 3, (uint8_t)sy);
      api_wr8(st + 4, (uint8_t)cx);
      api_wr8(st + 5, (uint8_t)cy);
      api_wr8(st + 6, raw_trigger(s.trigger_l));
      api_wr8(st + 7, raw_trigger(s.trigger_r));
      api_wr8(st + 8, 0);    // analog A, B
      api_wr8(st + 9, 0);
      api_wr8(st + 10, 0);   // err: connected
    }
  }
  api_call(cpu, g_pad_original);
}

bool pad_hook() {
  if (!g_pad_hooked) {
    g_pad_hooked = true;
    g_pad_original = api_hook(PAD_RENEW_MASTER, pad_renew_master, nullptr);
    if (!g_pad_original) log("[pascalpatch] could not hook HSD_PadRenewMasterStatus");
  }
  return g_pad_original != 0;
}

int api_state_save(int slot) {
  if (slot < 0 || slot >= PP_STATE_SLOTS || !pad_hook()) return 0;
  g_state_save = slot;
  return 1;
}
int api_state_load(int slot) {
  if (slot < 0 || slot >= PP_STATE_SLOTS || !g_states[slot].full || !pad_hook()) return 0;
  g_state_load = slot;
  return 1;
}

void api_pad_set(int port, const pp_pad_state* s) {
  if (port < 0 || port > 3 || !s) return;
  if (!pad_hook()) return;
  g_pad[port].on = true;
  g_pad[port].s = *s;
}
void api_pad_release(int port) {
  if (port >= 0 && port <= 3) g_pad[port].on = false;
}
int api_key_down(int vk) {
  if (vk <= 0 || vk > 0xFE || overlay::is_open()) return 0;
  HWND fg = GetForegroundWindow();
  if (!fg || fg != port_window()) return 0;
  return (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0;
}

const pp_host g_host = {
  PP_PLUGIN_ABI, sizeof(pp_host), api_log,
  api_rd8, api_rd16, api_rd32, api_rdf32, api_wr8, api_wr16, api_wr32, api_wrf32,
  api_reg, api_set_reg, api_freg, api_set_freg,
  api_call, api_trampoline, api_hook, api_on_frame, api_guest_alloc,
  api_declare_setting, api_setting_number, api_setting_text, api_set_status,
  api_hud_text, api_hud_rect, api_hud_circle,
  api_toast, api_key_down, api_overlay_open, api_hud_label,
  api_hud_capsule,
  api_hud_place, api_pad_set, api_pad_release, api_state_save, api_state_load,
};

// A plugin's record. Its id is the DLL's name; a downloaded plugin's staged config carries its
// manifest (name, version, settings schema) under "_pascalpatch".
void register_plugin(const std::filesystem::path& dll, const std::filesystem::path& cfg_path) {
  pp::Plugin p;
  p.id = dll.stem().string();
  p.file = dll.filename().string();
  json cfg = read_json(cfg_path);
  g_configs[p.id] = cfg;
  const json meta = cfg.is_object() && cfg.count("_pascalpatch") ? cfg["_pascalpatch"] : json();
  if (meta.is_object()) {
    p.name = meta.value("name", p.id);
    p.version = meta.value("version", "");
    p.source = meta.value("source", "store");
    if (meta.count("settings_schema") && meta["settings_schema"].is_array())
      for (auto& spec : meta["settings_schema"]) {
        if (!spec.is_object() || !spec.count("key")) continue;
        pp::Setting s = parse_setting(spec);
        load_value(p, s);
        p.settings.push_back(std::move(s));
      }
  } else {   // staged by a profile: "unlock-all" -> "Unlock All"
    p.name = p.id;
    bool up = true;
    for (char& ch : p.name) {
      if (ch == '-' || ch == '_') { ch = ' '; up = true; }
      else if (up) { ch = (char)toupper((unsigned char)ch); up = false; }
    }
  }
  std::lock_guard<std::mutex> lock(pp::mutex());
  pp::plugins().push_back(std::move(p));
}

void load_plugins() {
  namespace fs = std::filesystem;
  std::wstring dir = env(L"PASCALPATCH_MODS");
  if (dir.empty()) dir = env(L"MELEEMOD_MODS");   // a launcher from before the rename
  if (dir.empty()) { log("[pascalpatch] no plugin folder"); return; }
  std::error_code ec;
  std::vector<fs::path> dlls;
  for (auto& e : fs::directory_iterator(dir, ec))
    if (e.path().extension() == L".dll") dlls.push_back(e.path());
  std::sort(dlls.begin(), dlls.end());
  for (auto& p : dlls) {
    fs::path cfg_path = p; cfg_path.replace_extension(".json");
    register_plugin(p, cfg_path);
    HMODULE h = LoadLibraryW(p.c_str());
    if (!h) { log("[pascalpatch] cannot load %s (error %lu)", p.string().c_str(), GetLastError()); continue; }
    auto load = reinterpret_cast<pp_plugin_load_fn>(GetProcAddress(h, PP_PLUGIN_LOAD_SYMBOL));
    if (!load)   // plugins built against the MeleeMod names: same table, older export name
      load = reinterpret_cast<pp_plugin_load_fn>(GetProcAddress(h, "mm_native_load"));
    if (!load) { log("[pascalpatch] %s has no %s", p.filename().string().c_str(), PP_PLUGIN_LOAD_SYMBOL); FreeLibrary(h); continue; }
    fs::path cfg = p; cfg.replace_extension(".json");
    std::string cfgs = cfg.string();
    int rc = load(&g_host, fs::exists(cfg, ec) ? cfgs.c_str() : nullptr);
    if (rc != 0) { log("[pascalpatch] %s refused to load (%d)", p.filename().string().c_str(), rc); FreeLibrary(h); continue; }
    g_modules.push_back(h);
    {
      std::lock_guard<std::mutex> lock(pp::mutex());
      if (pp::Plugin* rec = pp::find(p.stem().string())) rec->loaded = true;
    }
    log("[pascalpatch] loaded %s", p.filename().string().c_str());
  }
}

// Stands in front of the VI retrace handler. The first call happens on the simulation thread once
// the game is running (well before any fighter loads), which is where plugins are loaded.
void retrace(Context& c, uint8_t* m) {
  if (!g_ready.exchange(true)) {
    g_ram = m;
    if (std::memcmp(m, "GALE01", 6) != 0) log("[pascalpatch] warning: guest RAM does not start with GALE01");
    entry(SLOT_BASE) = sentinel;
    load_plugins();
  }
  pp::hud_building().clear();
  hud_place_reset();
  for (auto& f : g_frame) f.first(f.second);
  pp::hud_publish();
  g_frame_original(c, m);
}

DWORD WINAPI attach(void*) {
  HMODULE exe = GetModuleHandleW(nullptr);
  for (int i = 0; i < 120000; ++i) {   // the port builds the table right after start-up
    if (Fn* t = find_table(exe)) {
      g_table = t;
      g_frame_original = entry(FRAME_FN);
      entry(FRAME_FN) = retrace;
      log("[pascalpatch] attached: dispatch table found, frame hook installed");
      // The overlay hooks presentation, which needs the game's window and renderer up first.
      for (int w = 0; w < 1200 && !FindWindowW(L"MeleePortWindow", nullptr); ++w) Sleep(50);
      if (env(L"PASCALPATCH_NO_OVERLAY").empty()) overlay::install();
      return 0;
    }
    Sleep(1);
  }
  log("[pascalpatch] could not find the port's dispatch table; plugins are not loaded");
  return 1;
}

}  // namespace

// ---- state shared with the overlay (pp_runtime.h) ----
namespace pp {
std::mutex& mutex() { static std::mutex m; return m; }
std::vector<Plugin>& plugins() { static std::vector<Plugin> v; return v; }
Plugin* find(const std::string& id) {
  for (auto& p : plugins()) if (p.id == id) return &p;
  return nullptr;
}
std::deque<std::string>& console() { static std::deque<std::string> c; return c; }
void log(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog(fmt, ap); va_end(ap); }

std::string settings_dir() {
  std::wstring d = env(L"PASCALPATCH_SETTINGS");
  return d.empty() ? std::string() : std::filesystem::path(d).u8string();
}

void save_settings(const Plugin& p) {
  std::string dir = settings_dir();
  if (dir.empty()) return;
  auto path = std::filesystem::u8path(dir) / (p.id + ".json");
  json j = read_json(path);
  if (!j.is_object()) j = json::object();
  for (auto& s : p.settings) {
    if (s.type == "bool") j[s.key] = s.num != 0;
    else if (s.type == "int") j[s.key] = (long long)s.num;
    else if (s.type == "float") j[s.key] = s.num;
    else j[s.key] = s.text;
  }
  write_json(path, j);
}

// Downloaded plugins are switched on and off in <data>/plugins.json, PluginStore's state file,
// which sits beside the settings folder.
std::filesystem::path state_file() {
  std::string dir = settings_dir();
  return dir.empty() ? std::filesystem::path() : std::filesystem::u8path(dir).parent_path() / "plugins.json";
}
bool next_launch(const std::string& id, bool* enabled) {
  json j = read_json(state_file());
  if (!j.is_object() || !j.count("plugins") || !j["plugins"].is_object() || !j["plugins"].count(id)) return false;
  *enabled = j["plugins"][id].value("enabled", false);
  return true;
}
bool set_next_launch(const std::string& id, bool enabled) {
  auto path = state_file();
  json j = read_json(path);
  if (!j.is_object() || !j.count("plugins") || !j["plugins"].is_object() || !j["plugins"].count(id)) return false;
  j["plugins"][id]["enabled"] = enabled;
  write_json(path, j);
  return true;
}

std::vector<HudCmd>& hud_building() { static std::vector<HudCmd> v; return v; }
static std::vector<HudCmd>& hud_published() { static std::vector<HudCmd> v; return v; }
void hud_publish() { std::lock_guard<std::mutex> lock(mutex()); hud_published() = hud_building(); }
std::vector<HudCmd> hud_latest() { std::lock_guard<std::mutex> lock(mutex()); return hud_published(); }
std::deque<Toast>& toasts() { static std::deque<Toast> t; return t; }

// ---- key names, for "key" settings ----
namespace {
const std::pair<const char*, int> KEYS[] = {
  {"Backspace", VK_BACK}, {"Tab", VK_TAB}, {"Enter", VK_RETURN}, {"Shift", VK_SHIFT}, {"Ctrl", VK_CONTROL}, {"Alt", VK_MENU},
  {"Pause", VK_PAUSE}, {"CapsLock", VK_CAPITAL}, {"Space", VK_SPACE}, {"PageUp", VK_PRIOR}, {"PageDown", VK_NEXT},
  {"End", VK_END}, {"Home", VK_HOME}, {"Left", VK_LEFT}, {"Up", VK_UP}, {"Right", VK_RIGHT}, {"Down", VK_DOWN},
  {"Insert", VK_INSERT}, {"Delete", VK_DELETE}, {"NumpadMultiply", VK_MULTIPLY}, {"NumpadAdd", VK_ADD},
  {"NumpadSubtract", VK_SUBTRACT}, {"NumpadDecimal", VK_DECIMAL}, {"NumpadDivide", VK_DIVIDE},
  {"Semicolon", VK_OEM_1}, {"Equals", VK_OEM_PLUS}, {"Comma", VK_OEM_COMMA}, {"Minus", VK_OEM_MINUS},
  {"Period", VK_OEM_PERIOD}, {"Slash", VK_OEM_2}, {"Grave", VK_OEM_3}, {"LeftBracket", VK_OEM_4},
  {"Backslash", VK_OEM_5}, {"RightBracket", VK_OEM_6}, {"Quote", VK_OEM_7},
};
}  // namespace
int key_vk(const std::string& name) {
  if (name.empty()) return 0;
  if (name.size() == 1 && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9'))) return name[0];
  if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z') return name[0] - 'a' + 'A';
  if (name[0] == 'F' && name.size() <= 3) {
    int n = std::atoi(name.c_str() + 1);
    if (n >= 1 && n <= 24) return VK_F1 + n - 1;
  }
  if (name.rfind("Numpad", 0) == 0 && name.size() == 7 && name[6] >= '0' && name[6] <= '9') return VK_NUMPAD0 + (name[6] - '0');
  for (auto& k : KEYS) if (_stricmp(k.first, name.c_str()) == 0) return k.second;
  return 0;
}
std::string key_name(int vk) {
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, (char)vk);
  if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
  if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return "Numpad" + std::to_string(vk - VK_NUMPAD0);
  for (auto& k : KEYS) if (k.second == vk) return k.first;
  return std::string();
}
}  // namespace pp

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
  if (reason != DLL_PROCESS_ATTACH) return TRUE;
  DisableThreadLibraryCalls(self);
  std::wstring path = env(L"PASCALPATCH_LOG");
  if (path.empty()) path = env(L"MELEEMOD_LOG");
  if (!path.empty())
    g_log = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  // Runs while the port's main thread is still suspended, so no request can go out first.
  int n = guard_imports();
  log("[pascalpatch] offline guard: %d network import(s) refused", n);
  if (HANDLE h = CreateThread(nullptr, 0, attach, nullptr, 0, nullptr)) CloseHandle(h);
  return TRUE;
}
