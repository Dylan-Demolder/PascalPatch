# MeleeMod mod catalog

The launcher shows installed mods and the forward-looking catalog together.
A roadmap entry is visible for planning but cannot be enabled until it has a
validated manifest and implementation.

## Available

| ID | Name | Mode | Notes |
|---|---|---|---|
| `demo-mod` | Demo Mod | Online-safe | Static PPC plugin. Logs initialization and its first frame callback. |

## Prototype

| ID | Name | Mode | Scope |
|---|---|---|---|
| `input-display` | Input Display | Online-safe | Controller buttons, sticks and triggers overlay. |
| `training-tools` | Training Tools | Offline | Frame counter, input history, frame advance and reset spike. |

## Planned

| ID | Name | Mode | Scope |
|---|---|---|---|
| `frame-data-hud` | Frame Data HUD | Online-safe | Startup, active, recovery and hitstun display. |
| `widescreen-plus` | Widescreen Plus | Online-safe | Camera and HUD presentation changes. |
| `hd-texture-pack` | HD Texture Packs | Online-safe | Profile-local texture replacements and budgets. |
| `stage-music-manager` | Stage Music Manager | Online-safe | Stage music selection and shuffle. |
| `costume-packs` | Costume Packs | Online-safe | Validated costume and menu-art packages. |
| `character-packages` | Offline Character Packages | Offline | `.melee-character` installation from Character Studio. |
| `tournament-hud` | Tournament HUD | Online-safe | Minimal local tournament overlays. |
| `replay-analyzer` | Replay Analyzer | Online-safe | Replay frame, input and opening analysis. |
| `stage-expansion` | Stage Expansion Pack | Offline | Isolated offline stage and event content. |

## Product rules

- **Available** means the manifest is installed and the profile can build it.
- **Prototype** means code or research exists, but the launcher does not yet
  advertise it as installable.
- **Planned** means the idea is in the product catalog only.
- Gameplay-changing entries are offline-only until safety and runtime tests
  prove otherwise.
- Every installable entry must have a versioned manifest, compatibility data,
  dependency/conflict metadata and a deterministic build path.
