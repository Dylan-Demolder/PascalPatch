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

This validates static source composition, profile integration and link placement. The integrated `BuildStore` path also produced an isolated build metadata record with `plugin_composition: static-source-overlay` and the same generated DOL hash. It does not yet validate in-game execution.

## PPC ABI integration result

The real SDK hello-plugin was then composed with the PPC-safe runtime implementation. The generated source included the event runtime, `mm_runtime_init`, `mm_log`, the plugin context, and `plugin_init`. The Metrowerks GameCube compiler and linker completed successfully.

- ABI/frame-hook DOL SHA-1: `fec71eb3043db374c93163390109e0984b2f52e4`
- ABI/frame-hook DOL size: 4,425,472 bytes
- The output contains `hello-plugin initialized` and `[meleemod]` strings
- The generated game-mode loop calls `mm_meleemod_frame()` before each mode step
- Clean DOL remains `08e0bf20134dfcb260699671004527b2d6bb1a45`

This validates PPC compilation, static runtime inclusion, plugin-context initialization and link placement. It still does not prove that Dolphin reaches the initialization path.

## Emulator result

The initial test used Slippi Online and was misleading because its front-end can show Login. Extracted directories are also not valid Dolphin GameCube launch targets; Dolphin treated them as NAND content.

A temporary ISO was then created from the user ISO. Small static DOLs fit the existing 32-byte padding; larger runtime DOLs use the safe ISO recomposer, which shifts the FST and updates recorded file offsets. The original ISO was not modified. `dtk disc verify` accepts the recomposed ISO as a lossless GALE01 Rev.02 image.

Standalone Dolphin Emulator 2606 was extracted from the Arch package (not Slippi). The exact user ISO and both a static-plugin ISO and recomposed runtime-plugin ISO reach the same Melee prompt: `The Memory Card in Slot A has no saved Game Data. Create Game Data?`.

Verified generated DOL hashes include static deferred plugin `2ee9b3c64f3d73870b95eef3d50ec505f128ebce` and runtime hello plugin `3773dd04acb95f9ee551d9e0107ff7a5cc5bb46d`.

Using the installed Slippi Playback build with `-b -e <ISO> -v Null`:

- Clean user ISO: process remained alive until the 12-second timeout (`124`).
- Fresh deferred-init static plugin ISO: process also remained alive until timeout (`124`) and did not segfault.
- The previous early-initialization version segfaulted; moving plugin initialization to the first game-loop frame fixed that crash.

This demonstrates that the deferred static plugin build does not immediately crash the emulator. A visible menu/title state and plugin log still require a GUI-capable, log-capturing Dolphin test.

## Static plugin-link finding

A second disposable attempt added `src/meleemod/runtime.c` as a separate object and called it from `gmmain.c`. The new object compiled, but the linker rejected the build because the generated DOL project configuration contains only units from the original split map. This confirms that static plugin composition must update the decompilation project/link configuration and cannot merely copy a `.c` file into `src`.

## Next evidence required

- Run the clean and modified layouts in a known-good Dolphin/Slippi environment.
- Capture process exit status and emulator log.
- Verify the startup report or another visible effect.
- Only then promote this verdict to `VALIDATED`.
