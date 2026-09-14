# Spike 003 — Dolphin launch integration

**Verdict: PARTIAL.**

- `/usr/bin/dolphin` is the KDE file manager and is rejected by behavior-based discovery.
- The Slippi Online AppImage may present Login/front-end behavior, so it is not used as evidence for original Melee title-screen behavior.
- The installed Slippi Playback AppImage accepts `-b -e <ISO> -v Null`. A clean user ISO remains alive until a bounded timeout, proving the launch path reaches a running emulator process.
- A temporary ISO with the generated static-plugin DOL also remains alive after deferred plugin initialization.
- No visible menu state or runtime log is captured in the current headless/null-renderer environment.
- A standalone Dolphin Emulator package or GUI-capable controlled runner is still required for final smoke verification.
