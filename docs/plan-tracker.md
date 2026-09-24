# Project plan tracker

This tracker separates implemented contracts from features that still need game/runtime evidence. A feature is not marked complete merely because a schema or stub exists.

## Native-first tasks (added 2026-09-24)

Retarget: melee-unlocked (native Windows port, GPL-2.0-or-later, pinned
revision) is the primary runtime. Tiers are defined in `architecture.md`.
All rows start NOT STARTED; the pivot itself is a spec change and proves
nothing. Nothing above or below is reclassified by these rows existing.

| Task | Status | Evidence / next proof |
|---|---|---|
| N1 pin + native build spike | OBSERVED (2026-09-24, Slippi-free) | melee-unlocked `4d1aa84` built on this Windows host with VS2022 Build Tools from the user's ISO (DOL SHA-1 `08e0bf20…` accepted). Translated with `recomp.py --no-slippi`, run with a local `--no-slippi` runtime flag (no Slippi device, login lookup or network). Boots to the vanilla main menu, VS. Mode CSS and an offline match (`tooling/native/offline_vs_fox.txt`). Needs `melee/` linked to the pinned `sourceport/extern/melee` submodule; NVIDIA Streamline DLLs are absent (DLSS off) |
| N2 native log evidence | NOT STARTED | Confirm guest `OSReport` markers and the port's gecko/texture log lines land in `melee_port.log`; this is the replacement for GDB-stub reads |
| N3 Tier A packaging | NOT STARTED | A profile installs a user-gecko INI + texture pack beside `melee_port.exe`; port logs show code classification and texture matched/lookups counters |
| N4a Tier B data overlay | OBSERVED (2026-09-24) | `characters` profile entries replace an existing fighter file in the profile ISO with the DOL untouched (`meleemod build`); `meleemod launch --runtime native` starts `melee_port.exe --iso <profile.iso>`. The overlay ISO keeps the source disc size: a tightly packed image failed a sector-rounded read of the last file (`FATAL: disc read failed`) |
| N4 Tier B bake | NOT STARTED | Profile `sys-dir` GCT + recomposed ISO; the recomp bake summary prints the profile codes and one baked data code is observed in game |
| N5 Tier C recompiler fork | NOT STARTED (requires GPL-compatible terms, agreed in `architecture.md`) | Fork accepts a profile-modified DOL under base-hash + delta validation; `demo-mod` observed natively |
| N6 in-process host bridge | NOT STARTED (blocked on N5) | Replaces Task 11's production transport: a live host↔runtime control/observe exchange from the host CLI |
| N7 online smoke | NOT STARTED | `online_pair.py` completes a full game with a `slippi`-mode profile; log shows no `DESYNC` |
| N8 verification harness | NOT STARTED | `validate_native.py` 2400 checkpoints run against clean vs profile build; wired into release evidence |

Dolphin-era rows (spikes 001–004, Task 3 discovery, Task 6 launch, smoke and
roster verification) keep their recorded Dolphin evidence; N1–N8 are their
native replacements and are additive.

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
| Task 9 sample plugin | STARTUP + FRAME/EVENT LOAD VALIDATED | First-party hello/startup-probe/frame-probe plugins compile, link, initialize, subscribe, and log from recomposed in-game ISOs; frame/input callback evidence is recorded in `docs/evidence/frame-input-observation.md`; catalog-loaded `demo-mod` loader/game evidence is recorded in `docs/evidence/demo-mod.md` |
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
| Playable round trip | IMPORTED MODEL + RETUNED MOVES OBSERVED (native, offline) | 2026-09-24: Character Studio `examples/leesin` (static CC-BY glTF, auto-rigged onto Captain Falcon; moves retuned in `PlCa.dat` hitboxes) installed by a profile `characters` entry with `fighter_file` + `costume_file`. The Falcon slot renders as Lee Sin with Falcon's animations; scripted down-B deals 18% and KOs from 0% where vanilla deals 12%. Earlier: | Character Studio `ember` example composed into Fox's slot (`compose-fighter-slot`, attribute table only), built and launched by MeleeMod on the native port: CSS selection and an offline match start, and the fighter visibly differs from vanilla Fox (1.55 model scale; the same jump input carries it far higher). Evidence is local frame captures only (game-derived, not committed). A new roster slot, new model/animations and log-marker gates remain pending. Native-first: profile ISO + baked Gecko on the pinned port (N-series), with PAS-28 gate 5 restated in MeleeCharacterStudio's `docs/authoring.md`; Dolphin path retained as secondary cross-check |

The next gating milestone is production bridge/payload validation and direct validation of the remaining GUI, conversion, and release requirements. Safety policy now fails closed for arbitrary future capabilities, but real Slippi smoke remains unverified. Development GDB mailbox handshake, plugin initialization, and frame/input callbacks are now directly observed before the memory-card prompt is dismissed.
