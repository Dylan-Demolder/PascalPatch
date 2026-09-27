// pascalpatch-launch: starts an unmodified melee_port.exe with the PascalPatch runtime injected.
//
//   pascalpatch-launch [--mods DIR] [--sandbox DIR] [--log FILE] [--settings DIR] [--runtime DLL] -- <port.exe> [port args...]
//
// The port is created suspended, pascalpatch_runtime.dll is loaded into it (LoadLibraryW on a remote
// thread), and only then is the port's main thread resumed, so the runtime's offline guard is in
// place before the port runs a single line. --sandbox points APPDATA/LOCALAPPDATA at a private
// folder so the port cannot pick up a Slippi Launcher login or any other per-user state.
// Exit code: the port's, or 90+ when the launch itself failed.
// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::wstring quote(const std::wstring& a) {
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

static int fail(int code, const wchar_t* what) {
  std::fwprintf(stderr, L"pascalpatch-launch: %ls (error %lu)\n", what, GetLastError());
  return code;
}

int wmain(int argc, wchar_t** argv) {
  std::wstring mods, sandbox, logfile, runtime, settings;
  int i = 1;
  for (; i < argc; ++i) {
    std::wstring a = argv[i];
    if (a == L"--") { ++i; break; }
    if (i + 1 >= argc) return fail(90, L"missing value");
    if (a == L"--mods") mods = argv[++i];
    else if (a == L"--sandbox") sandbox = argv[++i];
    else if (a == L"--log") logfile = argv[++i];
    else if (a == L"--runtime") runtime = argv[++i];
    else if (a == L"--settings") settings = argv[++i];
    else { std::fwprintf(stderr, L"usage: pascalpatch-launch [--mods DIR] [--sandbox DIR] [--log FILE] [--settings DIR] [--runtime DLL] -- <port.exe> [args]\n"); return 90; }
  }
  if (i >= argc) return fail(90, L"no port executable given");
  if (runtime.empty()) {
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    runtime = (fs::path(self).parent_path() / L"pascalpatch_runtime.dll").wstring();
  }
  runtime = fs::absolute(runtime).wstring();
  if (!fs::is_regular_file(runtime)) return fail(91, L"pascalpatch_runtime.dll not found");

  std::wstring cmd;
  for (int k = i; k < argc; ++k) cmd += (cmd.empty() ? L"" : L" ") + quote(argv[k]);

  // The port's environment: ours, with the runtime's settings and the sandboxed profile folders.
  std::map<std::wstring, std::wstring, bool (*)(const std::wstring&, const std::wstring&)> vars(
      [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
  if (wchar_t* block = GetEnvironmentStringsW()) {
    for (wchar_t* p = block; *p; p += std::wcslen(p) + 1) {
      std::wstring kv = p; size_t eq = kv.find(L'=', 1);
      if (eq != std::wstring::npos) vars[kv.substr(0, eq)] = kv.substr(eq + 1);
    }
    FreeEnvironmentStringsW(block);
  }
  if (!mods.empty()) vars[L"PASCALPATCH_MODS"] = fs::absolute(mods).wstring();
  if (!logfile.empty()) vars[L"PASCALPATCH_LOG"] = fs::absolute(logfile).wstring();
  if (!settings.empty()) vars[L"PASCALPATCH_SETTINGS"] = fs::absolute(settings).wstring();   // the F2 overlay saves here
  if (!sandbox.empty()) {
    fs::path root = fs::absolute(sandbox);
    std::error_code ec;
    fs::create_directories(root / L"Roaming", ec); fs::create_directories(root / L"Local", ec);
    vars[L"APPDATA"] = (root / L"Roaming").wstring();
    vars[L"LOCALAPPDATA"] = (root / L"Local").wstring();
  }
  std::wstring envblock;
  for (auto& [k, v] : vars) { envblock += k + L"=" + v; envblock.push_back(L'\0'); }
  envblock.push_back(L'\0');

  STARTUPINFOW si{sizeof si};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                      envblock.data(), nullptr, &si, &pi))
    return fail(92, L"cannot start the port");

  auto abort_launch = [&](int code, const wchar_t* what) {
    int rc = fail(code, what);
    TerminateProcess(pi.hProcess, (UINT)code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return rc;
  };
  size_t bytes = (runtime.size() + 1) * sizeof(wchar_t);
  void* remote = VirtualAllocEx(pi.hProcess, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remote || !WriteProcessMemory(pi.hProcess, remote, runtime.c_str(), bytes, nullptr)) return abort_launch(93, L"cannot write into the port");
  auto load = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
  HANDLE t = CreateRemoteThread(pi.hProcess, nullptr, 0, load, remote, 0, nullptr);
  if (!t) return abort_launch(94, L"cannot start the loader thread");
  WaitForSingleObject(t, 30000);
  DWORD loaded = 0; GetExitCodeThread(t, &loaded); CloseHandle(t);
  VirtualFreeEx(pi.hProcess, remote, 0, MEM_RELEASE);
  if (!loaded) return abort_launch(95, L"pascalpatch_runtime.dll did not load into the port");

  ResumeThread(pi.hThread);
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
  return (int)code;
}
