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
| Character package security | Validator complete | Checksums and unsafe path/executable rejection |

## Explicitly pending

- A real modified `main.dol` boot in Dolphin (Spike 002 is build-validated but emulator execution remains PARTIAL; clean and modified runs do not yet expose a menu state).
- A bounded Slippi launch was attempted with the user ISO; it remained running until timeout, so menu/game boot is not yet confirmed.
- Linking the runtime into the Melee DOL.
- Generalizing static plugin source bundles into the full PPC runtime ABI and lifecycle.
- PPC plugin composition and runtime loading.
- Host/runtime transport and frame-time measurements.
- Real Dolphin/Slippi launch smoke tests and exit/log behavior.
- GUI.
- Character model import, skeleton retargeting, game asset conversion and playable character round trip.
- Character Studio move/attribute UI and approved move library.

Do not label pending features as supported.
