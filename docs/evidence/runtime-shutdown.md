# Generated runtime shutdown observation

Date: 2026-09-14. A disposable host fixture generated the current static bundle with a plugin whose `plugin_shutdown` increments a counter. The fixture compiled the generated bundle with strict C flags, supplied host stubs for `OSReport` and `HSD_PadCopyStatus`, called `mm_meleemod_static_shutdown()` twice, and printed:

```text
shutdown_seen=1
```

This directly verifies that the generated shutdown path invokes the plugin shutdown hook once and is idempotent. It is host-fixture evidence, not proof that Dolphin reaches shutdown during process termination; the latter remains pending because bounded Dolphin termination uses a hard process-group kill.
