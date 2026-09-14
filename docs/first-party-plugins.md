# First-party plugins

`input-display` subscribes to frame events and reads the read-only input ring. Its current implementation logs button activity; a Dolphin overlay renderer is still pending. It is declared visual-only.

`training-tools` provides a frame heartbeat subscription and is classified visual-only in its current sample manifest. Its heartbeat is directly observed in standalone Dolphin; frame advance, reset and hit events are not yet implemented.

The training-tools heartbeat is runtime-verified; input-display still has no overlay proof. Neither plugin is advertised as providing unimplemented gameplay or rendering behavior.
