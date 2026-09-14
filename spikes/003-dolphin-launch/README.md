# Spike 003 — Dolphin launch integration

**Verdict: PARTIAL.**

- `/usr/bin/dolphin` exists but is the KDE file manager; filename-only discovery is unsafe and is rejected.
- `/home/dyland/.config/Slippi Launcher/netplay/Slippi_Online-x86_64.AppImage` exposes Dolphin Emulator's `--exec`/batch interface and is selected by validated discovery.
- A bounded `-b -e <user ISO>` launch was attempted. The process remained active until the 20-second test timeout and emitted environment warnings, so this proves executable invocation only, not a verified menu boot.
- A future smoke test must capture emulator logs and verify a known menu state or clean exit.
