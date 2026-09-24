# MeleeMod mod catalog

The launcher shows installed mods and the forward-looking catalog together.
A roadmap entry is visible for planning but cannot be enabled until it has a
validated manifest and implementation.

Native-first tiers (retarget 2026-09-24, see `architecture.md`): **A** runs
beside `melee_port.exe` with no rebuild (user Gecko file, texture packs);
**B** needs a profile build (recomposed ISO + baked profile Gecko table);
**C** is blocked on the planned GPL recompiler fork (profile-modified DOL).
The Tier column states the intended native delivery mechanism — nothing is
verified on the native runtime yet.

## Available

| ID | Name | Mode | Tier | Notes |
|---|---|---|---|---|
| `demo-mod` | Demo Mod | Online-safe | C (Dolphin today) | Static PPC plugin. Logs initialization and its first frame callback. Native reach requires the Tier C fork. |

## Prototype

| ID | Name | Mode | Tier | Scope |
|---|---|---|---|---|
| `input-display` | Input Display | Online-safe | B/C | Controller buttons, sticks and triggers overlay. Re-evaluate on native: baked Gecko (B) versus a port-side overlay (C). |
| `training-tools` | Training Tools | Offline | B/C | Frame counter, input history, frame advance and reset spike; frame advance/reset are game code, so they bake at recompile (B). |

## Planned

| ID | Name | Mode | Tier | Scope |
|---|---|---|---|---|
| `frame-data-hud` | Frame Data HUD | Online-safe | B | Startup, active, recovery and hitstun display via the baked profile table. |
| `widescreen-plus` | Widescreen Plus | Online-safe | A/B | Upstream already ships optional widescreen, PAL stock icons and no-screen-shake as run-time flags; this entry shrinks to verifying them plus any extra presentation codes. |
| `hd-texture-pack` | HD Texture Packs | Online-safe | A | Upstream texture-pack support exists (Dolphin-compatible `tex1_*.png`); this entry is manifest, validation, install and budgeting of packs. |
| `stage-music-manager` | Stage Music Manager | Online-safe | A/B | Upstream jukebox plays custom HPS music with its own Music slider; this entry is selection, shuffle and validation around it. |
| `costume-packs` | Costume Packs | Online-safe | A (texture) / B (archive) | Texture-level replacements install as packs; archive-level replacement goes into the composed ISO. |
| `character-packages` | Offline Character Packages | Offline | B | `.melee-character` installation from Character Studio: composed fighter files in the profile ISO plus a baked table/hook Gecko list. |
| `tournament-hud` | Tournament HUD | Online-safe | B | Minimal local tournament overlays via the baked profile table. |
| `replay-analyzer` | Replay Analyzer | Online-safe | host tool | Runtime-independent; the port writes `.slp` files to `Replays\`. |
| `stage-expansion` | Stage Expansion Pack | Offline | B/C | File replacement covers assets (B); new stage/game logic likely needs DOL-level code (C). |

## Product rules

- **Available** means the manifest is installed and the profile can build it.
- **Prototype** means code or research exists, but the launcher does not yet
  advertise it as installable.
- **Planned** means the idea is in the product catalog only.
- Gameplay-changing entries are offline-only until safety and runtime tests
  prove otherwise.
- Every installable entry must have a versioned manifest, compatibility data,
  dependency/conflict metadata and a deterministic build path.
- Tier A entries install as files and are proven by the runtime's own log
  counters (gecko classification lines, texture matched/lookups report).
- Tier B entries are proven only by a profile build booted under the native
  evidence path (`plan-tracker.md` N-series).
- Tier C entries cannot be advertised as installable before the recompiler
  fork exists.
- Upstream mechanisms (texture packs, run-time optional codes, jukebox) reduce
  MeleeMod's job to manifest, validation, install, and safety classification —
  they do not discharge those duties.
