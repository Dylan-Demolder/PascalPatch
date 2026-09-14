# Release checklist

## Green now

- [x] Host schema/profile tests
- [x] GALE01 Rev.02 hash validation
- [x] Atomic build and exact asset conflict checks
- [x] Redacted diagnostics
- [x] Runtime C host-compile checks
- [x] Character Studio package/core tests
- [x] No game data committed

## Blocking before public alpha

The emulator boot gate is passed. The remaining blockers are production bridge payloads, visual/training runtime behavior, real Slippi online/tournament smoke, character conversion/gameplay, remote folders, and full GUI recovery.

- [x] Known-good Dolphin boot of clean profile (standalone Dolphin 2606 reaches Melee memory-card prompt)
- [x] Known-good boot of one modified DOL (static and recomposed runtime DOLs reach the same prompt)
- [x] Bounded smoke shutdown without a modal confirmation deadlock
- [x] Static PPC runtime linked into DOL (Metrowerks PPC build and recomposed ISO verified)
- [x] Harmless plugin observed in-game (hello initialization and frame/input callback markers)
- [x] Gameplay-changing profile blocked before Slippi process launch (installed Slippi path; online/login smoke remains unavailable)
- [x] Validated Character Studio package staged into an Offline authoring workspace (`game_integration: false`; no game composition claimed)
- [ ] Character-select and playable-match round trip
- [ ] GUI display workflow and full recovery test (invalid-profile dismissal and Tk Validate/Build/Launch self-tests are observed with a fake executable; bounded real-Dolphin launch/cleanup is observed and bounded callback-failure recovery is tested; interactive recovery remains)
