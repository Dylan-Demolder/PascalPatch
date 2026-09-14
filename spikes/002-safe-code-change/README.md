# Spike 002 — One safe code change

**Verdict: PARTIAL.**

A disposable worktree was used; the main Melee decompilation checkout was not changed.

## Build evidence

- Source checkout: `doldecomp/melee` at `2ae2f91719796c518638fabe846330e6ff14359d`
- Change: one additional `OSReport("# MELEEMOD SPIKE 002\n")` after the existing startup banner in `src/melee/gm/gmmain.c`
- Build: `python configure.py --non-matching --build-dir build-spike && ninja`
- Result: all 960 Ninja targets completed
- Clean DOL SHA-1: `08e0bf20134dfcb260699671004527b2d6bb1a45`
- Modified DOL SHA-1: `04bc31fb2bdb1d6f7c3212ce1f6da7e39e2b1e25`
- Modified DOL size: 4,425,216 bytes

## Static plugin source integration result

The host-side static integration adapter now generates a bundle source, includes it into the known linked `gmmain.c` unit in a disposable Git worktree, and runs the complete decompilation build. This succeeded without modifying the original checkout.

- Generated static-plugin DOL SHA-1: `3178ff32e269796a0c6322ea3fb95535a6de35a5`
- Generated DOL size: 4,425,216 bytes
- Clean DOL SHA-1: `08e0bf20134dfcb260699671004527b2d6bb1a45`
- The generated DOL contains the plugin marker string `MELEEMOD STATIC PLUGIN`
- No generated DOL or Nintendo data is stored in this repository

This validates static source composition and link placement. It does not yet validate in-game execution.

## Emulator result

A clean extracted layout and a modified extracted layout were tested with the installed Slippi AppImage using batch execution. Both aborted before a menu/game state could be observed. This is currently indistinguishable from an emulator/display/environment problem because the clean input fails the same way. No boot success is claimed.

## Static plugin-link finding

A second disposable attempt added `src/meleemod/runtime.c` as a separate object and called it from `gmmain.c`. The new object compiled, but the linker rejected the build because the generated DOL project configuration contains only units from the original split map. This confirms that static plugin composition must update the decompilation project/link configuration and cannot merely copy a `.c` file into `src`.

## Next evidence required

- Run the clean and modified layouts in a known-good Dolphin/Slippi environment.
- Capture process exit status and emulator log.
- Verify the startup report or another visible effect.
- Only then promote this verdict to `VALIDATED`.
