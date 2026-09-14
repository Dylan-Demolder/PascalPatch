# Frame and input callback observation

Date of the recorded run: 2026-09-14. This evidence uses only the user-owned local GALE01 Rev.02 ISO and a disposable recomposed output. No game data or ISO is stored in this repository.

## What was changed

The static integration hook is inserted in the decompilation's real per-scene frame function, `gm_801A4D34` in `src/melee/gm/gm_1A45.c`. It runs immediately after `lb_800195D0()`, where the game has copied controller status, and before reset handling. The runtime then:

1. initializes deferred first-frame plugins;
2. samples the four `HSD_PadCopyStatus` entries into the bounded input history;
3. dispatches `MM_EVENT_FRAME`.

The disposable worktree is used for composition and the original decompilation checkout is not modified.

## Probe

`sdk/examples/frame-probe/plugin.c` subscribes to `MM_EVENT_FRAME`. Its callback calls `mm_input_history_get(0)` and logs only when the returned sample is non-null and has the same frame number as the event. This checks initialization, frame dispatch, controller-history population, and the plugin callback path without changing gameplay or rendering.

The probe is syntax-checked with strict host GCC and compiled by the normal Metrowerks PPC static-bundle build. The resulting DOL is recomposed into a disposable ISO by `BuildStore`.

## Direct emulator result

Standalone Dolphin 2606 was run headless with OSREPORT logging and a hard process-group timeout. The log contained repeated lines of the form:

```text
[meleemod] frame-probe frame/input observed
```

The first-frame hello build also produced:

```text
[meleemod] hello-plugin initialized
```

The callback markers appeared at approximately 60 Hz for the bounded run. Dolphin was killed as a complete process group at timeout, and the run left no Dolphin process or modal Confirm Stop dialog.

This is direct evidence of the static PPC frame hook, deferred first-frame initialization, event dispatch, and non-empty input-history reads. It does **not** claim that an interactive controller button was observed, that a visual overlay rendered, or that a gameplay/training feature is complete.

## Reproduction outline

From the MeleeMod checkout:

```text
PYTHONPATH=host/src python -m unittest discover -s host/tests -q
# Build a profile containing sdk/examples/frame-probe/plugin.c with:
#   mode=offline
#   decomp_repo=<disposable-capable Melee decomp checkout>
#   decomp_orig=<checkout>/orig/GALE01
#   plugin_source_root=<this checkout>
#   runtime_root=<this checkout>
# Run the resulting ISO with tooling/dolphin_smoke.py or Dolphin 2606,
# enabling OSREPORT and using the documented hard timeout.
```

The source ISO remains unchanged. The recomposed ISO and Dolphin logs are temporary test artifacts and are not release assets.
