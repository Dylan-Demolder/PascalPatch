from __future__ import annotations
import hashlib, struct
from dataclasses import dataclass
MAGIC=b"MMB1"; VERSION=1; MAX_PAYLOAD=64*1024; HEADER=4+1+1+4+4+16
@dataclass(frozen=True)
class Message: kind:int; request_id:int; payload:bytes
def encode(message:Message)->bytes:
 if not 0<=message.kind<=255 or not 0<=message.request_id<=0xffffffff: raise ValueError("invalid message fields")
 if len(message.payload)>MAX_PAYLOAD: raise ValueError("payload exceeds bridge limit")
 checksum=hashlib.sha256(message.payload).digest()[:16]
 return MAGIC+bytes((VERSION,message.kind))+struct.pack(">II",message.request_id,len(message.payload))+checksum+message.payload
def decode(raw:bytes)->Message:
 if len(raw)<HEADER or raw[:4]!=MAGIC: raise ValueError("invalid bridge header")
 version,kind=raw[4],raw[5]; request,length=struct.unpack(">II",raw[6:14]); expected=raw[14:30]
 if version!=VERSION: raise ValueError("unsupported bridge version")
 if length>MAX_PAYLOAD or len(raw)!=HEADER+length: raise ValueError("invalid bridge payload length")
 payload=raw[HEADER:];
 if hashlib.sha256(payload).digest()[:16]!=expected: raise ValueError("bridge checksum mismatch")
 return Message(kind,request,payload)
