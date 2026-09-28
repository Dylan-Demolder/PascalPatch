# Publishing and contributing

There are three ways to share a plugin:

1. **Send a ZIP.** Anyone can install it with **Plugins > Install from file**. The app marks it as installed from a file, which means unverified. This is fine for friends and testers.
2. **List it on the plugin site.** It then appears on the app's **Browse** page and at <https://dylan-demolder.github.io/pascalpatch-plugins/> for every PascalPatch player, with its README, settings and changelog. Installs are verified against a signed index, and players get updates. This page covers this route.
3. **Improve PascalPatch itself.** Fixes and additions to the SDK (`melee.h` fields, helpers, docs) are pull requests to [PascalPatch](https://github.com/Dylan-Demolder/PascalPatch).

## How the plugin site works

You send **source**, never a DLL. The maintainer builds every listed plugin from the reviewed source, signs the site's index with a key that never leaves their machine, and publishes the package. So:

- players can trust that a plugin on Browse is exactly the reviewed source;
- anyone can read, learn from and improve any listed plugin;
- you do not need a certificate or a signing key.

The plugin site lives in [Dylan-Demolder/pascalpatch-plugins](https://github.com/Dylan-Demolder/pascalpatch-plugins). Community plugins go in its `community/` folder:

```
community/
  ledge-trainer/
    plugin.json
    README.md
    LICENSE            optional: a copy of the GPL
    native/
      CMakeLists.txt
      ledge_trainer.cpp
      ...
```

## Before you open a pull request

Run the same check the site's CI runs:

```bash
python <PascalPatch>/tooling/check_plugin.py path/to/ledge-trainer --community --build
```

It checks the following:

- **plugin.json is valid.** `author` is set and `license` is `GPL-2.0-or-later`.
- **The folder is named after the plugin's id.**
- **The folder has README.md and native/CMakeLists.txt.** The build must work from that folder and the SDK alone, so vendor any header-only library into `native/` together with its licence.
- **Only source is in the folder.** No DLLs, executables, archives or game files, and no file over 2 MB.
- **There is no networking code.** Plugins are offline-only.
- **It builds, and the DLL packs:** 64-bit, named `<id>.dll`, exporting `pp_plugin_load`.

It also prints "for the reviewer" notes for calls that are allowed but get a closer look: starting processes, loading other DLLs, changing memory protection, deleting files. If your plugin needs one of these, explain why in the pull request.

## Opening the pull request

1. Fork [pascalpatch-plugins](https://github.com/Dylan-Demolder/pascalpatch-plugins).
2. Add your folder under `community/<id>/`.
3. Open a pull request titled `<Name> <version>`, for example `Ledge Trainer 1.0.0`. In the description, say:
   - what it does and who it is for;
   - how you tested it: which characters, stages and modes;
   - whether it writes game memory, holds a controller, uses savestates or hooks game functions, and why.
   Include a screenshot or a short clip of it in game.
4. The **Community plugins** check builds and packs your plugin on Windows. Its ZIP is attached to the run as an artifact, so reviewers and testers can install it with Install from file.
5. A reviewer reads the source, tries it in game, and may ask for changes. When it is merged, the maintainer builds, signs and publishes it, and it appears on Browse.

## The review checklist

Reviewers go through this list, so check your plugin against it first:

- [ ] **Does one clear job.** The summary and README say what the player gets.
- [ ] **Offline.** No networking, no telemetry, no update checks.
- [ ] **No game data.** Nothing from the disc in the source. Everything the plugin needs from the game is read from memory at run time.
- [ ] **Safe by default.** Anything that writes game memory, drives a controller or loads a state is behind a setting or a hotkey, and the README says so.
- [ ] **Checks the runtime.** `PP_HOST_HAS` for every call newer than 0.1, and `min_runtime` set to match.
- [ ] **Checks the match.** `pp_in_match` before reading fighters. It works in VS, Training and Quick Match, and leaves the menus alone.
- [ ] **Cheap per frame.** No file I/O, allocation storms or long loops in `on_frame`.
- [ ] **Shares the screen and the keys.** Panels go through `hud_place`, hotkeys are `key` settings, and the defaults do not clash with [the keys in use](input.md#keys-already-in-use).
- [ ] **Addresses are named.** Every game address is a constant with a comment naming the decomp symbol.
- [ ] **Settings are complete.** Each has a label, sensible limits and `help` where needed. Settings are grouped when there are more than about six.
- [ ] **The README covers** what it shows, the hotkeys, the settings worth knowing, and what it changes in the game.
- [ ] **GPL-2.0-or-later**, with an SPDX line at the top of each source file:
  `// SPDX-License-Identifier: GPL-2.0-or-later`

## Updating your plugin

1. Change the source.
2. Bump `version`:
   - **patch** (1.0.1) for fixes;
   - **minor** (1.1.0) for new features or settings;
   - **major** (2.0.0) when settings change meaning or are removed.
3. Replace `changes` with a sentence or two on what is new. The site shows it as the version's changelog.
4. Open a pull request titled `<Name> <new version>`.

A published version is never rebuilt with different bytes. Every change ships as a new version, and players' apps offer the update.

Keep setting keys stable across versions, because players' saved values are keyed by them. When a setting's meaning changes, give it a new key.

## Rules for listed plugins

- **Offline only.** PascalPatch is not for netplay, and plugins must never give an advantage in online play.
- **No game data and no piracy.** Players bring their own game.
- **GPL-2.0-or-later**, source in the plugin site repository.
- **No harm.** Nothing that damages saves, hides what it does, collects data, or touches files outside its own settings and the files its README describes.
- **Respect other people's work.** Credit ideas and research (the decomp, UnclePunch's Training Mode, community frame data) in your README.

The maintainer may remove a plugin that breaks these rules, and may update a listed plugin for a new PascalPatch runtime if its author is unavailable.

## For the maintainer

Publishing a merged community plugin, on the machine that holds the signing key:

```bash
python tooling/native/build_plugins.py --data <data> --plugins-root <pascalpatch-plugins>/community --only <id>
```

```bash
python tooling/site/publish.py --out <pascalpatch-plugins> --seed <seed> --dlls <data>/native-plugins --plugins-root <pascalpatch-plugins>/community --plugin <id>
```

Then commit the site repository and run the `gh release create` command that publish.py prints. See [plugin-site.md](../plugin-site.md).
