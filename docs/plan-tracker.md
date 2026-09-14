# Project plan tracker

This tracker separates implemented contracts from features that still need game/runtime evidence. A feature is not marked complete merely because a schema or stub exists.

## MeleeMod

| Task | Status | Evidence / next proof |
|---|---|---|
| Spike 001 reproducible build | VALIDATED on current Linux host | `spikes/001-reproducible-build/README.md`; rerun wrapper passes |
| Spike 002 safe code change | BOOT + STARTUP + FRAME/INPUT VALIDATED | Clean/static/runtime ISOs boot in standalone Dolphin; recomposed first-frame plugin emits initialization and repeated frame/input callback markers; bounded smoke remains required for each release profile |
| Spike 003 Dolphin/Slippi launch | BOOT VALIDATED | Standalone Dolphin 2606 reaches the user ISO memory-card prompt; Slippi Online/Playback remain unsuitable as original-title evidence |
| Spike 004 host/runtime bridge | DOLPHIN GDB MAILBOX HANDSHAKE VALIDATED / PRODUCTION ADAPTER PENDING | Host transport, PPC state machine, and a live Dolphin GDB mailbox HELLO/HELLO_ACK exchange are tested; general Dolphin/EXI transport and payload callbacks remain pending |
| Task 1 manifests | MVP COMPLETE | Versioned schemas and semantic validators |
| Task 2 profile loading | MVP COMPLETE | Dependency and safety resolution tests |
| Task 3 game/Dolphin discovery | MVP COMPLETE | ISO hash and emulator identity checks |
| Task 4 isolated atomic builds | MVP COMPLETE | Staging/current build test, including staged ISO recomposition |
| Task 5 asset mods | MVP COMPLETE | Exact target/conflict/traversal tests |
| Task 6 CLI | MVP COMPLETE | Validate/build/launch/log commands; launch smoke proof pending |
| Task 7 decomp build | MVP COMPLETE on known checkout | Pinned wrapper and expected hash |
| Task 8 runtime ABI | PPC BUILD + STARTUP/FRAME/EVENT LIFECYCLE VALIDATED | Runtime initialization, first-frame plugin call, frame subscription, input-history read, and `MM_EVENT_RUNTIME_READY` dispatch execute in standalone Dolphin; a host dispatcher benchmark and direct PPC input-read, 60-frame cadence, and opt-in full generated frame-hook timing probes are recorded in `docs/evidence/runtime-dispatch-overhead.md`; generated shutdown hook execution/idempotence is observed in a strict host fixture; Dolphin plugin shutdown hook is directly observed with the opt-in diagnostic; emulator process shutdown ordering and total in-game frame timing remain pending |
| Task 9 sample plugin | STARTUP + FRAME/EVENT LOAD VALIDATED | First-party hello/startup-probe/frame-probe plugins compile, link, initialize, subscribe, and log from recomposed in-game ISOs; frame/input callback evidence is recorded in `docs/evidence/frame-input-observation.md` |
| Task 10 plugin composition | STATIC MVP COMPLETE | Disposable worktree overlay, bundle generation, deferred first-frame lifecycle, and DOL link succeed |
| Task 11 bridge | DEVELOPMENT DOLPHIN GDB MAILBOX COMPLETE / PRODUCTION ADAPTER PENDING | Bounded host GDB adapter and PPC mailbox validate `MMB1`, checksum, version negotiation, heartbeat, and disconnect behavior; live HELLO/HELLO_ACK exchange is recorded in `docs/evidence/dolphin-gdb-bridge.md`; general production transport remains pending |
| Task 12 input history | FRAME SAMPLING + DISPATCH VALIDATED | Bounded ring receives frame samples from `HSD_PadCopyStatus[4]`; first-party frame probe read a current sample and matched its frame number in standalone Dolphin; values and interactive controller input are not claimed |
| Task 13 input display | FRAME CALLBACK OBSERVED / OVERLAY PENDING | First-party subscription/input-reader plugin emits a callback marker in standalone Dolphin; no controller activity or visual overlay is claimed; SisLib ownership research is recorded in `docs/input-overlay-research.md` |
| Task 14 training tools | FRAME HEARTBEAT OBSERVED / CONTROLS PENDING | Offline training-tools plugin compiles, links, and emits repeated `MM_EVENT_FRAME` heartbeat markers in standalone Dolphin; frame advance/reset/hit events remain pending and are not claimed; source hook candidates are documented in `docs/training-hooks-research.md` |
| Task 15 GUI | MVP + ERROR-DIALOG RECOVERY VALIDATED | Headless controller tests, bounded Tk display launch, and invalid-profile error dismissal are recorded in `docs/evidence/gui-recovery.md`; Tk self-tests validate the real Validate/Build/Launch callbacks, atomic output, process handling, and log creation with a fake discovery-approved executable; bounded real-Dolphin GUI launch and cleanup are observed; callback-failure recovery is also validated in self-test mode; interactive recovery/game progression remains pending |
| Task 16 safety | MVP COMPLETE | Capability-based fail-closed decisions; unrecognized future capability names now classify as `unknown` and are rejected in Slippi/tournament modes with regression tests; installed-Slippi-path pre-launch block is recorded in `docs/evidence/slippi-safety.md` |
| Task 17 diagnostics | Host report + symbolizer complete | Redacted report and bounded shell-free addr2line adapter; runtime crash capture pending |
| Task 18 registry | SIGNED INDEX + BOUNDED REMOTE FILE/ZIP INSTALL COMPLETE | Dependency-free Ed25519 index verification, trust metadata, validity/revocation checks, key rotation, HTTPS bounded fetch/cache update, streamed hash verification, and safe ZIP extraction with atomic install and pre-promotion Character Studio package validation are tested; remote folders without an archive representation remain separate |
| Task 19 CI | COMPLETE for host/core checks | GitHub Actions workflows run dependency-free tests; game smoke remains self-hosted only |
| Task 20 documentation/legal | MVP COMPLETE | README, architecture, legal notice, status |
| Task 21 release candidate | AUDIT COMPLETE / BLOCKED | Runtime/plugin output observations and release safety gates are documented; public-alpha release remains blocked by production bridge, visual/training, online smoke, conversion, playable round-trip, remote-folder, and interactive-recovery requirements |

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
| Melee composition | OFFLINE AUTHORING STAGING OBSERVED / GAME CONVERSION PENDING | Validated package is atomically staged outside game files with `game_integration: false`; source-backed HSD/`ftData` research is recorded in `docs/character-conversion-research.md`; asset/code conversion remains pending |
| Playable round trip | NOT STARTED | Requires Dolphin/game integration |

The next gating milestone is production bridge/payload validation and direct validation of the remaining GUI, conversion, and release requirements. Safety policy now fails closed for arbitrary future capabilities, but real Slippi smoke remains unverified. Development GDB mailbox handshake, plugin initialization, and frame/input callbacks are now directly observed before the memory-card prompt is dismissed.
