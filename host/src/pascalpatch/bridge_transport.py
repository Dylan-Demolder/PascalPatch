from __future__ import annotations
import socket
from typing import Callable
from .bridge import HEADER, MAX_PAYLOAD, Message, decode, encode

class BridgeTransportError(ConnectionError):
    pass

def _read_exact(sock: socket.socket, size: int) -> bytes:
    if size < 0 or size > HEADER + MAX_PAYLOAD:
        raise BridgeTransportError("invalid bridge read size")
    data=bytearray()
    while len(data) < size:
        try: chunk=sock.recv(size-len(data))
        except socket.timeout as exc: raise BridgeTransportError("bridge receive timeout") from exc
        except OSError as exc: raise BridgeTransportError("bridge receive failed") from exc
        if not chunk: raise BridgeTransportError("bridge peer disconnected")
        data.extend(chunk)
    return bytes(data)

def receive(sock: socket.socket) -> Message:
    header=_read_exact(sock,HEADER)
    length=int.from_bytes(header[10:14],"big")
    if length > MAX_PAYLOAD: raise BridgeTransportError("bridge payload exceeds limit")
    return decode(header+_read_exact(sock,length))

def send(sock: socket.socket, message: Message) -> None:
    frame=encode(message)
    try: sock.sendall(frame)
    except socket.timeout as exc: raise BridgeTransportError("bridge send timeout") from exc
    except OSError as exc: raise BridgeTransportError("bridge send failed") from exc

def serve_once(sock: socket.socket, handler: Callable[[Message], Message | None]) -> Message | None:
    request=receive(sock); response=handler(request)
    if response is not None: send(sock,response)
    return response


class UnixBridgeServer:
    """Single-request, local-only bridge listener.

    The socket is private to the current user and removed on close. The server
    handles one bounded request at a time; callers can recreate it for the
    next heartbeat or runtime session.
    """
    def __init__(self, path, timeout=3.0):
        self.path=__import__("pathlib").Path(path).expanduser()
        self.timeout=timeout; self._server=None
    def __enter__(self):
        self.path.parent.mkdir(parents=True,exist_ok=True)
        try: self.path.unlink()
        except FileNotFoundError: pass
        self._server=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        self._server.settimeout(self.timeout); self._server.bind(str(self.path)); self.path.chmod(0o600); self._server.listen(1)
        return self
    def serve_once(self, handler):
        if self._server is None: raise BridgeTransportError("bridge server is not open")
        try: conn,_=self._server.accept()
        except socket.timeout as exc: raise BridgeTransportError("bridge accept timeout") from exc
        with conn:
            conn.settimeout(self.timeout); return serve_once(conn,handler)
    def close(self):
        if self._server is not None:
            self._server.close(); self._server=None
        try: self.path.unlink()
        except FileNotFoundError: pass
    def __exit__(self,exc_type,exc,tb): self.close()
