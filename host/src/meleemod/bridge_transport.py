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
