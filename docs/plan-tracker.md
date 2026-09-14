# Project plan tracker

This tracker separates implemented contracts from features that still need game/runtime evidence. A feature is not marked complete merely because a schema or stub exists.

## MeleeMod

| Task | Status | Evidence / next proof |
|---|---|---|
| Spike 001 reproducible build | VALIDATED on current Linux host | `spikes/001-reproducible-build/README.md`; rerun wrapper passes |
| Spike 002 safe code change | BOOT VALIDATED / PLUGIN OBSERVATION PARTIAL | Clean, static-plugin, and recomposed runtime-plugin ISOs reach the same Melee memory-card prompt in standalone Dolphin 2606; plugin-specific output observation remains pending |
| Spike 003 Dolphin/Slippi launch | BOOT VALIDATED | Standalone Dolphin 2606 reaches the user ISO memory-card prompt; Slippi Online/Playback remain unsuitable as original-title evidence |
| Spike 004 host/runtime bridge | HOST PARTIAL | Checksummed protocol and bounded host transport are tested; runtime endpoint remains pending |
| Task 1 manifests | MVP COMPLETE | Versioned schemas and semantic validators |
| Task 2 profile loading | MVP COMPLETE | Dependency and safety resolution tests |
| Task 3 game/Dolphin discovery | MVP COMPLETE | ISO hash and emulator identity checks |
| Task 4 isolated atomic builds | MVP COMPLETE | Staging/current build test, including staged ISO recomposition |
| Task 5 asset mods | MVP COMPLETE | Exact target/conflict/traversal tests |
| Task 6 CLI | MVP COMPLETE | Validate/build/launch/log commands; launch smoke proof pending |
| Task 7 decomp build | MVP COMPLETE on known checkout | Pinned wrapper and expected hash |
| Task 8 runtime ABI | PPC BUILD COMPLETE | PPC-safe runtime/event ABI is compiled and linked into a DOL; in-game lifecycle proof pending |
| Task 9 sample plugin | PPC BUILD COMPLETE | Real hello-plugin compiles through SDK/runtime bundle and embeds its log string; in-game load pending |
| Task 10 plugin composition | STATIC MVP COMPLETE | Disposable worktree overlay, bundle generation, deferred first-frame lifecycle, and DOL link succeed |
| Task 11 bridge | Host transport complete | Versioned bounded checksum frames, stream adapter, private Unix listener, and cleanup tests; PPC runtime endpoint pending |
| Task 12 input history | Frame hook compiled | Bounded ring buffer and frame-event hook are compiled into the PPC bundle; controller sampling pending |
| Task 13 input display | PPC build complete | First-party subscription/input-reader plugin compiles and links; overlay and emulator proof pending |
| Task 14 training tools | PPC build complete | Offline frame-heartbeat plugin compiles and links; frame advance/reset/hit events pending |
| Task 15 GUI | MVP COMPLETE | Headless controller tests plus bounded Tk display launch; full interactive recovery test remains pending |
| Task 16 safety | MVP COMPLETE | Capability-based fail-closed decisions |
| Task 17 diagnostics | Host report + symbolizer complete | Redacted report and bounded shell-free addr2line adapter; runtime crash capture pending |
| Task 18 registry | Local MVP complete | Validated local file/folder install with deterministic hash checks; remote signing/update flow pending |
| Task 19 CI | COMPLETE for host/core checks | GitHub Actions workflows run dependency-free tests; game smoke remains self-hosted only |
| Task 20 documentation/legal | MVP COMPLETE | README, architecture, legal notice, status |
| Task 21 release candidate | NOT STARTED | Runtime/plugin output observation and character integration remain blocked |

## Character Studio

| Area | Status | Evidence / next proof |
|---|---|---|
| Versioned package contracts | MVP COMPLETE | Character/moveset/asset schemas |
| glTF/GLB import validation | MVP COMPLETE | Structural, URI and budget tests |
| Skeleton mapping | Transform retargeting complete | Deterministic name mapping plus rest-pose translation/rotation/scale retargeting; runtime calibration pending |
| Move editor/library | Core editor + Tk UI facade complete | Add/validate moves through CLI/controller; approved library pending |
| Attribute editor | Core editor + Tk UI facade complete | Numeric edits through CLI/controller; game-unit calibration pending |
| Preview/test scene | Authoring skeleton preview complete | Deterministic SVG preview is available; Melee renderer/runtime test scene remains pending |
| Deterministic package export | MVP COMPLETE | Repeated export hashes match |
| MeleeMod package validation | MVP COMPLETE | Cross-project package validation passes |
| Melee composition | Validated-only staging complete | Offline package can be staged outside game files; Melee asset/code conversion remains pending |
| Playable round trip | NOT STARTED | Requires Dolphin/game integration |

The next gating milestone is observing plugin initialization and frame/input callbacks after progressing past the initial memory-card prompt.
