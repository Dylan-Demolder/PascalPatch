# Host/runtime bridge

The bridge wire frame is versioned (`MMB1`), bounded to 64 KiB, and protected by a truncated SHA-256 payload checksum. `host/src/pascalpatch/bridge_transport.py` provides the host-side stream adapter:

- reads exactly one complete frame, including fragmented socket reads;
- rejects invalid lengths, versions, checksums, and disconnected peers;
- converts socket timeout and OS errors into `BridgeTransportError`;
- sends one complete response with `sendall`.

The transport is intentionally request/response and does not assume gameplay state. A future runtime endpoint can use the same frame format. Heartbeat messages should use a reserved message kind and monotonically increasing request IDs; a timeout or disconnect must tear down the session and leave gameplay unchanged.

This is a host transport contract, not proof of a physical in-game transport endpoint. The PPC runtime now also contains `runtime/include/pascalpatch/bridge.h` and `runtime/src/bridge.c`: a transport-neutral bounded endpoint that validates the same `MMB1` framing and truncated SHA-256 checksum, negotiates `MM_BRIDGE_VERSION`, handles heartbeat/ack messages, dispatches bounded data callbacks, and resets on disconnect. Its read/write callbacks are deliberately supplied by a future platform adapter; the callback layer itself is covered by `runtime/tests/test_bridge.c`.


## Bounded emulator shutdown

The launcher hard-kills an emulator when its wait timeout expires. It does not send graceful termination first, because Dolphin can open a modal Confirm Stop dialog that blocks automation.


`UnixBridgeServer` provides a private `AF_UNIX` listener for local host/runtime integration. It binds mode `0600`, accepts one request with a bounded timeout, uses the same frame validation, and removes the socket on close. The PPC endpoint is transport-neutral and cannot open this Unix socket. A bounded Dolphin GDB memory adapter is now implemented for development and has directly completed a live HELLO/HELLO_ACK exchange through the mailbox; it requires an explicitly enabled GDB stub and a discovered mailbox address. A production/general Dolphin/EXI adapter remains separate work.


The runtime ABI emits `MM_EVENT_RUNTIME_READY` after runtime initialization and startup-phase plugin registration; standalone Dolphin logs confirm it. The real game frame hook now samples `HSD_PadCopyStatus[4]`, dispatches `MM_EVENT_FRAME`, and a first-party probe reads the resulting history. The PPC bridge state machine is compile/test validated, and the Dolphin GDB mailbox adapter has completed a live handshake. A production/general Dolphin/EXI adapter and shutdown observation remain separate lifecycle work.
