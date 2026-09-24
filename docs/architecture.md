# Architecture

Native-first retarget: 2026-09-24. Primary runtime is
[melee-unlocked](https://github.com/hero88go/melee-unlocked) (GPL-2.0-or-later,
pinned revision); Dolphin is secondary. This document is a specification
change; no native row below is verified for MeleeMod yet.

```text
profile JSON + plugin/mod manifests
              │
              ▼
 host validator → discovery → staged composer → atomic build
              │                              │
              │                              └── extracted game directory or untouched ISO
              ▼
        build outputs (native-first)
          Tier A   user gecko INI + texture-pack folders    → beside melee_port.exe, no rebuild
          Tier B   profile ISO (data files composed)        → melee_port.exe --iso <profile.iso>
                 + profile sys-dir (GALE01r2.ini gecko list) → baked into the translated build
          Tier C   profile-modified DOL                     → pinned GPL fork of recomp.py (planned)
              │
              ▼
        launch: melee_port.exe (primary) · Dolphin adapter (secondary)

future: in-process host bridge (Tier C) replaces the pending GDB/EXI production transport
future: Character Studio exports validated .melee-character packages
```

The source ISO and source profile are never mutated. A build either promotes a
complete staged directory or leaves the previous `current` build in place.
Safety is derived from capabilities: unknown or gameplay-changing content is
not accepted in Slippi or tournament-safe profiles.

## What the native runtime provides (upstream facts)

Read from melee-unlocked source at the pinned revision; each still needs
direct verification against a MeleeMod profile build (plan-tracker N-series):

- **Recompiler**: `port/recomp/recomp.py` reads a DOL, bakes Slippi's Gecko
  tables from a `sys-dir` (`GameSettings/GALE01r2.ini`, `codehandler.bin`,
  `bootloader.gct`), translates hooks and C0 caves, and emits C++ built by
  CMake/VS2022. The input DOL must hash to the verified vanilla NTSC 1.02
  `main.dol` (`08e0bf20134dfcb260699671004527b2d6bb1a45`); any other input is
  refused. `--extra-gct`/`--gct-base` extend the bake to additional code
  lists, and run-time optional codes (widescreen, PAL stock icons, no screen
  shake) are emitted as both-variant instructions behind settings flags.
- **Runtime user codes**: a Dolphin-format `[Gecko]` INI is loaded at
  startup and every code is classified with a logged reason. Only data-write
  types (00/02/04/06) inside game memory are applied; code injection (C2/C3),
  pointer-register writes, and any write into the translated text sections are
  rejected. Enabled state persists in `port-settings.ini`.
- **Texture packs**: Dolphin-compatible `tex1_*.png` packs (XXH64 naming)
  under `Load/Textures/GALE01` or `TexturePacks/` beside the executable;
  per-pack enable/disable is persisted, and a dump mode writes the exact
  names the build looks for. DDS files and material-map companions are
  ignored.
- **File layers**: game data files are read from the user's ISO (the DVD HLE
  serves reads straight from the disc); Slippi's own menu/HUD files are
  served as vcdiff `.diff`/`.dat` overrides through the EXI device
  (`port/slippi_sys/GameFiles/`). Arbitrary asset replacement therefore goes
  into the composed ISO, not a loose override folder.
- **Saves**: `.gci` memory-card files in Dolphin GCI-folder format.
- **Evidence tooling**: `validate_native.py` (2400 checkpoints across four
  render modes), `online_pair.py` (two instances, full online game, `DESYNC`
  scan), `replay_compare.py` (frame-exact replay diff), input-automation
  scripts in `port/scripts/`, and the host log `melee_port.log`.

## Tiers

| Tier | Profile produces | Mechanism | Depends on |
|---|---|---|---|
| A — no rebuild | user Gecko INI + texture-pack folders | installed beside the executable; the port applies and classifies them itself | nothing beyond a pinned upstream revision |
| B — recompile (primary) | recomposed ISO with the DOL untouched + profile `sys-dir` | recompiler bakes the profile Gecko table into the translated build; assets are served from the ISO at run time | pinned upstream revision |
| C — GPL fork (planned) | staged modified DOL from static C plugins | recompiler fork that accepts a modified input under base-hash + delta validation; optional in-process host bridge | GPL-2.0-or-later-compatible distribution terms |

Whether Tier A content is *safe* (online-safe vs offline) is never answered by
the port's classifier — that remains MeleeMod's capability classification
(`docs/compatibility.md`). The port's classifier only answers "can this code
physically run in a translated build".

## Evidence-path mapping

| Dolphin-era gate | Native replacement |
|---|---|
| DTM movie input automation | input-automation scripts (`port/scripts/*.txt`) |
| GDB stub reads (`verify_fighters.py`) | guest `OSReport` markers in `melee_port.log` (verify via OS HLE) plus the port's own classification/counter log lines |
| memory-card priming (`prime_memcard.py`) | a seeded `.gci` folder dropped in beside the game |
| bounded Dolphin smoke | `validate_native.py` checkpoints, `online_pair.py`, `replay_compare.py` |
| production PPC↔host bridge (GDB mailbox / EXI, pending) | in-process host bridge inside the port runtime (Tier C) |

## GPL posture

- **Tiers A and B** invoke upstream tools as external programs: no upstream
  code is vendored, so no license obligation attaches to this repository while
  it stays private and undistributed.
- **Tier C** is explicit GPL-compatible reuse — forking, vendoring, or linking
  the recompiler/runtime. Any distributed artifact that includes
  melee-unlocked code (the recompiler fork, a runtime-linked bridge, or a
  packaged native build) must ship under GPL-2.0-or-later-compatible terms
  with source and notices. MeleeMod's schemas and validators are original
  code; that does not change the combined artifact's terms.
- Distributing Nintendo data remains prohibited regardless of license.

## Pinning

Pin the port revision the way the decomp build is pinned: the exact revision
is recorded and upgrades are explicit. The vanilla-DOL invariant and the
Gecko bake are the contracts this project depends on; a pin break is a
plan-tracker event, not silent drift.

## Not claimed

- No profile has been built, booted, or observed on the native runtime.
- No production bridge exists in any form beyond the development GDB mailbox.
- No catalog entry is installable natively.
- The upstream facts above were read from source at a revision not yet
  re-verified locally.
