# Training-tools frame heartbeat observation

Date: 2026-09-14. This run used a disposable recomposed ISO built from the user-owned GALE01 Rev.02 ISO. No game data is stored in the repository.

The profile selected only `sdk/examples/training-tools/plugin.c` with `mode=offline`. The plugin was compiled and linked through the normal disposable PPC static-bundle path. Standalone Dolphin 2606 ran with OSREPORT logging and a bounded 10-second process-group timeout.

The log contained repeated markers:

```text
[meleemod] training-tools: frame heartbeat
```

The markers were produced from the plugin's `MM_EVENT_FRAME` callback. This directly confirms training-tools plugin initialization, frame subscription, frame dispatch, and bounded callback execution in Dolphin. It does not claim frame advance, reset, hit events, or a visual training overlay. The emulator was hard-killed at timeout and left no process or modal dialog.
