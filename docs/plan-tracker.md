# Project plan tracker

This tracker separates implemented contracts from features that still need game/runtime evidence. A feature is not marked complete merely because a schema or stub exists.

## MeleeMod

| Task | Status | Evidence / next proof |
|---|---|---|
| Spike 001 reproducible build | VALIDATED on current Linux host | `spikes/001-reproducible-build/README.md`; rerun wrapper passes |
| Spike 002 safe code change | BUILD VALIDATED / EMULATOR PARTIAL | Disposable static bundle builds and marker is present; emulator menu proof remains pending |
| Spike 003 Dolphin/Slippi launch | PARTIAL | Executable behavior discovery works; menu boot not confirmed |
| Spike 004 host/runtime bridge | DEFERRED | No transport selected until runtime is booted |
| Task 1 manifests | MVP COMPLETE | Versioned schemas and semantic validators |
| Task 2 profile loading | MVP COMPLETE | Dependency and safety resolution tests |
| Task 3 game/Dolphin discovery | MVP COMPLETE | ISO hash and emulator identity checks |
| Task 4 isolated atomic builds | MVP COMPLETE | Staging/current build test |
| Task 5 asset mods | MVP COMPLETE | Exact target/conflict/traversal tests |
| Task 6 CLI | MVP COMPLETE | Validate/build/launch/log commands; launch smoke proof pending |
| Task 7 decomp build | MVP COMPLETE on known checkout | Pinned wrapper and expected hash |
| Task 8 runtime ABI | PPC BUILD COMPLETE | PPC-safe runtime/event ABI is compiled and linked into a DOL; in-game lifecycle proof pending |
| Task 9 sample plugin | PPC BUILD COMPLETE | Real hello-plugin compiles through SDK/runtime bundle and embeds its log string; in-game load pending |
| Task 10 plugin composition | STATIC MVP COMPLETE | Disposable worktree overlay, bundle generation and DOL link succeed; generic runtime hooks pending |
| Task 11 bridge | Protocol contract complete | Versioned bounded checksum frames; transport/runtime integration pending |
| Task 12 input history | Frame hook compiled | Bounded ring buffer and frame-event hook are compiled into the PPC bundle; controller sampling pending |
| Task 13 input display | PPC build complete | First-party subscription/input-reader plugin compiles and links; overlay and emulator proof pending |
| Task 14 training tools | PPC build complete | Offline frame-heartbeat plugin compiles and links; frame advance/reset/hit events pending |
| Task 15 GUI | NOT STARTED | CLI/core must remain source of truth |
| Task 16 safety | MVP COMPLETE | Capability-based fail-closed decisions |
| Task 17 diagnostics | Host report complete | Redacted profile/plugin/runtime report; native crash symbolization pending |
| Task 18 registry | Local MVP complete | Validated local file/folder install with deterministic hash checks; remote signing/update flow pending |
| Task 19 CI | COMPLETE for host/core checks | GitHub Actions workflows run dependency-free tests; game smoke remains self-hosted only |
| Task 20 documentation/legal | MVP COMPLETE | README, architecture, legal notice, status |
| Task 21 release candidate | NOT STARTED | Blocked by emulator and plugin/character integration |

## Character Studio

| Area | Status | Evidence / next proof |
|---|---|---|
| Versioned package contracts | MVP COMPLETE | Character/moveset/asset schemas |
| glTF/GLB import validation | MVP COMPLETE | Structural, URI and budget tests |
| Skeleton mapping | BASIC COMPLETE | Deterministic name mapping; transform retargeting pending |
| Move editor/library | Core validator complete | Duplicate binding/reference diagnostics; UI and approved library pending |
| Attribute editor | Core validator complete | Range/inheritance diagnostics; UI and game-unit calibration pending |
| Preview/test scene | NOT STARTED | Requires renderer and runtime data |
| Deterministic package export | MVP COMPLETE | Repeated export hashes match |
| MeleeMod package validation | MVP COMPLETE | Cross-project package validation passes |
| Melee composition | NOT STARTED | Requires runtime asset/code conversion |
| Playable round trip | NOT STARTED | Requires Dolphin/game integration |

The next gating milestone is a known-good emulator boot of both the clean and modified DOL.
