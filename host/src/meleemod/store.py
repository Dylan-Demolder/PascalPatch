from __future__ import annotations
from pathlib import Path
from dataclasses import dataclass
import datetime, hashlib, json, os, shutil, tempfile, uuid
from .composer import compose_assets
from .discovery import inspect_game, find_dolphin
from .errors import CompositionError, ValidationError
from .profile import ResolvedProfile
from .static_integration import build_in_worktree

@dataclass(frozen=True)
class BuildResult:
    profile_id:str; output:Path; metadata:Path; compatibility:str

def default_data_root():
 return Path(os.environ.get("MELEEMOD_DATA",Path.home()/".local/share/meleemod")).expanduser()

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
  stamp=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")+"-"+uuid.uuid4().hex[:8]; stage=Path(tempfile.mkdtemp(prefix=pid+"-",dir=self.staging)); output=stage/"game"; report={"profile_id":pid,"profile_name":profile.data["name"],"game_version":profile.data["game_version"],"base_game":str(game),"base_kind":info.kind,"base_main_dol_sha1":info.main_dol_sha1,"plugins":[{k:p.get(k) for k in ("id","version","api_version")} for p in profile.plugins],"mods":[{k:m.get(k) for k in ("id","version","priority")} for m in profile.mods],"compatibility":profile.compatibility,"tool_version":"meleemod-host/0.1.0"}
  try:
   if info.kind=="directory":
    if profile.plugins:
     required=("decomp_repo","decomp_orig","plugin_source_root")
     missing=[k for k in required if not profile.data.get(k)]
     if missing: raise CompositionError("static plugin build requires explicit decomp_repo, decomp_orig and plugin_source_root",[ValidationError("profile."+k,"required", "missing static build path") for k in missing])
     generated=stage/"generated-main.dol"
     static=build_in_worktree(profile.data["decomp_repo"],profile.data["decomp_orig"],profile.plugins,generated,source_root=profile.data["plugin_source_root"])
     report["static_plugin_dol_sha1"]=static.sha1
    composition=compose_assets(game,profile.mods,output, bool(profile.data.get("allow_priority",False))) if profile.mods else (shutil.copytree(game,output),{"mods":[],"replacements":[],"conflicts":[]})[1]
    if profile.plugins:
     target=output/"sys/main.dol"
     if not target.is_file(): raise CompositionError("extracted game directory has no sys/main.dol for static plugin output")
     shutil.copy2(generated,target)
     report["plugin_composition"]="static-source-overlay"
    report["composition"]=composition; launch=output
   else:
    if profile.mods or profile.plugins: raise CompositionError("asset and static code mods require an extracted game directory; extract the ISO first with tooling/extract_disc.py")
    output.mkdir(); (output/"game.iso").symlink_to(game); report["composition"]={"mods":[],"replacements":[],"conflicts":[]}; launch=output/"game.iso"
   report["output_hash"]=_tree_hash(output) if output.is_dir() else hashlib.sha256(output.read_bytes()).hexdigest()
   (output.parent/"build.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
   final=target_root/stamp; os.replace(output.parent,final)
   current=target_root/"current"; tmp=target_root/(".current-"+stamp); tmp.symlink_to(final, target_is_directory=True); os.replace(tmp,current)
   return BuildResult(pid,final/"game/game.iso" if info.kind=="iso" else final/"game",final/"build.json",profile.compatibility)
  except Exception:
   shutil.rmtree(stage,ignore_errors=True); raise
