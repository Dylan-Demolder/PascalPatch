from __future__ import annotations
import hashlib, json
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
def verify_file(path,expected):
 h=hashlib.sha256();
 with Path(path).open("rb") as f:
  for b in iter(lambda:f.read(8*1024*1024),b""): h.update(b)
 if h.hexdigest()!=expected: raise ManifestError("registry package hash mismatch",[ValidationError("sha256","mismatch",h.hexdigest())])
 return True
