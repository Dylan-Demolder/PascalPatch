# Building PascalPatch plugins

PascalPatch is a community project. Anyone can write a plugin, install it into their own game in a minute, and share it through the plugin site so every PascalPatch player can install it from the app's **Browse** page.

A plugin is a small Windows DLL that runs inside the game, the way BakkesMod plugins run inside Rocket League. The game itself is never changed: PascalPatch starts an unmodified copy of the game and loads your DLL next to it, then hands the DLL a table of functions. Through that table your plugin can read and write the game's memory, call the game's own functions, draw on the screen, read hotkeys, hold a controller, take savestates, and add settings to the in-game **F2** window.

What people have built with it so far: frame data readouts, a hitbox viewer, an L-cancel and tech trainer, a wavedash trainer, a DI trainer with predicted launch lines, a combo counter, match statistics, and a training lab with savestates, a pausable and slowable game, and dummies that replay recorded inputs. Every one of them lives in [`plugins/`](../../plugins) as complete source you can read and copy.

## Start here

1. **[Getting started](getting-started.md)**: build the template, install it, see it in game. About 15 minutes.
2. **[Designing a useful plugin](design.md)**: what makes players keep a plugin on. Read this before you pick an idea.

## Reference

| Page | What it covers |
|---|---|
| [plugin.json](plugin-json.md) | the manifest: id, version, settings and every setting type |
| [The host API](host-api.md) | every function in `pp_host`, and the PascalPatch version that added it |
| [Reading the game](game-data.md) | `melee.h`: players, fighters, action states, scenes, and the traps |
| [Drawing on screen](hud.md) | text, panels, capsules, sharing the screen, projecting world positions |
| [Hotkeys and controllers](input.md) | key settings, reading the pads, `pad_set` for dummies and playback |
| [Savestates](savestates.md) | `state_save` / `state_load`, and keeping them safe |
| [Hooks and game functions](hooks.md) | `hook`, `call`, `trampoline`, `guest_alloc`, and finding addresses in the decomp |
| [Testing and debugging](testing.md) | logs, the F2 window, Quick Match, input scripts |
| [Cookbook](cookbook.md) | short recipes for the things plugins do most |

## Sharing your plugin

**[Publishing and contributing](publishing.md)**: send the source to the plugin site in a pull request. The maintainer builds it, signs it and lists it. The page also has the review checklist and the rules every listed plugin follows.

## The ground rules

- **Offline only.** The runtime refuses every network request, and PascalPatch never runs on Slippi netplay. Plugins are for training, practice, local play and fun.
- **No game data.** Never commit or ship anything taken from the game disc (files, models, textures, sounds). Read what you need from memory at run time instead.
- **Open source, GPL-2.0-or-later.** PascalPatch builds on the doldecomp/melee research and is GPL. Plugins on the site ship their source under the same licence, so others can learn from and improve them.
- **Say what you change.** If a plugin writes game memory, it says so in its README and only does it while an option is on.

## Versions

Game addresses are for NTSC 1.02, the only version the port runs. The plugin ABI is 1. New host functions arrive in PascalPatch releases (0.2, 0.3, 0.4, 0.5). Each one is marked in [the host API](host-api.md), and `PP_HOST_HAS` tells your plugin at run time whether it is there.
