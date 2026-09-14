from __future__ import annotations
import hashlib, json, os, stat, zipfile, shutil, tempfile
from pathlib import Path
from .errors import ManifestError, ValidationError
from .manifest import GAME_VERSION
FORBIDDEN={".exe",".dll",".so",".dylib",".sh",".bat",".cmd",".elf"}
def _safe(name):
 p=Path(name); drive_prefix=len(name) >= 2 and name[1] == ":"
 return not p.is_absolute() and not drive_prefix and "\\" not in name and ".." not in p.parts and "\x00" not in name and p.suffix.lower() not in FORBIDDEN
def _read(path,name):
 if path.is_dir(): return (path/name).read_bytes()
 with zipfile.ZipFile(path) as z: return z.read(name)
def _names(path):
 if path.is_dir():
  out=[]
  for x in path.rglob("*"):
   if x.is_symlink(): raise ManifestError("unsafe character package",[ValidationError(str(x),"symlink","symlinks are forbidden")])
   if x.is_file(): out.append(str(x.relative_to(path)))
  return out
 with zipfile.ZipFile(path) as z:
  names=z.namelist()
  if len(names)!=len(set(names)): raise ManifestError("unsafe character package",[ValidationError("archive","duplicate_path","duplicate archive members are forbidden")])
  return names
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


def installation_plan(path, profile_mode):
    """Validate a package and return a safe install decision.

    Asset/code conversion is deliberately not guessed here. Gameplay packages
    are accepted for planning only in Offline profiles until the runtime
    composition backend exists.
    """
    info=validate_package(path)
    if info["compatibility"] == "offline-gameplay" and profile_mode != "offline":
        raise ManifestError("gameplay character packages require an Offline profile",[ValidationError("character.compatibility","offline_required","select mode=offline")])
    if info["compatibility"] == "unknown":
        raise ManifestError("unknown character compatibility",[ValidationError("character.compatibility","unknown_capability","package must declare a supported compatibility")])
    return {"status":"validated-only","package":str(Path(path).resolve()),"profile_mode":profile_mode,"composition":"deferred-until-character-runtime-integration","metadata":info}


def compose_validated_package(path, destination, profile_mode):
    """Stage a validated package outside the game filesystem.

    This deliberately performs no Melee asset conversion. It is useful for an
    Offline authoring workspace while keeping the playable-character boundary
    explicit and fail-closed.
    """
    info=installation_plan(path,profile_mode)
    source=Path(path).resolve(); destination=Path(destination).expanduser().resolve()
    if destination.exists(): raise ManifestError("character staging destination exists",[ValidationError(str(destination),"exists","refusing to merge into an existing destination")])
    names=_names(source); destination.parent.mkdir(parents=True,exist_ok=True); stage=Path(tempfile.mkdtemp(prefix=destination.name+"-",dir=destination.parent))
    try:
        target=stage/info["metadata"]["id"]; target.mkdir(parents=True)
        for name in names:
            if name=="checksums.json": continue
            data=_read(source,name); out=target/name; out.parent.mkdir(parents=True,exist_ok=True); out.write_bytes(data)
        (target/"staging.json").write_text(json.dumps({"status":"validated-only","profile_mode":profile_mode,"source_package":str(source),"game_integration":False},indent=2,sort_keys=True)+"\n",encoding="utf-8")
        os.replace(target,destination)
    finally: shutil.rmtree(stage,ignore_errors=True)
    return destination
