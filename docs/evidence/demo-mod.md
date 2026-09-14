# Demo mod loader and game observation

Date: 2026-09-14. The catalog entry `plugins/demo-mod/plugin.json` was loaded by the normal profile loader from a temporary profile that referenced the user-owned GALE01 Rev.02 ISO. The loader resolved `demo-mod` from the repository catalog and classified the harmless plugin as `online-safe`.

The normal `BuildStore` path produced a recomposed ISO. Standalone Dolphin 2606 emitted:

```text
[meleemod] demo-mod: initialized
[meleemod] demo-mod: first frame callback
```

This confirms catalog loading, static PPC composition, ISO promotion, plugin initialization, and first-frame dispatch in the game. The mod is intentionally non-gameplay-changing and has no visual or asset changes. Dolphin was hard-killed at the bounded timeout with no process left behind.
