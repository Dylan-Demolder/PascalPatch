from __future__ import annotations
import hashlib, json, os, shutil, tempfile, stat, zipfile
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
def _reject_symlinks(path):
 p=Path(path)
 if p.is_symlink(): raise ManifestError("registry packages cannot contain symlinks")
 if p.is_dir():
  for child in p.rglob("*"):
   if child.is_symlink(): raise ManifestError("registry packages cannot contain symlinks")

def _hash_path(path):
 p=Path(path); _reject_symlinks(p); h=hashlib.sha256()
 if p.is_file():
  with p.open("rb") as f:
   for b in iter(lambda:f.read(8*1024*1024),b""): h.update(b)
 else:
  for f in sorted(x for x in p.rglob("*") if x.is_file()):
   h.update(f.relative_to(p).as_posix().encode()+b"\0"); h.update(hashlib.sha256(f.read_bytes()).digest())
 return h.hexdigest()
def verify_file(path,expected):
 actual=_hash_path(path)
 if actual!=expected: raise ManifestError("registry package hash mismatch",[ValidationError("sha256","mismatch",actual)])
 return True


def install_local(entry, destination):
    errors=validate_entry(entry)
    if errors: raise ManifestError("invalid registry entry",errors)
    source=Path(entry["source"]).expanduser()
    _reject_symlinks(source)
    source=source.resolve()
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


def install_remote_archive(entry, destination, timeout=5.0, max_bytes=64 * 1024 * 1024, max_extracted_bytes=256 * 1024 * 1024, opener=None):
    """Verify an HTTPS ZIP package, validate every member, then extract atomically."""
    with tempfile.TemporaryDirectory(prefix="meleemod-registry-archive-") as temporary:
        downloaded=install_remote(entry,temporary,timeout=timeout,max_bytes=max_bytes,opener=opener)
        archive=downloaded/"package"; root=Path(destination).expanduser().resolve()
        try: zf=zipfile.ZipFile(archive)
        except zipfile.BadZipFile as exc: raise ManifestError("remote registry package is not a ZIP archive") from exc
        with zf:
            infos=zf.infolist(); names=[info.filename for info in infos]
            if len(names)!=len(set(names)): raise ManifestError("unsafe remote registry archive",[ValidationError("archive","duplicate_path","duplicate members are forbidden")])
            total=0; safe=[]
            for info in infos:
                name=info.filename; path=Path(name)
                mode=(info.external_attr >> 16) & 0xffff
                if path.is_absolute() or (len(name) >= 2 and name[1] == ":") or "\\" in name or ".." in path.parts or "\x00" in name or stat.S_ISLNK(mode) or (path.suffix.lower() in {".exe",".dll",".so",".dylib",".sh",".bat",".cmd",".elf"}):
                    raise ManifestError("unsafe remote registry archive",[ValidationError(name,"unsafe_member","traversal, symlink, or executable member")])
                if info.is_dir(): continue
                total+=info.file_size
                if total>max_extracted_bytes: raise ManifestError("remote registry archive exceeds extraction limit")
                safe.append(info)
            target=root/entry["id"]/entry["version"]; staging=target.parent/("."+target.name+".staging")
            shutil.rmtree(staging,ignore_errors=True); staging.mkdir(parents=True)
            try:
                for info in safe:
                    out=(staging/info.filename).resolve()
                    if staging not in out.parents: raise ManifestError("unsafe remote registry archive",[ValidationError(info.filename,"traversal","member escapes staging")])
                    out.parent.mkdir(parents=True,exist_ok=True)
                    with zf.open(info) as source, out.open("wb") as dest: shutil.copyfileobj(source,dest,1024*1024)
                target.parent.mkdir(parents=True,exist_ok=True); shutil.rmtree(target,ignore_errors=True); staging.rename(target)
            except Exception:
                shutil.rmtree(staging,ignore_errors=True); raise
            return target
