#!/usr/bin/env python3
"""Create a local, untracked profile for the cataloged demo-mod."""
from __future__ import annotations
import argparse, json
from pathlib import Path

def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("iso", type=Path)
    ap.add_argument("decomp", type=Path, help="Melee decompilation checkout")
    ap.add_argument("--output", type=Path, default=Path("profiles/demo-runtime.json"))
    ap.add_argument("--id", default="demo-runtime")
    a=ap.parse_args()
    iso=a.iso.expanduser().resolve(); decomp=a.decomp.expanduser().resolve()
    if not iso.is_file(): ap.error(f"ISO does not exist: {iso}")
    if not (decomp/"configure.py").is_file(): ap.error(f"not a Melee decompilation checkout: {decomp}")
    out=a.output.expanduser()
    data={"id":a.id,"name":"PascalPatch demo runtime","game_version":"GALE01-1.02","base_game":str(iso),"plugins":["demo-mod"],"mods":[],"mode":"offline","online_safe":False,"decomp_repo":str(decomp),"decomp_orig":str(decomp/"orig/GALE01"),"plugin_source_root":str(Path(__file__).resolve().parents[1]),"runtime_root":str(Path(__file__).resolve().parents[1])}
    out.parent.mkdir(parents=True,exist_ok=True); out.write_text(json.dumps(data,indent=2)+"\n"); print(out); return 0
if __name__=="__main__": raise SystemExit(main())
