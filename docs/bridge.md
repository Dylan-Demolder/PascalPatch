# Host/runtime bridge

The bridge wire frame is versioned (`MMB1`), bounded to 64 KiB, and protected by a truncated SHA-256 payload checksum. `host/src/meleemod/bridge_transport.py` provides the host-side stream adapter:

- reads exactly one complete frame, including fragmented socket reads;
- rejects invalid lengths, versions, checksums, and disconnected peers;
- converts socket timeout and OS errors into `BridgeTransportError`;
- sends one complete response with `sendall`.

The transport is intentionally request/response and does not assume gameplay state. A future runtime endpoint can use the same frame format. Heartbeat messages should use a reserved message kind and monotonically increasing request IDs; a timeout or disconnect must tear down the session and leave gameplay unchanged.

This is a host transport contract, not proof of an in-game network endpoint.


## Bounded emulator shutdown

The launcher hard-kills an emulator when its wait timeout expires. It does not send graceful termination first, because Dolphin can open a modal Confirm Stop dialog that blocks automation.


`UnixBridgeServer` provides a private `AF_UNIX` listener for local host/runtime integration. It binds mode `0600`, accepts one request with a bounded timeout, uses the same frame validation, and removes the socket on close. It is host-side transport infrastructure; a PPC runtime socket implementation is still required before claiming an in-game endpoint.


The runtime ABI emits `MM_EVENT_RUNTIME_READY` after runtime initialization and startup-phase plugin registration; standalone Dolphin logs confirm it. The real game frame hook now samples `HSD_PadCopyStatus[4]`, dispatches `MM_EVENT_FRAME`, and a first-party probe reads the resulting history. Shutdown remains separate lifecycle work.
