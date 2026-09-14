# Dolphin GDB mailbox bridge observation

Date of observation: 2026-09-14. This uses a disposable recomposed ISO built from the user-owned GALE01 Rev.02 ISO. No game data is stored in the repository.

## Emulator transport

Standalone Dolphin 2606 exposes its GDB remote stub when `General.GDBPort` is configured. The bounded host adapter is `host/src/meleemod/dolphin_gdb.py`. It validates packet checksums, enforces packet and memory-transfer limits, applies socket timeouts, and supports only bounded memory reads/writes.

Dolphin was started with a 60-second hard process-group timeout. The adapter connected to `127.0.0.1:24691`, received the stop packet, and sent the GDB `c` packet. The first stop response was:

```text
T0f40:8000522c;01:81566550;
```

A clean-game memory read at `0x804eec00` returned 16 zero bytes. This validates the adapter against the real Dolphin stub, not only a socket mock.

## PPC mailbox

The generated runtime contains a `MMBX` mailbox with a 16-byte header and two bounded 1054-byte slots. The runtime prints the mailbox address through OSREPORT. The modified ISO printed:

```text
[meleemod] bridge mailbox 803d65a4
```

The host then read the live mailbox header at `0x803d65a4`:

```text
4d4d4258000000010000000000000022
```

This means magic `MMBX`, version `1`, host-to-game size `0`, and game-to-host size `0x22` (34 bytes). Reading the game slot produced this valid bridge frame:

```text
4d4d423101010000000100000004b40711a88c7039756fb8a73827eabe2c00000001
```

The host decoded it as a `HELLO` message with request ID `1` and protocol version `1`. It cleared the game-to-host slot and wrote a valid `HELLO_ACK` frame to the host-to-game slot. The runtime consumed the acknowledgment; a subsequent runtime heartbeat frame was observed in the game-to-host slot.

The mailbox publishes payload bytes before its size field. The runtime clears a consumed host slot. This prevents the game from reading a partial GDB write and keeps one bounded frame in flight in each direction.

## Evidence boundary

This directly validates a real Dolphin GDB transport, the PPC mailbox layout, runtime hello generation, host decoding, host acknowledgment, and runtime consumption. It does not claim a general-purpose network bridge, arbitrary payload callbacks, online safety, or gameplay features. The GDB stub must be explicitly enabled for this development transport; release profiles remain fail-closed unless a trusted transport is configured.
