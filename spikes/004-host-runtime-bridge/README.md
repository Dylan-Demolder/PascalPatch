# Spike 004 — Host/runtime bridge

**Verdict: HOST TRANSPORT VALIDATED; PPC ENDPOINT DEFERRED.**

The host-side protocol is implemented and tested in `host/src/meleemod/bridge.py` and `bridge_transport.py`:

- frames use `MMB1`, version 1, bounded payloads (64 KiB), request IDs, and a truncated SHA-256 payload checksum;
- fragmented stream reads are reassembled and invalid headers, lengths, versions, checksums, timeouts, and disconnects fail closed;
- `UnixBridgeServer` is local-only, accepts one bounded request, uses socket mode `0600`, and removes its socket on close.

The runtime cannot open a Unix socket from PPC code. `runtime/src/bridge.c` provides the PPC-compatible, transport-neutral endpoint state machine. It preserves the frame contract, negotiates version, handles bounded heartbeat/data messages, and resets state on disconnect. `runtime/tests/test_bridge.c` validates the endpoint against known SHA-256 frames. A bounded Dolphin GDB memory adapter now connects that endpoint to a mailbox, and a live standalone-Dolphin run completed HELLO/HELLO_ACK; see `docs/evidence/dolphin-gdb-bridge.md`. A production/general Dolphin/EXI adapter and arbitrary payload callback path remain pending, so bridge-dependent gameplay features stay disabled by profile capability checks.
