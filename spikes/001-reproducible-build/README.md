# Spike 001 — Reproducible Melee build

**Verdict: VALIDATED for the current Linux development environment.**

Evidence collected on 2026-09-14:

- Checkout: `doldecomp/melee` at commit `2ae2f91719796c518638fabe846330e6ff14359d`
- Input: user-owned `GALE01` Rev.02 ISO
- `configure.py` and Ninja build are present and configured
- Generated `build/GALE01/main.dol` size: 4,425,184 bytes
- SHA-1: `08e0bf20134dfcb260699671004527b2d6bb1a45`
- The ISO's embedded `main.dol` produces the same SHA-1

This validates the known toolchain and checkout, not cross-platform reproducibility. Tool versions and a clean checkout must still be pinned before release.
