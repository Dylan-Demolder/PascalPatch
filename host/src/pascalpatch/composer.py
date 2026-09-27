from __future__ import annotations
from pathlib import Path
import shutil
from .errors import CompositionError, ValidationError

def safe_rel(value):
 p=Path(value)
 if p.is_absolute() or ".." in p.parts or "\\" in value or "\x00" in value: raise CompositionError(f"unsafe target path: {value!r}")
 return p

def compose_assets(base_dir, mods, output_dir, allow_priority=False):
 base=Path(base_dir); out=Path(output_dir)
 if not base.is_dir(): raise CompositionError(f"asset composition requires extracted game directory: {base}")
 shutil.copytree(base,out,dirs_exist_ok=True)
 owners={}
 report={"mods":[],"replacements":[],"conflicts":[]}
 for mod in sorted(mods,key=lambda m:m.get("priority",0)):
  src=Path(mod["source"]).expanduser()
  if not src.is_dir(): raise CompositionError(f"mod source directory does not exist: {src}")
  entry={"id":mod["id"],"version":mod.get("version"),"targets":[]}
  for raw in mod["targets"]:
   rel=safe_rel(raw); source=src/rel; target=out/rel
   if not source.is_file(): raise CompositionError(f"mod {mod['id']} target is missing: {rel}")
   key=str(rel)
   if key in owners:
    previous=owners[key]
    if not allow_priority or previous[1]==mod.get("priority",0):
     report["conflicts"].append({"target":str(rel),"mods":[previous[0],mod["id"]]})
     continue
   target.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(source,target); owners[key]=(mod["id"],mod.get("priority",0)); entry["targets"].append(key)
  report["mods"].append(entry)
 if report["conflicts"]: raise CompositionError("asset target conflicts",[ValidationError("mods","target_conflict",str(x)) for x in report["conflicts"]])
 report["replacements"]=[{"target":k,"mod":v[0]} for k,v in owners.items()]
 return report
