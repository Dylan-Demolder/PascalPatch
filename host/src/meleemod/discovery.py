from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
import hashlib, os, shutil
from .errors import DiscoveryError, ValidationError

EXPECTED_DOL_SHA1="08e0bf20134dfcb260699671004527b2d6bb1a45"
GAME_ID=b"GALE01"
@dataclass(frozen=True)
class GameInput:
    path: Path
    kind: str
    game_id: str
    revision: int
    main_dol_sha1: str|None

def _sha1_file(p):
 h=hashlib.sha1()
 with p.open("rb") as f:
  for chunk in iter(lambda:f.read(8*1024*1024),b""): h.update(chunk)
 return h.hexdigest()

def _dol_size(f):
 h=f.read(0x100); sections=[]
 for i in range(7): sections.append((int.from_bytes(h[i*4:i*4+4],"big"),int.from_bytes(h[0x90+i*4:0x94+i*4],"big")))
 for i in range(11): sections.append((int.from_bytes(h[0x1c+i*4:0x20+i*4],"big"),int.from_bytes(h[0xac+i*4:0xb0+i*4],"big")))
 vals=[o+s for o,s in sections if o and s]
 if not vals: raise DiscoveryError("main.dol has no valid sections")
 return max(vals)

def inspect_iso(path: str|Path) -> GameInput:
 p=Path(path).expanduser().resolve()
 if not p.is_file(): raise DiscoveryError(f"game file is not readable: {p}")
 with p.open("rb") as f:
  h=f.read(0x440)
  if len(h)<0x428 or h[:6]!=GAME_ID: raise DiscoveryError("not a supported GALE01 GameCube disc")
  revision=h[7]
  if revision != 2: raise DiscoveryError(f"unsupported GALE01 revision byte {revision}; expected Rev.02 / 1.02")
  dol_offset=int.from_bytes(h[0x420:0x424],"big")
  f.seek(dol_offset); size=_dol_size(f); f.seek(dol_offset); sha=hashlib.sha1(f.read(size)).hexdigest()
 if sha != EXPECTED_DOL_SHA1: raise DiscoveryError(f"GALE01 Rev.02 main.dol hash mismatch: {sha}")
 return GameInput(p,"iso","GALE01",revision,sha)

def inspect_directory(path: str|Path) -> GameInput:
 p=Path(path).expanduser().resolve()
 if not p.is_dir(): raise DiscoveryError(f"game directory is not readable: {p}")
 # Dolphin extracted-game layouts commonly expose sys/main.dol or main.dol.
 candidates=[p/"sys"/"main.dol",p/"main.dol"]
 dol=next((x for x in candidates if x.is_file()),None)
 if dol is None: raise DiscoveryError("game directory has no sys/main.dol or main.dol")
 sha=_sha1_file(dol)
 if sha != EXPECTED_DOL_SHA1: raise DiscoveryError(f"main.dol hash mismatch: {sha}")
 return GameInput(p,"directory","GALE01",2,sha)

def inspect_game(path):
 p=Path(path).expanduser()
 return inspect_iso(p) if p.is_file() else inspect_directory(p)

def _looks_like_emulator(path):
 try:
  import subprocess
  r=subprocess.run([str(path),"--help"],capture_output=True,text=True,timeout=5)
  text=(r.stdout+r.stderr).lower()
  return "loads the specified file" in text and ("batch" in text or "video_backend" in text)
 except (OSError,subprocess.SubprocessError):
  return False

def find_dolphin(explicit=None):
 candidates=[]
 if explicit: candidates=[Path(explicit).expanduser()]
 else:
  import glob
  for name in ("dolphin-emu","dolphin"):
   found=shutil.which(name)
   if found: candidates.append(Path(found))
  candidates.extend(Path(x) for x in glob.glob(str(Path.home()/".config/Slippi Launcher"/"**/Slippi*.AppImage"),recursive=True))
 for p in candidates:
  if p.is_file() and os.access(p,os.X_OK) and _looks_like_emulator(p): return p.resolve()
 if explicit: raise DiscoveryError(f"not a Dolphin Emulator executable: {candidates[0] if candidates else explicit}")
 raise DiscoveryError("Dolphin Emulator/Slippi executable not found; provide --dolphin")
