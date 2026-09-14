from __future__ import annotations
import hashlib, json, os, shutil, tempfile
from urllib.parse import urlparse
from urllib.request import Request, urlopen
from pathlib import Path
from .errors import ManifestError, ValidationError
from .manifest import ID_RE
def validate_entry(x):
 required={"id","version","source","sha256","license","compatibility","dependencies","maintainer"}; e=[]
 if not isinstance(x,dict): return [ValidationError("registry","type","expected object")]
 for k in set(x)-required: e.append(ValidationError("registry."+k,"unknown_field","unknown field"))
 for k in required:
  if k not in x: e.append(ValidationError("registry."+k,"required","missing field"))
 if "id" in x and (not isinstance(x["id"],str) or not ID_RE.fullmatch(x["id"])): e.append(ValidationError("registry.id","id","invalid ID"))
 if "sha256" in x and (not isinstance(x["sha256"],str) or len(x["sha256"])!=64 or any(c not in "0123456789abcdef" for c in x["sha256"])): e.append(ValidationError("registry.sha256","hash","must be lowercase SHA-256"))
 if x.get("compatibility") not in {"online-safe","offline-only","unknown"}: e.append(ValidationError("registry.compatibility","compatibility","invalid compatibility"))
 return e
def _hash_path(path):
 p=Path(path); h=hashlib.sha256()
 if p.is_file():
  with p.open("rb") as f:
   for b in iter(lambda:f.read(8*1024*1024),b""): h.update(b)
 else:
  for f in sorted(x for x in p.rglob("*") if x.is_file() and not x.is_symlink()):
   h.update(f.relative_to(p).as_posix().encode()+b"\0"); h.update(hashlib.sha256(f.read_bytes()).digest())
 return h.hexdigest()
def verify_file(path,expected):
 actual=_hash_path(path)
 if actual!=expected: raise ManifestError("registry package hash mismatch",[ValidationError("sha256","mismatch",actual)])
 return True


def install_local(entry, destination):
    errors=validate_entry(entry)
    if errors: raise ManifestError("invalid registry entry",errors)
    source=Path(entry["source"]).expanduser().resolve()
    if not source.exists(): raise ManifestError("registry source does not exist")
    verify_file(source,entry["sha256"])
    target=Path(destination).resolve()/entry["id"]/entry["version"]; staging=target.parent/("."+target.name+".staging")
    import shutil
    shutil.rmtree(staging,ignore_errors=True); staging.mkdir(parents=True)
    if source.is_dir(): shutil.copytree(source,staging/"package",symlinks=False)
    else: shutil.copy2(source,staging/"package")
    target.parent.mkdir(parents=True,exist_ok=True); shutil.rmtree(target,ignore_errors=True); staging.rename(target)
    return target


def install_remote(entry, destination, timeout=5.0, max_bytes=64 * 1024 * 1024, opener=None):
    """Download one HTTPS registry file, verify it while streaming, and promote atomically."""
    errors=validate_entry(entry)
    if errors: raise ManifestError("invalid registry entry",errors)
    parsed=urlparse(entry["source"])
    if parsed.scheme != "https" or not parsed.netloc: raise ManifestError("remote registry sources require HTTPS")
    opener=opener or urlopen
    root=Path(destination).expanduser().resolve(); root.mkdir(parents=True,exist_ok=True)
    temporary=None
    try:
        request=Request(entry["source"],headers={"Accept":"application/octet-stream","User-Agent":"meleemod-registry/1"})
        with opener(request,timeout=timeout) as response:
            declared=response.headers.get("Content-Length")
            if declared is not None and int(declared)>max_bytes: raise ManifestError("remote registry package exceeds size limit")
            fd,temporary=tempfile.mkstemp(prefix=".download-",dir=root); digest=hashlib.sha256(); total=0
            with os.fdopen(fd,"wb") as output:
                while True:
                    chunk=response.read(min(1024*1024,max_bytes-total+1))
                    if not chunk: break
                    total+=len(chunk)
                    if total>max_bytes: raise ManifestError("remote registry package exceeds size limit")
                    digest.update(chunk); output.write(chunk)
                output.flush(); os.fsync(output.fileno())
        if digest.hexdigest()!=entry["sha256"]: raise ManifestError("registry package hash mismatch",[ValidationError("sha256","mismatch",digest.hexdigest())])
        target=root/entry["id"]/entry["version"]; staging=target.parent/("."+target.name+".staging")
        shutil.rmtree(staging,ignore_errors=True); staging.mkdir(parents=True)
        shutil.copyfile(temporary,staging/"package"); target.parent.mkdir(parents=True,exist_ok=True); shutil.rmtree(target,ignore_errors=True); staging.rename(target)
        return target
    finally:
        if temporary:
            try: os.unlink(temporary)
            except FileNotFoundError: pass
