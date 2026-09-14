# Implementation status

## Completed and verified

| Plan area | State | Evidence |
|---|---|---|
| GALE01 1.02 identification | Complete | Real local ISO: header `GALE01`, revision byte `2`, expected `main.dol` SHA-1 |
| Reproducible decompilation build | Validated on current Linux host | `spikes/001-reproducible-build/README.md`; wrapper rerun passed in 33.62 s |
| Profile/mod schemas | MVP complete | `schemas/*.schema.json`, dependency-free semantic validator and tests |
| Profile isolation | MVP complete | `host/src/meleemod/store.py`, atomic promotion test |
| Static plugin profile builds | MVP complete | Disposable worktree overlay produces and stages a modified DOL; standalone Dolphin directly observes hello initialization and frame/input callback markers |
| Filesystem asset composition | MVP complete | Exact targets, traversal rejection, conflict test |
| Dolphin discovery | MVP complete | Explicit executable validation; KDE `/usr/bin/dolphin` is rejected and installed Slippi AppImage is selected; launch helper records PID/exit/timeout |
| Runtime event ABI | PPC build + in-game frame lifecycle | C tests plus Metrowerks PPC ABI/runtime link; standalone Dolphin observes initialization, runtime-ready, frame dispatch, and input-history reads; generated shutdown hook is directly observed with the opt-in diagnostic; normal process shutdown ordering remains pending |
| Plugin dependency composition | Contract complete | Stable topological ordering, cycle detection and static manifest tests |
| Character package security | Validator + offline staging | Checksums, unsafe path/executable rejection, and validated-only offline workspace staging |

## Explicitly pending

- Native crash symbolization is implemented as a bounded, shell-free `addr2line` adapter.
- A real modified `main.dol` boot in standalone Dolphin 2606: clean, static-plugin, and recomposed runtime-plugin ISOs reach the Melee memory-card prompt. Startup and first-frame hello/frame-probe plugins emit initialization and repeated frame/input callback markers through Dolphin's OSREPORT logger. See `docs/evidence/frame-input-observation.md`.
- Bounded emulator shutdown kills the complete Dolphin process group after timeout to avoid a modal Confirm Stop dialog or orphan child process.
- A bounded Slippi launch was attempted with the user ISO; it remained running until timeout, so menu/game boot is not yet confirmed.
- Runtime shutdown observation and overhead budget.
- Production PPC-to-host transport and frame-time budget (development Dolphin GDB mailbox transport now completes a live HELLO/HELLO_ACK exchange; general Dolphin/EXI transport and total game-frame budget remain pending).
- Real Dolphin/Slippi launch smoke tests and exit/log behavior.
- Full interactive GUI recovery testing (Tk Validate/Build/Launch callbacks, bounded real-Dolphin launch cleanup, and callback-failure self-test pass when extracted Tk libraries are supplied; interactive recovery boundaries are documented; base host lacks `libtk8.6`).
- Character model import, HSD/game asset conversion and playable character round trip (deterministic skeleton retargeting and bounded HSD container validation/writing are implemented; game conversion remains pending).
- Validated character packages can now be staged into a non-game Offline workspace; no runtime asset conversion is claimed.
- Character Studio visual move/attribute UI and approved move library (source-preserving core editor is complete).
- First-party plugin visual/training behavior in Dolphin (input-display callback and training heartbeat are directly observed; controller activity, overlay rendering, and training controls remain pending).

Do not label pending features as supported.

## Latest validation

- MeleeMod host tests: 34 passed.
- Character Studio tests: 16 passed.
- Strict C event/input/bridge tests and all SDK example syntax checks pass.
- Standalone Dolphin 2606 clean/modified smoke runner reports both processes started and hard-stops them without leftovers.

- Runtime lifecycle evidence: recomposed startup-probe ISO logged `startup-probe initialized` and `startup-probe runtime-ready observed` through Dolphin OSREPORT.

- Registry validation: signed indexes, bounded HTTPS file/ZIP installation, symlink/traversal/executable rejection, and pre-promotion Character Studio package validation pass.
- Character Studio HSD validation/writer tests pass; no fighter conversion or playable support is claimed.
