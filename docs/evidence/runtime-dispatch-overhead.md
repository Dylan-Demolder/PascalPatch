# Runtime dispatcher overhead benchmark

Date: 2026-09-14. `runtime/tests/bench_dispatch.c` compiled the actual runtime event dispatcher with strict host C flags and ran one million iterations for each case.

Observed on this host:

```text
dispatch_empty_ns=15.75 dispatch_one_subscriber_ns=17.44 callbacks=1000000
```

The benchmark confirms bounded host-compiled dispatcher cost and callback execution. It is not a PPC/Dolphin frame-time measurement; CPU/cache/compiler differences mean it must not be used as an in-game budget. An in-game timing counter and shutdown-order observation remain pending.
