# Project plan tracker

This tracker separates implemented contracts from features that still need game/runtime evidence. A feature is not marked complete merely because a schema or stub exists.

## MeleeMod

| Task | Status | Evidence / next proof |
|---|---|---|
| Spike 001 reproducible build | VALIDATED on current Linux host | `spikes/001-reproducible-build/README.md`; rerun wrapper passes |
| Spike 002 safe code change | BOOT + STARTUP + FRAME/INPUT VALIDATED | Clean/static/runtime ISOs boot in standalone Dolphin; recomposed first-frame plugin emits initialization and repeated frame/input callback markers; bounded smoke remains required for each release profile |
| Spike 003 Dolphin/Slippi launch | BOOT VALIDATED | Standalone Dolphin 2606 reaches the user ISO memory-card prompt; Slippi Online/Playback remain unsuitable as original-title evidence |
| Spike 004 host/runtime bridge | HOST COMPLETE / PPC ENDPOINT PENDING | Checksummed protocol and bounded host transport are tested; no in-game endpoint is claimed until a supported PPC transport is implemented |
| Task 1 manifests | MVP COMPLETE | Versioned schemas and semantic validators |
| Task 2 profile loading | MVP COMPLETE | Dependency and safety resolution tests |
| Task 3 game/Dolphin discovery | MVP COMPLETE | ISO hash and emulator identity checks |
| Task 4 isolated atomic builds | MVP COMPLETE | Staging/current build test, including staged ISO recomposition |
| Task 5 asset mods | MVP COMPLETE | Exact target/conflict/traversal tests |
| Task 6 CLI | MVP COMPLETE | Validate/build/launch/log commands; launch smoke proof pending |
| Task 7 decomp build | MVP COMPLETE on known checkout | Pinned wrapper and expected hash |
| Task 8 runtime ABI | PPC BUILD + STARTUP/FRAME/EVENT LIFECYCLE VALIDATED | Runtime initialization, first-frame plugin call, frame subscription, input-history read, and `MM_EVENT_RUNTIME_READY` dispatch execute in standalone Dolphin; shutdown lifecycle remains pending |
| Task 9 sample plugin | STARTUP + FRAME/EVENT LOAD VALIDATED | First-party hello/startup-probe/frame-probe plugins compile, link, initialize, subscribe, and log from recomposed in-game ISOs; frame/input callback evidence is recorded in `docs/evidence/frame-input-observation.md` |
| Task 10 plugin composition | STATIC MVP COMPLETE | Disposable worktree overlay, bundle generation, deferred first-frame lifecycle, and DOL link succeed |
| Task 11 bridge | Host transport complete | Versioned bounded checksum frames, stream adapter, private Unix listener, and cleanup tests; PPC runtime endpoint pending |
| Task 12 input history | FRAME SAMPLING + DISPATCH VALIDATED | Bounded ring receives frame samples from `HSD_PadCopyStatus[4]`; first-party frame probe read a current sample and matched its frame number in standalone Dolphin; values and interactive controller input are not claimed |
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
| Move editor/library | Contract + editor complete | Versioned provenance-aware approved-library validator plus add/validate editor; library content and game calibration pending |
| Attribute editor | Contract + editor complete | Numeric editor plus provenance-aware unit/range calibration validator; game-unit values remain user-supplied |
| Preview/test scene | Authoring skeleton preview complete | Deterministic SVG preview is available; Melee renderer/runtime test scene remains pending |
| Deterministic package export | MVP COMPLETE | Repeated export hashes match |
| MeleeMod package validation | MVP COMPLETE | Cross-project package validation passes |
| Melee composition | Validated-only staging complete | Offline package can be staged outside game files; Melee asset/code conversion remains pending |
| Playable round trip | NOT STARTED | Requires Dolphin/game integration |

The next gating milestone is a PPC-to-host bridge decision and direct validation of the remaining safety, GUI, conversion, and release requirements. Plugin initialization and frame/input callbacks are now directly observed before the memory-card prompt is dismissed.
