# PascalPatch

PascalPatch is a separate host-side project for building isolated Super Smash Bros. Melee profiles. It never distributes Nintendo game data. Users provide their own GALE01 Rev.02 / Melee 1.02 ISO or extracted game directory.

## Start here

PascalPatch is a mod loader for [melee-unlocked](https://github.com/hero88go/melee-unlocked), the native Windows port of Melee 1.02. It works like BakkesMod: the game is never modified. `pascalpatch-launch.exe` starts the port and injects `pascalpatch_runtime.dll`, which loads plugins into the running game. Everything is offline: never Slippi, never netplay.

**Play:** download `PascalPatch-<version>-windows.zip` from the [latest release](https://github.com/Dylan-Demolder/PascalPatch/releases/latest), unzip it, and double-click `pascalpatch.cmd`. It includes Python and the built runtime: no compiler, no installs.

**New here?** [The PascalPatch guide](docs/user-guide.md) covers setup, playing, every plugin and hotkey, practice recipes, Character Studio and troubleshooting.

**Build your own plugin:** PascalPatch is community developed. Start with [docs/plugins](docs/plugins/README.md): a 15-minute tutorial from the SDK template to your plugin in game, the full API reference, and how to get it listed on the plugin site.

| Part | What it does | Where |
|---|---|---|
| **The app** | Pick a profile and Play; turn plugins on and off; browse and install plugins from the plugin site; settings; activity and logs | `pascalpatch app` (below) |
| **F2 overlay** | In game: the plugin list, each plugin's settings tab (saved between runs), a console | `runtime/native/overlay.cpp` (D3D12 renderer) |
| **Plugins** | DLLs with a `plugin.json`; settings, status line and HUD drawing through the host table | [docs/plugins](docs/plugins/README.md), `sdk/include/pascalpatch/plugin.h`, `sdk/template/` |
| **Plugin site** | Signed index, packages on GitHub Releases, a website on GitHub Pages | [docs/plugin-site.md](docs/plugin-site.md), `tooling/site/publish.py`, `website/` |
| **Character Studio** | Make fighters from your own 3D models: projects, save/autosave, rosters | the MeleeCharacterStudio repo; the app's Character Studio button opens it |
| **Pascal UI** | One look for the app, the overlay, the site and the studio | `design/` |

```sh
PYTHONPATH=host/src python -m pascalpatch.cli --root C:/Users/you/MeleeMods app
```

In game, press **F2** to open the overlay and **Esc** to close it. `PASCALPATCH_NO_OVERLAY=1` turns the overlay off.

The rest of this README covers the host's profile builds and the older Dolphin evidence path.

## Runtime targets (native-first, retarget 2026-09-24)

The primary runtime is now [melee-unlocked](https://github.com/hero88go/melee-unlocked) (GPL-2.0-or-later): a native Windows port that statically recompiles the vanilla NTSC 1.02 `main.dol` together with Slippi's Gecko tables into C++, runs it on D3D12/D3D11, and includes Slippi netplay. Dolphin remains available as a secondary runtime for cross-checks; it is no longer the evidence path of record. This is a specification change only — no PascalPatch feature has been verified on the native runtime, and no pending gate below is promoted by it.

Profile builds feed the native runtime in three tiers (full design in `docs/architecture.md`):

| Tier | What the profile produces | How it runs | Status |
|---|---|---|---|
| A — runtime, no rebuild | user Gecko file (Dolphin `[Gecko]` INI) + texture pack folders (`tex1_*.png`) | dropped beside `melee_port.exe`; the port classifies each code at load and logs why unsupported codes cannot run | upstream mechanisms exist; PascalPatch packaging not started |
| B — recompile (primary) | profile ISO (data files composed, DOL untouched) + profile `sys-dir` Gecko list | `recomp.py` bakes the Gecko table into the translated build; `melee_port.exe --iso <profile.iso>` | not started |
| C — GPL fork (planned) | profile-modified DOL from static C plugins | pinned fork of the recompiler that accepts a modified DOL under base-hash + delta validation, plus an in-process host bridge | not started; requires GPL-2.0-or-later-compatible distribution terms |

The recompiler refuses any input DOL that is not the verified vanilla one (SHA-1 enforced), so Tier B is the primary target: every code change rides in the baked Gecko table (hooks, C0 caves, data writes) and every asset change rides in the recomposed ISO. PascalPatch's static source-to-DOL composition (below) becomes a Tier C input.

## Verified today

- GALE01 Rev.02 identification and embedded `main.dol` SHA-1 validation.
- Pinned `doldecomp/melee` build wrapper.
- Isolated profile builds with staging and atomic `current` promotion.
- Exact relative-path filesystem mods with fail-closed conflict detection.
- Vanilla ISO build outputs use a symlink to the original file; the ISO is not copied or changed.
- Strict profile/plugin/mod manifest validation.
- Capability-based safety classification.
- Initial C runtime event ABI, host-compiled tests, SDK sample manifest, dependency-ordered static plugin manifests, and disposable static source-to-DOL composition.
- Character-package path safety and checksum validation.

All of the above is host-side or Dolphin-observed evidence. None of it has been exercised against the native runtime.

## Native evidence path (planned)

Nothing in this section is verified for PascalPatch. These are the upstream mechanisms each Dolphin-era evidence path will be replaced by, to be exercised against profile outputs in the native-first tasks (`docs/plan-tracker.md`, N-series):

| Dolphin-era evidence | Native replacement |
|---|---|
| `.dtm` movie input automation | the port's input-automation scripts (`port/scripts/*.txt`, e.g. `to_css.txt`, `vs_match.txt`, `online_bot.txt`) |
| Dolphin GDB stub fighter reads (`verify_fighters.py`) | guest `OSReport` markers in `melee_port.log` via the port's OS HLE (verify in the first spike) and the port's own log lines (gecko classification, texture-pack counters) |
| `prime_memcard.py` first-boot dialog priming | plain `.gci` memory-card folder (Dolphin GCI format) dropped in beside the game |
| clean/modified `dolphin_smoke.py` comparisons | `tools/validate_native.py` (2400 simulation checkpoints across headless/hidden/threaded/authored modes), `tools/online_pair.py`, `tools/replay_compare.py` |
| GDB mailbox / EXI production bridge (Task 11, pending) | in-process host bridge in the port runtime (Tier C, planned) |

## Run

```sh
PYTHONPATH=host/src python tooling/inspect_disc.py /path/to/GALE01.iso
PYTHONPATH=host/src python -m pascalpatch.cli --root . profile list
PYTHONPATH=host/src python -m pascalpatch.cli --root . profile validate PROFILE_ID
PYTHONPATH=host/src python -m pascalpatch.cli --root . build PROFILE_ID
PYTHONPATH=host/src python -m pascalpatch.cli --root . launch PROFILE_ID --dry-run
PYTHONPATH=host/src python -m pascalpatch.cli --root . app
```

`profile list` and other commands expect `profiles/`, `plugins/`, and `mods/` catalogs under `--root`. Relative base-game and mod paths are resolved from their manifest locations.

Extract a user-owned ISO for filesystem composition with the pinned decompilation toolkit:

```sh
python tooling/extract_disc.py /path/to/GALE01.iso /path/to/extracted-game \
  --dtk /path/to/dtk
```

PPC static profiles require an extracted game directory and explicit decompilation/runtime/source paths. Plugin entrypoints are initialized on the first game-loop frame; in-game behavior still requires runtime observation. The optional Tk GUI is a thin view over the same validated core APIs.

## Launcher dashboard and mod catalog

The desktop app (`pascalpatch app`) replaced the Tk launcher: profiles, the
installed and roadmap catalog, plugins and the plugin site, build and Play on
the native port. `tooling/pascalpatch_gui.py` now opens the app. The Tk
launcher is still there for Dolphin profiles:

```sh
PYTHONPATH=host/src python tooling/pascalpatch_gui.py --root . --tk   --data ~/.local/share/pascalpatch --dolphin /usr/bin/dolphin-emu
```

See `docs/mod-catalog.md` for the catalog, tiers, and support rules.

## Tests

```sh
PYTHONPATH=host/src python -m unittest discover -s host/tests -v
gcc -std=c99 -Wall -Wextra -Werror -Iruntime/include \
  runtime/src/events.c runtime/src/runtime.c runtime/tests/test_events.c \
  -o /tmp/pascalpatch-test && /tmp/pascalpatch-test
```

## Legal boundary

This repository contains tooling, schemas and original sample code only. Do not commit, distribute or fetch Nintendo ISO, DOL, extracted assets or copyrighted game data.

Tier C plans GPL-compatible reuse of melee-unlocked (GPL-2.0-or-later):
forking, vendoring, or linking its recompiler/runtime. This repository is
private and undistributed today; any distributed build that includes
melee-unlocked code must ship under GPL-2.0-or-later-compatible terms and
retain its notices. The posture is recorded in `docs/architecture.md`.


## Static code profiles

A code profile must explicitly provide `decomp_repo`, `decomp_orig` and `plugin_source_root`. Plugins using the SDK context ABI also require `runtime_root`. Each selected plugin must declare a relative `.c` `source` and a C entrypoint. The host creates a disposable worktree and stages the generated DOL. Static code profiles can also recompose a staged ISO with `tooling/recompose_disc.py`; the source ISO is never modified. Filesystem asset mods still require an extracted game directory.

The staged DOL is a Tier C input for the native runtime: upstream `recomp.py`
refuses any input whose SHA-1 is not the verified vanilla DOL, so static code
profiles run in Dolphin today and reach the native runtime only through the
planned recompiler fork. Filesystem asset mods reach the native runtime with
no fork at all (Tier B: they are composed into the profile ISO).


## Secondary: bounded Dolphin smoke (legacy evidence path)

Retained for cross-checking. The primary evidence path is the native table
above and is not yet implemented for PascalPatch.

For clean/modified comparisons, use the user-data-only runner:

```sh
PYTHONPATH=host/src python tooling/dolphin_smoke.py \
  --dolphin /path/to/dolphin-emu \
  --clean "/path/to/Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso" \
  --modified /path/to/generated.iso \
  --movie /path/to/menu-to-match.dtm \
  --expected-input-automation-ready INPUT_AUTOMATION_READY \
  --timeout 30
```

The runner kills the complete emulator process group on timeout. It does not leave a Confirm Stop dialog or orphan child process.


## Secondary: roster / character verification in Dolphin (legacy evidence path)

Before any of this: a fresh Dolphin user-dir hits an un-clearable
"Create Game Data?" memory-card dialog on first boot (it does not respond to
DTM movie input). Prime a memory card once per host:

```sh
DISPLAY=:0 PYTHONPATH=host/src python tooling/prime_memcard.py \
  --dolphin /usr/bin/dolphin-emu --iso "/path/to/GALE01.iso" \
  --output ~/.cache/pascalpatch/memcard-seed
```

Boot-level smoke above only proves Dolphin didn't crash. To confirm specific
characters — retail and newly-injected custom ones — actually exist as live
`Fighter` instances in the same running match/demo, attach over Dolphin's GDB
stub and read the real fighter list:

```sh
dolphin-emu -b --batch -e /path/to/modified.iso \
  -C Dolphin.General.GDBPort=24689 -C Dolphin.Display.RenderToMain=False &
PYTHONPATH=host/src python tooling/verify_fighters.py \
  --port 24689 --duration 180 --expect-kind 1 --expect-kind 20
```

See `docs/dolphin-control.md` for the full guide: Dolphin control rules
(SIGKILL only, xcb/movie-playback pitfalls, container `QT_QPA_PLATFORM`
requirements), driving character-select deterministically with `.dtm` movies,
and what evidence actually counts as a working character per
`docs/character-conversion-research.md`'s PAS-28 gates. On the native-first
path those gates are restated against the native runtime in
`docs/plan-tracker.md` and MeleeCharacterStudio's `docs/authoring.md`.


## Verified demo mod

The repository includes a harmless `demo-mod` catalog entry at `plugins/demo-mod/plugin.json`. Create a local runnable profile with `python tooling/create_demo_profile.py /path/to/game.iso /path/to/MeleeDecomp/melee`. It is a visual-only static PPC plugin that logs initialization and its first frame callback. Use a temporary profile with your own GALE01 Rev.02 input, then run `profile validate` and `build`; do not commit the ISO or generated game output. Direct loader/Dolphin evidence is in `docs/evidence/demo-mod.md`.

The demo mod is a static source-to-DOL composition, so it is Dolphin-observed
today and Tier C on the native runtime until the recompiler fork exists.

## License

PascalPatch is free software: you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation, either version 2 of
the License, or (at your option) any later version. See [LICENSE](LICENSE). This matches
[melee-unlocked](https://github.com/hero88go/melee-unlocked), the port PascalPatch runs.

The repository holds no Nintendo game data: bring your own NTSC 1.02 disc image.
