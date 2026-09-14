# Release-candidate audit

This audit records the evidence boundary for the current repositories. It is intentionally not a release approval. A row is marked complete only where code and a bounded test or direct observation exist.

## MeleeMod

| Area | Current result | Evidence / remaining gate |
|---|---|---|
| Game identity and source protection | Complete | GALE01 Rev.02 DOL hash validation; source ISO and original decomp checkout remain clean |
| Reproducible decomp build | Complete on the pinned checkout | Spike 001 wrapper and clean build verification |
| Static PPC composition | Complete | Disposable worktree, generated bundle, strict PPC link, atomic staging and ISO recomposition |
| Runtime initialization | Directly observed | Standalone Dolphin OSREPORT logs hello initialization and runtime-ready subscription/dispatch |
| Frame and input history | Directly observed | `docs/evidence/frame-input-observation.md`; frame probe callback matched current event and history frame numbers |
| Host bridge | Complete for local host transport | Checksummed bounded stream and private Unix transport tests |
| PPC bridge | Development Dolphin GDB mailbox validated; production adapter pending | `runtime/src/bridge.c`, bounded host GDB adapter, and `docs/evidence/dolphin-gdb-bridge.md`; live HELLO/HELLO_ACK exchange is proven, but general production transport and payload callbacks remain pending |
| Launcher and bounded shutdown | Complete | Process-group hard-kill tests and smoke tooling; interactive emulator prompt recovery is not claimed |
| Online/tournament safety | Fail-closed host policy complete; real Slippi smoke absent | Capability-derived policy tests now reject arbitrary unrecognized capability names as unknown; actual online login/tournament environment remains unverified |
| Input display and training tools | Infrastructure only | Plugins compile and receive frame events; no visual overlay, frame advance, reset, or hit-event proof |
| GUI | Headless controller and bounded Tk launch complete | Full interactive recovery remains unverified on the base host |
| Diagnostics | Host reporting/symbolization complete | Runtime crash capture remains absent |
| Registry | Signed indexes, local install, and bounded remote file/ZIP install complete | Dependency-free Ed25519 signatures, trust metadata, revocation/expiry checks, key rotation, HTTPS bounded fetch/cache update, streamed hash verification, and safe ZIP extraction with atomic promotion are tested; remote folders and package-specific semantic validation remain separate |

## Character Studio

| Area | Current result | Remaining gate |
|---|---|---|
| Package contracts and secure validation | Complete | Versioned schemas, checksums, archive/path/symlink/executable rejection tests |
| glTF/GLB authoring and retargeting | Complete for authoring scope | Runtime calibration and Melee conversion are not implemented |
| Moves and attributes | Contract/editor complete | Approved content and game-unit calibration require verified source values |
| Preview | Deterministic SVG skeleton preview complete | Full Melee renderer/test scene absent |
| Offline package staging | Complete | Writes validated-only staging metadata and never modifies game files |
| Melee asset conversion | Not implemented | Fighter/archive/animation conversion and hooks need research and implementation |
| Character-select/playable round trip | Not started | Requires conversion, runtime integration, and user-owned test assets |

## Release decision

The project is not complete and must not be marked release-ready. The hard blockers are production bridge transport/payload callbacks, visual/training runtime features, real Slippi safety smoke, remote folder/package-semantic validation, Melee asset conversion, playable round-trip testing, and full GUI recovery. `goal.complete()` must not be called until these rows are either implemented with direct evidence or explicitly removed from the project scope by the user.
