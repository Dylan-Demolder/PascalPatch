from __future__ import annotations
import hashlib, json, os, stat, zipfile
from pathlib import Path
from .errors import ManifestError, ValidationError
from .manifest import GAME_VERSION
FORBIDDEN={".exe",".dll",".so",".dylib",".sh",".bat",".cmd",".elf"}
def _safe(name):
 p=Path(name); return not p.is_absolute() and ".." not in p.parts and "\x00" not in name and p.suffix.lower() not in FORBIDDEN
def _read(path,name):
 if path.is_dir(): return (path/name).read_bytes()
 with zipfile.ZipFile(path) as z: return z.read(name)
def _names(path):
 if path.is_dir(): return [str(x.relative_to(path)) for x in path.rglob("*") if x.is_file()]
 with zipfile.ZipFile(path) as z: return z.namelist()
def validate_package(path):
 p=Path(path).resolve(); names=_names(p)
 bad=[n for n in names if not _safe(n)]
 if bad: raise ManifestError("unsafe character package",[ValidationError(n,"unsafe_path","path or executable payload is forbidden") for n in bad])
 required={"character.json","moveset.json","checksums.json"}; missing=required-set(names)
 if missing: raise ManifestError("incomplete character package",[ValidationError(x,"required","missing package file") for x in sorted(missing)])
 try: character=json.loads(_read(p,"character.json")); moves=json.loads(_read(p,"moveset.json")); checks=json.loads(_read(p,"checksums.json"))
 except (json.JSONDecodeError,KeyError) as e: raise ManifestError(f"invalid character package JSON: {e}")
 errors=[]
 for key in ("id","display_name","version","author","license","target_game_version","compatibility"):
  if key not in character: errors.append(ValidationError("character."+key,"required","missing field"))
 if character.get("target_game_version") != GAME_VERSION: errors.append(ValidationError("character.target_game_version","game_version","must target GALE01-1.02"))
 if character.get("compatibility") not in {"visual-only","offline-gameplay","training-compatible","unknown"}: errors.append(ValidationError("character.compatibility","compatibility","invalid classification"))
 if not isinstance(moves,dict): errors.append(ValidationError("moveset","type","must be object"))
 if not isinstance(checks,dict): errors.append(ValidationError("checksums","type","must map paths to SHA-256"))
 if errors: raise ManifestError("invalid character package",errors)
 # Checksums are over package member bytes; no self-checksum recursion is allowed.
 for name,expected in checks.items():
  if name=="checksums.json" or name not in names: raise ManifestError("invalid character checksum manifest",[ValidationError(name,"reference","checksum references missing or reserved path")])
  actual=hashlib.sha256(_read(p,name)).hexdigest()
  if actual!=expected: raise ManifestError("character asset checksum mismatch",[ValidationError(name,"checksum",f"expected {expected}, got {actual}")])
 return {"id":character["id"],"version":character["version"],"compatibility":character["compatibility"],"target_game_version":character["target_game_version"],"files":len(names)}
