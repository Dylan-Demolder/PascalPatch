# Host/runtime bridge

The bridge wire frame is versioned (`MMB1`), bounded to 64 KiB, and protected by a truncated SHA-256 payload checksum. `host/src/meleemod/bridge_transport.py` provides the host-side stream adapter:

- reads exactly one complete frame, including fragmented socket reads;
- rejects invalid lengths, versions, checksums, and disconnected peers;
- converts socket timeout and OS errors into `BridgeTransportError`;
- sends one complete response with `sendall`.

The transport is intentionally request/response and does not assume gameplay state. A future runtime endpoint can use the same frame format. Heartbeat messages should use a reserved message kind and monotonically increasing request IDs; a timeout or disconnect must tear down the session and leave gameplay unchanged.

This is a host transport contract, not proof of an in-game network endpoint.
