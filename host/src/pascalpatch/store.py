from __future__ import annotations
from pathlib import Path
from dataclasses import dataclass
import datetime, hashlib, json, os, shutil, tempfile, uuid
from .composer import compose_assets
from .discovery import inspect_game, find_dolphin
from .errors import CompositionError, ValidationError
from .profile import ResolvedProfile
from .static_integration import build_in_worktree
from .recompose_iso import recompose_iso
from .iso_files import overlay_iso_files
from .native import assign_new_fighters, stage_native_plugins, validate_character_entry

@dataclass(frozen=True)
class BuildResult:
    profile_id:str; output:Path; metadata:Path; compatibility:str

def default_data_root():
 env=os.environ.get("PASCALPATCH_DATA") or os.environ.get("MELEEMOD_DATA")
 if env: return Path(env).expanduser()
 new,old=Path.home()/".local/share/pascalpatch",Path.home()/".local/share/meleemod"
 return old if old.is_dir() and not new.exists() else new   # keep a pre-rename data folder working

def _tree_hash(p):
 h=hashlib.sha256()
 for f in sorted(x for x in p.rglob("*") if x.is_file()):
  h.update(str(f.relative_to(p)).encode()); h.update(f.read_bytes())
 return h.hexdigest()

class BuildStore:
 def __init__(self,root=None):
  self.root=Path(root or default_data_root()); self.builds=self.root/"builds"; self.staging=self.root/"staging"; self.builds.mkdir(parents=True,exist_ok=True); self.staging.mkdir(parents=True,exist_ok=True)
 def build(self,profile:ResolvedProfile):
  game=Path(profile.data["base_game"]).expanduser().resolve(); info=inspect_game(game)
  pid=profile.data["id"]; target_root=self.builds/pid; target_root.mkdir(parents=True,exist_ok=True)
  stamp=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")+"-"+uuid.uuid4().hex[:8]; stage=Path(tempfile.mkdtemp(prefix=pid+"-",dir=self.staging)); output=stage/"game"; report={"profile_id":pid,"profile_name":profile.data["name"],"game_version":profile.data["game_version"],"base_game":str(game),"base_kind":info.kind,"base_main_dol_sha1":info.main_dol_sha1,"plugins":[{k:p.get(k) for k in ("id","version","api_version")} for p in profile.plugins],"mods":[{k:m.get(k) for k in ("id","version","priority")} for m in profile.mods],"compatibility":profile.compatibility,"tool_version":"pascalpatch-host/0.1.0"}
  try:
   characters=assign_new_fighters([validate_character_entry(c,profile.data["mode"],i) for i,c in enumerate(profile.data.get("characters",[]))])
   if characters:
    report["characters"]=characters
    if info.kind!="iso" or profile.plugins: raise CompositionError("character slot overlays currently require an ISO base and no static plugins (native Tier B)")
   if info.kind=="directory":
    if profile.plugins:
     required=("decomp_repo","decomp_orig","plugin_source_root")
     missing=[k for k in required if not profile.data.get(k)]
     needs_runtime=any(p.get("static_signature")=="context" for p in profile.plugins)
     if needs_runtime and not profile.data.get("runtime_root"): missing.append("runtime_root")
     if missing: raise CompositionError("static plugin build requires explicit decomp/runtime/source paths",[ValidationError("profile."+k,"required", "missing static build path") for k in missing])
     generated=stage/"generated-main.dol"
     static=build_in_worktree(profile.data["decomp_repo"],profile.data["decomp_orig"],profile.plugins,generated,source_root=profile.data["plugin_source_root"],runtime_root=profile.data.get("runtime_root"))
     report["static_plugin_dol_sha1"]=static.sha1
    composition=compose_assets(game,profile.mods,output, bool(profile.data.get("allow_priority",False))) if profile.mods else (shutil.copytree(game,output),{"mods":[],"replacements":[],"conflicts":[]})[1]
    if profile.plugins:
     target=output/"sys/main.dol"
     if not target.is_file(): raise CompositionError("extracted game directory has no sys/main.dol for static plugin output")
     shutil.copy2(generated,target)
     report["plugin_composition"]="static-source-overlay"
    report["composition"]=composition; launch=output
   else:
    if profile.mods: raise CompositionError("filesystem asset mods require an extracted game directory; extract the ISO first with tooling/extract_disc.py")
    output.mkdir()
    if profile.plugins:
     required=("decomp_repo","decomp_orig","plugin_source_root")
     missing=[k for k in required if not profile.data.get(k)]
     needs_runtime=any(p.get("static_signature")=="context" for p in profile.plugins)
     if needs_runtime and not profile.data.get("runtime_root"): missing.append("runtime_root")
     if missing: raise CompositionError("static plugin ISO build requires explicit decomp/runtime/source paths",[ValidationError("profile."+k,"required","missing static build path") for k in missing])
     generated=stage/"generated-main.dol"
     static=build_in_worktree(profile.data["decomp_repo"],profile.data["decomp_orig"],profile.plugins,generated,source_root=profile.data["plugin_source_root"],runtime_root=profile.data.get("runtime_root"))
     recompose_iso(game,generated,output/"game.iso")
     report["static_plugin_dol_sha1"]=static.sha1; report["plugin_composition"]="static-source-overlay+iso-recomposition"
    elif characters:
     # Tier B: data files replaced, DOL copied verbatim so the native
     # recompiler's vanilla-DOL invariant still holds.
     overlay_iso_files(game,{k:v for c in characters for k,v in c["overlays"].items()},output/"game.iso",
                       {k:v for c in characters for k,v in c.get("additions",{}).items()})
     report["plugin_composition"]="native-tier-b-data-overlay"
     native=stage_native_plugins(characters,self.root,stage/"mods",unlock_all=profile.data.get("unlock_all_characters",True))
     if native: report["native_plugins"]=native
    else:
     (output/"game.iso").symlink_to(game)
    report["composition"]={"mods":[],"replacements":[],"conflicts":[]}; launch=output/"game.iso"
   report["output_hash"]=_tree_hash(output) if output.is_dir() else hashlib.sha256(output.read_bytes()).hexdigest()
   (output.parent/"build.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
   final=target_root/stamp; os.replace(output.parent,final)
   current=target_root/"current"; tmp=target_root/(".current-"+stamp); tmp.symlink_to(final, target_is_directory=True)
   try: os.replace(tmp,current)
   except PermissionError:
    # Windows cannot rename over an existing directory symlink; the
    # previous build stays on disk, only the pointer is briefly absent.
    if os.name!="nt" or not current.is_symlink(): raise
    current.unlink(); os.replace(tmp,current)
   return BuildResult(pid,final/"game/game.iso" if info.kind=="iso" else final/"game",final/"build.json",profile.compatibility)
  except Exception:
   shutil.rmtree(stage,ignore_errors=True); raise
