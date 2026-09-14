# Decompilation build

`host/src/meleemod/decomp_build.py` runs the pinned configure and Ninja commands as argument arrays, captures output, and verifies the expected `main.dol` SHA-1. It does not interpolate user paths into a shell command.

The pinned current evidence is in `tooling/toolchains/melee-1.02.json`. A clean matching build works on the current Linux checkout. A modded build must use a disposable worktree or a generated source overlay. The decompilation split configuration owns DOL section placement; adding an arbitrary new object requires explicit link/split configuration and is not currently automatic.
