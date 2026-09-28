# Getting started

By the end of this page you will have built a plugin, installed it, and seen it in a match. It draws a small panel with port 1's current move and animation frame. From there you change it into your own idea.

## What you need

- **Windows 10 or 11, 64-bit.**
- **Visual Studio 2022 Build Tools** with the "Desktop development with C++" workload. The Community edition works too.
- **CMake 3.20 or newer.** Visual Studio ships one, or get it from cmake.org.
- **Python 3.10 or newer**, for the packing tool.
- **Git.**
- **PascalPatch set up and able to play**, with your own NTSC 1.02 game backup. See [user-setup.md](../user-setup.md).

Plugins are plain C or C++. You do not need to know PowerPC, the decomp, or anything about the port's internals to start.

## 1. Get the SDK

```bash
git clone https://github.com/Dylan-Demolder/PascalPatch
```

The SDK is two things in that repository:

- `sdk/include/pascalpatch/`: the headers. [`plugin.h`](../../sdk/include/pascalpatch/plugin.h) is the host API; [`melee.h`](../../sdk/include/pascalpatch/melee.h) names the game's data.
- `sdk/template/`: a working plugin to start from.

## 2. Copy the template

Put your plugin in its own folder, anywhere. Use your own git repository if you like:

```
my-plugin/
  plugin.json        name, version, settings (what the app and the F2 window show)
  README.md          what it does and how to use it (the plugin site shows it)
  native/
    CMakeLists.txt
    my_plugin.cpp
```

```bash
cp -r PascalPatch/sdk/template my-plugin
```

## 3. Build it

From inside `my-plugin`:

```bash
cmake -S native -B build -A x64 -DPASCALPATCH_SDK=C:/path/to/PascalPatch/sdk/include
```

```bash
cmake --build build --config Release
```

That produces `build/Release/my-plugin.dll`. Inside the PascalPatch repository (`sdk/template` itself) the `-DPASCALPATCH_SDK` part can be left out.

The template links the C runtime statically, so the DLL needs nothing installed beside it. Keep that setting when you add files.

## 4. Pack and install it

```bash
python C:/path/to/PascalPatch/tooling/pack_plugin.py . build/Release/my-plugin.dll --install
```

`pack_plugin.py` checks the same things the app does before anything reaches the game:

- `plugin.json` is valid;
- the DLL is 64-bit, is named `<id>.dll` and exports `pp_plugin_load`;
- there is a README.

It then writes `my-plugin-0.1.0.zip`. `--install` installs that ZIP straight into the PascalPatch app's data folder. Without `--install`, open the app, go to **Plugins > Install from file** and pick the ZIP. Both routes do the same thing.

The Plugins page now lists My Plugin, marked as installed from a file.

## 5. See it in game

Press **Play** in the app. Start any match (VS, Training, or Quick Match; see [testing](testing.md)). The panel appears in the top-left corner and follows port 1's moves frame by frame.

Press **F2** to open the overlay. My Plugin has its own tab with:

- its status line;
- its settings (the show / hide key and the opacity), which save between runs;
- its log.

Press **Numpad0** during the match to hide the panel. A toast confirms it.

## 6. Make it yours

1. Pick an id: lowercase letters, digits, `.`, `_` and `-`, for example `ledge-trainer`. It must be the same in four places:
   - `"id"` in `plugin.json`;
   - `"entry"` in `plugin.json`, which is `<id>.dll`;
   - the `add_library(...)` target in `CMakeLists.txt`;
   - `ID` in the source, which the plugin passes to the host's settings and logging calls.
2. Change `name`, `summary`, `author`, `tags` and the README.
3. Replace `frame()` with your idea. The [cookbook](cookbook.md) has the common pieces.
4. Rebuild, then run `pack_plugin.py ... --install` again. Reinstalling the same version replaces it. Close and relaunch the game to load the new DLL.

## How the template works

```cpp
extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char* config_path) {
  if (!host || host->abi != PP_PLUGIN_ABI) return 1;
  if (!PP_HOST_HAS(host, hud_place)) { host->log(ID, "needs PascalPatch 0.5 or newer"); return 1; }
  H = host;
  H->declare_setting(ID, R"({"key":"toggle_key","type":"key","label":"Show / hide","default":"Numpad0"})");
  ...
  H->on_frame(frame, nullptr);
  return 0;
}
```

- `pp_plugin_load` is the only function PascalPatch calls directly. It runs once, before the game starts. It checks the host, declares settings, registers callbacks, and returns 0. Anything else unloads the plugin.
- `PP_HOST_HAS(host, hud_place)` checks that the running PascalPatch is new enough for the newest call the plugin uses. Match it with `"min_runtime"` in plugin.json. See [the host API](host-api.md).
- `declare_setting` repeats plugin.json's settings, so the DLL also works when it is loaded on its own (see [testing](testing.md)).
- `on_frame(frame)` runs `frame` once per game frame, 60 times a second, on the game's own thread.

`frame()` then follows the pattern almost every plugin uses:

```cpp
if (!g_visible || !pp_in_match(H)) return;   // fighters only exist during a match
uint32_t fp = pp_fighter(H, 0);              // port 1's fighter, or 0
if (!fp) return;
uint32_t kind = H->rd32(fp + PP_FT_KIND), state = H->rd32(fp + PP_FT_STATE);
```

The steps are: check that a match is on, get a fighter, read its fields, then draw. [Reading the game](game-data.md) explains each part, and [Drawing on screen](hud.md) covers the panel.

## Next

- [Designing a useful plugin](design.md), before you settle on an idea.
- [Testing and debugging](testing.md), for a fast edit, build and try loop.
- [Publishing and contributing](publishing.md), when it is ready for others.
