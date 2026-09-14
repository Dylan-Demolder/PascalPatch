from __future__ import annotations
import json, os, re
from pathlib import Path
def redact(value):
 text=str(value); home=str(Path.home())
 text=text.replace(home,"<HOME>")
 text=re.sub(r"(?i)([A-Za-z]:)?[/\\][^\s]+\.(?:iso|dol)","<GAME_DATA>",text)
 return text
def report(profile_id,game_hash,plugins,runtime_api,error=None):
 return {"profile_id":redact(profile_id),"game_hash":redact(game_hash),"plugins":[{"id":redact(p.get("id")),"version":redact(p.get("version"))} for p in plugins],"runtime_api":runtime_api,"error":redact(error) if error else None}
def write_report(path,**kwargs):
 p=Path(path); p.parent.mkdir(parents=True,exist_ok=True); p.write_text(json.dumps(report(**kwargs),indent=2,sort_keys=True)+"\n"); return p
