from __future__ import annotations
import socket, time
from pathlib import Path

MAX_TRANSFER = 64 * 1024
RUNTIME_MAX_PAYLOAD = 1024
RUNTIME_FRAME_CAPACITY = 30 + RUNTIME_MAX_PAYLOAD
MAILBOX_GAME_OFFSET = 16 + RUNTIME_FRAME_CAPACITY
MAX_PACKET = 2 * MAX_TRANSFER + 256

class DolphinGdbError(ConnectionError):
    pass

def _packet(payload: bytes) -> bytes:
    if b"#" in payload or b"$" in payload:
        raise DolphinGdbError("GDB payload contains a packet delimiter")
    return b"$" + payload + b"#" + f"{sum(payload) & 0xff:02x}".encode("ascii")

class DolphinGdbClient:
    """Bounded subset of Dolphin's GDB remote protocol for memory mailbox tests."""
    def __init__(self, sock: socket.socket, timeout: float = 3.0):
        self.sock = sock
        self.timeout = timeout
        self.sock.settimeout(timeout)
        self.closed = False

    @classmethod
    def tcp(cls, host: str = "127.0.0.1", port: int = 24689, timeout: float = 3.0):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        try: sock.connect((host, port))
        except OSError as exc:
            sock.close()
            raise DolphinGdbError("Dolphin GDB connection failed") from exc
        return cls(sock, timeout)

    @classmethod
    def unix(cls, path, timeout: float = 3.0):
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        try: sock.connect(str(Path(path)))
        except OSError as exc:
            sock.close()
            raise DolphinGdbError("Dolphin GDB socket connection failed") from exc
        return cls(sock, timeout)

    def _read_packet(self) -> bytes:
        deadline = time.monotonic() + self.timeout
        state = 0; payload = bytearray(); checksum = bytearray()
        while time.monotonic() < deadline and len(payload) <= MAX_PACKET:
            remaining = max(0.001, deadline - time.monotonic())
            self.sock.settimeout(remaining)
            try: chunk = self.sock.recv(1)
            except socket.timeout as exc: raise DolphinGdbError("Dolphin GDB receive timeout") from exc
            except OSError as exc: raise DolphinGdbError("Dolphin GDB receive failed") from exc
            if not chunk: raise DolphinGdbError("Dolphin GDB peer disconnected")
            char = chunk[0]
            if state == 0:
                if char == ord("+"): continue
                if char == ord("$"): state = 1; continue
                if char == ord("-"): continue
                raise DolphinGdbError("invalid Dolphin GDB packet prefix")
            if state == 1:
                if char == ord("#"): state = 2
                else: payload.append(char)
                continue
            checksum.append(char)
            if len(checksum) == 2:
                try: received = int(checksum.decode("ascii"), 16)
                except ValueError as exc: raise DolphinGdbError("invalid Dolphin GDB checksum") from exc
                if received != (sum(payload) & 0xff):
                    try: self.sock.sendall(b"-")
                    except OSError: pass
                    raise DolphinGdbError("Dolphin GDB checksum mismatch")
                try: self.sock.sendall(b"+")
                except OSError as exc: raise DolphinGdbError("Dolphin GDB acknowledgment failed") from exc
                return bytes(payload)
        raise DolphinGdbError("Dolphin GDB packet exceeds limit")

    def command(self, payload: bytes) -> bytes:
        if self.closed: raise DolphinGdbError("Dolphin GDB client is closed")
        if not isinstance(payload, bytes) or not payload or len(payload) > MAX_PACKET:
            raise DolphinGdbError("invalid Dolphin GDB command")
        try: self.sock.sendall(_packet(payload))
        except (socket.timeout, OSError) as exc: raise DolphinGdbError("Dolphin GDB send failed") from exc
        response = self._read_packet()
        if response.startswith(b"E"):
            raise DolphinGdbError("Dolphin GDB command failed: " + response.decode("ascii", "replace"))
        return response

    def stop_reason(self) -> bytes:
        return self.command(b"?")

    def read_memory(self, address: int, size: int) -> bytes:
        if not 0 <= address <= 0xffffffff or not 0 <= size <= MAX_TRANSFER:
            raise ValueError("invalid bounded GDB memory read")
        response = self.command(f"m{address:x},{size:x}".encode("ascii"))
        try: result = bytes.fromhex(response.decode("ascii"))
        except ValueError as exc: raise DolphinGdbError("invalid Dolphin GDB memory response") from exc
        if len(result) != size: raise DolphinGdbError("short Dolphin GDB memory response")
        return result

    def write_memory(self, address: int, data: bytes) -> None:
        if not 0 <= address <= 0xffffffff or len(data) > MAX_TRANSFER or address + len(data) > 0x100000000:
            raise ValueError("invalid bounded GDB memory write")
        response = self.command(f"M{address:x},{len(data):x}:".encode("ascii") + data.hex().encode("ascii"))
        if response != b"OK": raise DolphinGdbError("Dolphin GDB memory write was not acknowledged")

    def close(self):
        if not self.closed:
            self.closed = True
            try: self.sock.close()
            except OSError: pass

    def __enter__(self): return self
    def __exit__(self, exc_type, exc, tb): self.close()


class DolphinGdbMailbox:
    """Mailbox adapter for a runtime mailbox exposed at a known emulated address."""
    def __init__(self, client: DolphinGdbClient, address: int):
        if not 0 <= address <= 0xffffffff - (16 + 2 * RUNTIME_FRAME_CAPACITY):
            raise ValueError("invalid mailbox address")
        self.client = client
        self.address = address
        header = client.read_memory(address, 16)
        if header[:4] != b"MMBX" or int.from_bytes(header[4:8], "big") != 1:
            raise DolphinGdbError("invalid MeleeMod bridge mailbox header")

    def _size(self, offset: int) -> int:
        return int.from_bytes(self.client.read_memory(self.address + offset, 4), "big")

    def _publish_size(self, offset: int, size: int):
        self.client.write_memory(self.address + offset, size.to_bytes(4, "big"))

    def send(self, frame: bytes) -> None:
        if not isinstance(frame, bytes) or len(frame) > RUNTIME_FRAME_CAPACITY:
            raise ValueError("bridge frame exceeds mailbox limit")
        if self._size(12):
            raise DolphinGdbError("bridge mailbox game-to-host slot is full")
        self.client.write_memory(self.address + 16, frame)
        self._publish_size(8, len(frame))

    def receive(self) -> bytes | None:
        size = self._size(12)
        if not size: return None
        if size > RUNTIME_FRAME_CAPACITY:
            raise DolphinGdbError("bridge mailbox frame length is invalid")
        frame = self.client.read_memory(self.address + MAILBOX_GAME_OFFSET, size)
        self._publish_size(12, 0)
        return frame
