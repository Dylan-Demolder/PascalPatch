# Implementation status

## Completed and verified

| Plan area | State | Evidence |
|---|---|---|
| GALE01 1.02 identification | Complete | Real local ISO: header `GALE01`, revision byte `2`, expected `main.dol` SHA-1 |
| Reproducible decompilation build | Validated on current Linux host | `spikes/001-reproducible-build/README.md`; wrapper rerun passed in 33.62 s |
| Profile/mod schemas | MVP complete | `schemas/*.schema.json`, dependency-free semantic validator and tests |
| Profile isolation | MVP complete | `host/src/meleemod/store.py`, atomic promotion test |
| Static plugin profile builds | MVP complete | Disposable worktree overlay produces and stages a modified DOL; emulator execution pending |
| Filesystem asset composition | MVP complete | Exact targets, traversal rejection, conflict test |
| Dolphin discovery | MVP complete | Explicit executable validation; KDE `/usr/bin/dolphin` is rejected and installed Slippi AppImage is selected; launch helper records PID/exit/timeout |
| Runtime event ABI | PPC build complete | C tests plus Metrowerks PPC ABI/runtime link with hello-plugin; in-game lifecycle pending |
| Plugin dependency composition | Contract complete | Stable topological ordering, cycle detection and static manifest tests |
| Character package security | Validator + offline staging | Checksums, unsafe path/executable rejection, and validated-only offline workspace staging |

## Explicitly pending

- Native crash symbolization is implemented as a bounded, shell-free `addr2line` adapter.
- A real modified `main.dol` boot in standalone Dolphin 2606: clean, static-plugin, and recomposed runtime-plugin ISOs reach the Melee memory-card prompt. The initial memory-card prompt prevents first-frame plugin-output observation in the current automated input setup.
- Bounded emulator shutdown kills the complete Dolphin process group after timeout to avoid a modal Confirm Stop dialog or orphan child process.
- A bounded Slippi launch was attempted with the user ISO; it remained running until timeout, so menu/game boot is not yet confirmed.
- Linking the runtime into the Melee DOL.
- Generalizing static plugin source bundles into the full PPC runtime ABI and lifecycle.
- PPC plugin composition and runtime loading.
- Runtime endpoint and frame-time measurements (host transport is implemented and socket-pair tested).
- Real Dolphin/Slippi launch smoke tests and exit/log behavior.
- Full GUI recovery testing (headless controller and bounded Tk display launch pass when Tk libraries are supplied; base host lacks `libtk8.6`).
- Character model import, game asset conversion and playable character round trip (deterministic skeleton retargeting is now implemented).
- Validated character packages can now be staged into a non-game Offline workspace; no runtime asset conversion is claimed.
- Character Studio visual move/attribute UI and approved move library (source-preserving core editor is complete).
- First-party plugin visual/training behavior in Dolphin.

Do not label pending features as supported.

## Latest validation

- MeleeMod host tests: 26 passed.
- Character Studio tests: 13 passed.
- Strict C event/input tests and all SDK example syntax checks pass.
- Standalone Dolphin 2606 clean/modified smoke runner reports both processes started and hard-stops them without leftovers.
