#!/usr/bin/env python3
"""Build a validated profile without invoking a shell or touching source game data."""
import argparse, json, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"host/src"))
from meleemod.profile import load_profile
from meleemod.store import BuildStore
from meleemod.errors import MeleeModError
a=argparse.ArgumentParser(); a.add_argument("profile"); a.add_argument("--catalog-root"); a.add_argument("--data"); x=a.parse_args()
try:
 p=load_profile(x.profile,x.catalog_root); r=BuildStore(x.data).build(p); print(json.dumps({"profile":p.data["id"],"output":str(r.output),"metadata":str(r.metadata),"compatibility":r.compatibility},indent=2))
except MeleeModError as e:
 print(str(e),file=sys.stderr); [print(f"{x.path}: {x.code}: {x.message}",file=sys.stderr) for x in e.errors]; raise SystemExit(2)
