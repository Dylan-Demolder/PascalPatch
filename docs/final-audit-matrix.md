# Final project-plan audit matrix

This matrix is the completion gate for the current scope. `Verified` requires code plus a test or direct observation. `Partial` records useful evidence without claiming the full feature. `Blocked` identifies the missing external capability or user-owned asset.

| Requirement | State | Evidence / next gate |
|---|---|---|
| GALE01 Rev.02 identity and source protection | Verified | ISO/DOL hashes and clean source checkouts |
| Reproducible build and atomic promotion | Verified | Spike 001, isolated worktrees, staged ISO tests |
| Static PPC composition and runtime ABI | Verified | Strict PPC link and standalone Dolphin startup/frame/input evidence |
| Plugin lifecycle and shutdown hook | Partial | Direct Dolphin opt-in shutdown-hook probe; normal emulator process shutdown remains unverified |
| Runtime overhead | Partial | Host dispatcher benchmark and opt-in Dolphin frame-hook tick samples; total game-frame budget remains unmeasured |
| Host bridge transport | Verified | Bounded checksummed Unix transport tests |
| PPC bridge | Partial | Live Dolphin GDB mailbox HELLO/HELLO_ACK and state-machine tests; production payload transport remains absent |
| Input history | Verified | Direct frame/input observation in Dolphin |
| Input display overlay | Partial | Input-display callback observed; visual renderer and controller activity remain unverified |
| Training tools | Partial | Frame heartbeat observed; frame advance, reset, and hit events remain unimplemented |
| Safety policy | Verified for pre-launch block | Installed-Slippi-path gameplay-changing profile rejected before process launch; online/login smoke remains unavailable |
| Registry trust and installation | Verified | Ed25519 trust/index tests, bounded HTTPS file/ZIP installation, safe extraction, package validation |
| Character package Offline staging | Verified for authoring scope | Atomic staging evidence with `game_integration: false` |
| HSD container layer | Verified for container scope | Bounded validator and deterministic writer tests |
| Melee fighter conversion | Blocked | Requires HSD object/animation/`ftData`/`PlCo.dat` conversion and user-owned assets |
| Playable character round trip | Blocked | Requires conversion, hooks, character-select, and playable-match evidence |
| Character Studio preview | Partial | Deterministic SVG skeleton preview; full Melee renderer scene absent |
| GUI Validate/Build/Launch | Verified for bounded workflow | Tk self-tests, real-Dolphin bounded launch/cleanup, and callback-failure recovery |
| Full interactive GUI recovery | Partial | Pixel-free bounded recovery exists; full human workflow and all modal paths remain unverified |
| Diagnostics and crash capture | Partial | Redacted host reports and symbolization complete; runtime crash capture absent |
| Remote folder support | Blocked/out of archive scope | Remote ZIP is verified; a remote folder protocol is not defined |
| Public-alpha release | Blocked | All partial/blocked rows above require evidence or an explicit scope decision |

The project must not call `goal.complete()` while any row remains `Partial` or `Blocked` unless the user explicitly removes that requirement from scope.
