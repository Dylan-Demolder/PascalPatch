# Input-display callback observation

Date: 2026-09-14. A disposable GALE01 Rev.02 recomposed ISO was built through the normal static PPC composition path with `sdk/examples/input-display/plugin.c`. The sample emits a one-time marker on its first frame callback so that a run with no controller activity is still distinguishable from a missing subscription.

Standalone Dolphin 2606 emitted:

```text
[meleemod] input-display: frame callback active
```

The bounded run directly confirms plugin initialization, `MM_EVENT_FRAME` subscription, callback dispatch, and input-history access. No controller button activity was observed, so no button-display claim is made. The sample still has no in-game visual overlay renderer.
