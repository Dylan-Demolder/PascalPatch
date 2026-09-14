# First-party plugins

`input-display` subscribes to frame events and reads the read-only input ring. Its current implementation logs button activity; a Dolphin overlay renderer is still pending. It is declared visual-only.

`training-tools` provides a frame heartbeat subscription and is classified gameplay-changing/offline-only. Frame advance, reset and hit events are not yet implemented.

Neither plugin is advertised as runtime-verified until the emulator smoke test observes its behavior.
