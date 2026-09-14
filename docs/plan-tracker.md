# Project plan tracker

This tracker separates implemented contracts from features that still need game/runtime evidence. A feature is not marked complete merely because a schema or stub exists.

## MeleeMod

| Task | Status | Evidence / next proof |
|---|---|---|
| Spike 001 reproducible build | VALIDATED on current Linux host | `spikes/001-reproducible-build/README.md`; rerun wrapper passes |
| Spike 002 safe code change | PARTIAL | Disposable changed DOL builds; clean and modified emulator runs abort equally |
| Spike 003 Dolphin/Slippi launch | PARTIAL | Executable behavior discovery works; menu boot not confirmed |
| Spike 004 host/runtime bridge | DEFERRED | No transport selected until runtime is booted |
| Task 1 manifests | MVP COMPLETE | Versioned schemas and semantic validators |
| Task 2 profile loading | MVP COMPLETE | Dependency and safety resolution tests |
| Task 3 game/Dolphin discovery | MVP COMPLETE | ISO hash and emulator identity checks |
| Task 4 isolated atomic builds | MVP COMPLETE | Staging/current build test |
| Task 5 asset mods | MVP COMPLETE | Exact target/conflict/traversal tests |
| Task 6 CLI | MVP COMPLETE | Validate/build/launch/log commands; launch smoke proof pending |
| Task 7 decomp build | MVP COMPLETE on known checkout | Pinned wrapper and expected hash |
| Task 8 runtime ABI | HOST COMPLETE | C event ABI tests; PPC link proof pending |
| Task 9 sample plugin | CONTRACT ONLY | Manifest and C example; in-game load pending |
| Task 10 plugin composition | CONTRACT COMPLETE | Dependency order and static manifest; link step pending |
| Task 11 bridge | Protocol contract complete | Versioned bounded checksum frames; transport/runtime integration pending |
| Task 12 input history | ABI primitive complete | Bounded ring buffer and reset/wrap tests; game sampling hook pending |
| Task 13 input display | NOT STARTED | Requires overlay/render hook |
| Task 14 training tools | NOT STARTED | Requires frame/state hooks |
| Task 15 GUI | NOT STARTED | CLI/core must remain source of truth |
| Task 16 safety | MVP COMPLETE | Capability-based fail-closed decisions |
| Task 17 diagnostics | Host report complete | Redacted profile/plugin/runtime report; native crash symbolization pending |
| Task 18 registry | CONTRACT COMPLETE | Schema/hash validator; installer/update flow pending |
| Task 19 CI | COMPLETE for host/core checks | GitHub Actions workflows run dependency-free tests; game smoke remains self-hosted only |
| Task 20 documentation/legal | MVP COMPLETE | README, architecture, legal notice, status |
| Task 21 release candidate | NOT STARTED | Blocked by emulator and plugin/character integration |

## Character Studio

| Area | Status | Evidence / next proof |
|---|---|---|
| Versioned package contracts | MVP COMPLETE | Character/moveset/asset schemas |
| glTF/GLB import validation | MVP COMPLETE | Structural, URI and budget tests |
| Skeleton mapping | BASIC COMPLETE | Deterministic name mapping; transform retargeting pending |
| Move editor/library | NOT STARTED | Schema only |
| Attribute editor | NOT STARTED | Schema only |
| Preview/test scene | NOT STARTED | Requires renderer and runtime data |
| Deterministic package export | MVP COMPLETE | Repeated export hashes match |
| MeleeMod package validation | MVP COMPLETE | Cross-project package validation passes |
| Melee composition | NOT STARTED | Requires runtime asset/code conversion |
| Playable round trip | NOT STARTED | Requires Dolphin/game integration |

The next gating milestone is a known-good emulator boot of both the clean and modified DOL.
