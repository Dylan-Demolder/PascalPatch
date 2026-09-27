# Plugin authoring

A PascalPatch plugin is a Windows DLL that runs inside the game, the way BakkesMod plugins run inside Rocket League. The game is never modified: `pascalpatch-launch.exe` starts an unmodified `melee_port.exe` and injects `pascalpatch_runtime.dll`, which loads your DLL and hands it the host table from [`sdk/include/pascalpatch/plugin.h`](../sdk/include/pascalpatch/plugin.h). Plugins are offline-only.

## The pieces

```
plugins/<id>/
  plugin.json        what the store and the F2 overlay show, and the settings
  README.md          shown on the plugin site
  native/            the DLL's source and CMakeLists.txt
```

`plugin.json`:

```json
{
  "id": "input-display", "name": "Input Display", "version": "1.0.0", "abi": 1,
  "entry": "input-display.dll",
  "summary": "Shows a controller's sticks, buttons and triggers on screen.",
  "author": "you", "license": "GPL-2.0-or-later", "tags": ["hud", "training"],
  "settings": [
    {"key": "scale", "type": "float", "label": "Size", "default": 1.0, "min": 0.5, "max": 2.0}
  ]
}
```

Setting types: `bool`, `int`, `float` (`min`/`max`), `choice` (`options`: values or `{value, label}`), `text`. A plugin can also declare settings itself with `declare_setting`, which is how a plugin loaded straight from a profile gets a settings tab.

## The DLL

Export `pp_plugin_load(const pp_host *host, const char *config_path)` and return 0. Everything else goes through `host`:

- guest memory (`rd*`/`wr*`), guest calls (`call`), hooks on indirect calls (`hook`, `trampoline`);
- `on_frame` to run once per game frame (VI retrace);
- since 0.2, check `PP_HOST_HAS(host, field)` first:
  - `setting_number` / `setting_text`: read a setting; cheap enough for every frame;
  - `set_status`: one line at the top of the plugin's F2 tab;
  - `hud_text` / `hud_rect` / `hud_circle`: draw from an `on_frame` callback in the game's 640 × 480 space; colours are `0xRRGGBBAA`.

`plugins/input-display/native` is a complete example: a controller HUD with four settings.

Callbacks run on the game's simulation thread, inside the frame: never block, never sleep, never do file or network I/O per frame.

## Try it

Put the DLL and its `.json` in a folder and launch through the sandbox (never Slippi):

```sh
pascalpatch-launch.exe --mods <plugin folder> --sandbox <private folder> -- melee_port.exe <port args>
```

The app's Play button does the same for a profile. In game, press **F2**: the overlay lists every plugin, and your plugin's tab shows its settings, status line and log. Changes apply on the next frame and are saved for the next launch. The overlay needs the port's D3D12 renderer (the default); on D3D11 plugins still run but there is no overlay.

## Publish it

See [plugin-site.md](plugin-site.md): the plugin site builds a reproducible, signed package from `plugins/<id>` and your built DLL.

## Static plugins (legacy Dolphin path)

The older static ABI (`plugin_init`/`plugin_shutdown`, composed as PPC source into a disposable decompilation worktree and linked into a DOL) is still validated by the host for Dolphin profiles. It is not dynamic loading. See [static-plugins.md](static-plugins.md).
