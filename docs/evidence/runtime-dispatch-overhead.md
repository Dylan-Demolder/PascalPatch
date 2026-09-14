# Runtime dispatcher overhead benchmark

Date: 2026-09-14. `runtime/tests/bench_dispatch.c` compiled the actual runtime event dispatcher with strict host C flags and ran one million iterations for each case.

Observed on this host:

```text
dispatch_empty_ns=15.75 dispatch_one_subscriber_ns=17.44 callbacks=1000000
```

The benchmark confirms bounded host-compiled dispatcher cost and callback execution. It is not a PPC/Dolphin frame-time measurement; CPU/cache/compiler differences mean it must not be used as an in-game budget. An in-game timing counter and shutdown-order observation remain pending.


## PPC callback timing probe

A disposable `sdk/examples/overhead-probe` static ISO measured only the `mm_input_history_get(0)` call with the game's `OSGetTime` counter. Standalone Dolphin 2606 emitted repeated markers such as:

```text
[meleemod] overhead-probe: 60-frame interval ticks 40540500 input-read ticks 1
```

Other bounded samples reported intervals from 40,540,500 to 49,752,033 raw ticks across 60 callbacks.

The probe ran before the memory-card prompt and was hard-killed at the bounded timeout. This is direct PPC/Dolphin evidence for the measured input-read operation, not total event-dispatch or full-frame overhead.


## Opt-in full generated frame-hook timing

Static composition now supports the optional plugin manifest field `frame_timing: true`. An overhead-probe ISO with this field measured the complete generated `mm_meleemod_frame` body around `MM_EVENT_FRAME` dispatch. Standalone Dolphin emitted:

```text
[meleemod] frame-hook ticks 12473
```

Steady samples were approximately 12,473--12,519 raw ticks; the first sample was 52 ticks during initialization. This measures the generated hook body, input sampling, bridge polling, and plugin dispatch, not the entire Melee frame. Timing is opt-in and diagnostic-only.
