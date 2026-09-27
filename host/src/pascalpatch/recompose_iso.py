from __future__ import annotations
import os, shutil, struct, tempfile
from pathlib import Path

ALIGN=0x20
HEADER_FST=0x424
HEADER_DOL=0x420

def _align(value, alignment=ALIGN): return (value+alignment-1)//alignment*alignment

def _copy_range(src,dst,start,end,chunk=1024*1024):
    src.seek(start); dst.seek(start)
    remaining=end-start
    while remaining:
        data=src.read(min(chunk,remaining))
        if not data: raise ValueError("source ISO ended unexpectedly")
        dst.write(data); remaining-=len(data)

def recompose_iso(base_iso, main_dol, output):
    """Write a new ISO with a replacement DOL and shifted FST/file region.

    The input is never modified. Only the system DOL/FST region and recorded
    file offsets are changed; all game data bytes are copied verbatim.
    """
    base=Path(base_iso).expanduser().resolve(); dol=Path(main_dol).expanduser().resolve(); out=Path(output).expanduser().resolve()
    if base==out: raise ValueError("output must differ from base ISO")
    if not base.is_file() or not dol.is_file(): raise ValueError("base ISO and DOL must be files")
    with base.open("rb") as src:
        src.seek(HEADER_DOL); dol_offset=int.from_bytes(src.read(4),"big")
        old_fst=int.from_bytes(src.read(4),"big"); fst_size=int.from_bytes(src.read(4),"big")
        if old_fst<=dol_offset or fst_size<12: raise ValueError("invalid GameCube disc header")
        src.seek(old_fst); fst=bytearray(src.read(fst_size))
        if len(fst)!=fst_size: raise ValueError("truncated FST")
        entry_count=int.from_bytes(fst[8:12],"big")
        if entry_count<1 or entry_count*12>fst_size: raise ValueError("invalid FST entry count")
        replacement=dol.read_bytes(); new_fst=_align(dol_offset+len(replacement)); delta=new_fst-old_fst
        if delta<0: raise ValueError("replacement unexpectedly moves FST backward")
        # File entries hold disc offsets. Directory entries use the same bytes
        # for parent/next indices and must not be changed.
        for index in range(entry_count):
            pos=index*12
            if fst[pos] & 1: continue
            offset=int.from_bytes(fst[pos+4:pos+8],"big")
            if offset>=old_fst:
                fst[pos+4:pos+8]=(offset+delta).to_bytes(4,"big")
        out.parent.mkdir(parents=True,exist_ok=True)
        fd,stage_name=tempfile.mkstemp(prefix=out.name+"-",dir=out.parent); os.close(fd); stage=Path(stage_name)
        try:
            with stage.open("w+b") as dst:
                dst.truncate(base.stat().st_size+delta)
                _copy_range(src,dst,0,dol_offset)
                dst.seek(dol_offset); dst.write(replacement)
                dst.write(b"\0"*(new_fst-(dol_offset+len(replacement))))
                src.seek(old_fst); dst.seek(new_fst)
                shutil.copyfileobj(src,dst,length=1024*1024)
                dst.seek(HEADER_FST); dst.write(new_fst.to_bytes(4,"big"))
                dst.seek(new_fst); dst.write(fst)
                dst.flush(); os.fsync(dst.fileno())
            os.replace(stage,out)
        finally:
            try: stage.unlink()
            except FileNotFoundError: pass
    return out
