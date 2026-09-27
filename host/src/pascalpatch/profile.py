from __future__ import annotations
import json
from dataclasses import dataclass
from pathlib import Path
from .manifest import load_json, require_valid, validate_plugin, validate_mod
from .errors import ManifestError, ValidationError
from .safety import validate_safety

@dataclass(frozen=True)
class ResolvedProfile:
    data: dict
    plugins: tuple[dict,...]
    mods: tuple[dict,...]
    compatibility: str
    source: Path

def _catalog_entry(root, kind, ident):
 p=Path(root)/kind/ident/("plugin.json" if kind=="plugins" else "mod.json")
 if not p.is_file(): raise ManifestError(f"missing {kind[:-1]} manifest for {ident}: {p}")
 return load_json(p)

def load_profile(path, catalog_root=None):
 source=Path(path).resolve(); data=load_json(source); require_valid(data,"profile","profile")
 data=dict(data); data["base_game"]=str((source.parent / data["base_game"]).resolve()) if not Path(data["base_game"]).is_absolute() else data["base_game"]
 root=Path(catalog_root or source.parent).resolve()
 for k in ("decomp_repo","decomp_orig","plugin_source_root","runtime_root"):
  if k in data and not Path(data[k]).is_absolute(): data[k]=str((root / data[k]).resolve())
 chars=[]
 for c in data.get("characters",[]):
  c=dict(c)
  for k in ("package","fighter_file","costume_file","animation_file","move_graft","portrait","icon"):
   if k in c and not Path(c[k]).is_absolute(): c[k]=str((source.parent / c[k]).resolve())
  chars.append(c)
 if chars: data["characters"]=chars
 plugins=[]; mods=[]; errors=[]
 for i,ident in enumerate(data["plugins"]):
  try:
   x=_catalog_entry(root,"plugins",ident); require_valid(x,"plugin",f"plugins[{i}]"); plugins.append(x)
  except ManifestError as ex: errors.extend(ex.errors or [ValidationError(f"plugins[{i}]","invalid",str(ex))])
 for i,ident in enumerate(data["mods"]):
  try:
   x=_catalog_entry(root,"mods",ident); require_valid(x,"mod",f"mods[{i}]")
   x=dict(x); x["source"]=str((root / x["source"]).resolve()) if not Path(x["source"]).is_absolute() else x["source"]
   mods.append(x)
  except ManifestError as ex: errors.extend(ex.errors or [ValidationError(f"mods[{i}]","invalid",str(ex))])
 ids={p["id"] for p in plugins}
 for p in plugins:
  for dep in p.get("dependencies",[]):
   if dep not in ids: errors.append(ValidationError(f"plugins.{p['id']}.dependencies","missing_dependency",f"plugin dependency {dep!r} is not selected"))
 eff,safety_errors=validate_safety(data,plugins,mods); errors.extend(safety_errors)
 if chars:
  # Custom characters change simulation state and would desync Slippi
  # netplay, so they are offline-only regardless of package claims.
  if data.get("mode")!="offline": errors.append(ValidationError("profile.characters","offline_required","character packages require mode=offline"))
  if eff=="online-safe": eff="offline-only"
 if errors: raise ManifestError("profile validation failed",errors)
 return ResolvedProfile(data,tuple(plugins),tuple(mods),eff,source)
