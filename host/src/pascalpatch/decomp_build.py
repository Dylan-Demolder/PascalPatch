from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
import hashlib, json, subprocess, time
from .errors import PascalPatchError
@dataclass(frozen=True)
class BuildReport:
 command:list[str]; returncode:int; duration_seconds:float; stdout:str; stderr:str; main_dol_sha1:str|None
def _sha1(p):
 h=hashlib.sha1();
 with Path(p).open("rb") as f:
  for b in iter(lambda:f.read(8*1024*1024),b""): h.update(b)
 return h.hexdigest()
def run_decomp_build(repo,toolchain,python="python",ninja="ninja"):
 repo=Path(repo).resolve(); cfg=json.loads(Path(toolchain).read_text()); start=time.monotonic(); out=[]; err=[]
 for cmd in ([python,"configure.py"],[ninja]):
  if cmd[0]==ninja: cmd=[ninja]
  p=subprocess.run(cmd,cwd=repo,text=True,capture_output=True,check=False); out.append("$ "+" ".join(cmd)+"\n"+p.stdout); err.append(p.stderr)
  if p.returncode: return BuildReport(cmd,p.returncode,time.monotonic()-start,"\n".join(out),"\n".join(err),None)
 dol=repo/"build/GALE01/main.dol"; sha=_sha1(dol) if dol.is_file() else None
 if sha != cfg.get("expected_main_dol_sha1"): raise PascalPatchError(f"generated main.dol hash mismatch: {sha}")
 return BuildReport([ninja],0,time.monotonic()-start,"\n".join(out),"\n".join(err),sha)
