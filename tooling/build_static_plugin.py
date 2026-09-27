#!/usr/bin/env python3
import argparse, json, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"host/src"))
from pascalpatch.static_integration import build_in_worktree
from pascalpatch.errors import PascalPatchError
a=argparse.ArgumentParser(); a.add_argument("--repo",required=True); a.add_argument("--orig",required=True); a.add_argument("--plugins",required=True,help="JSON file containing a plugin manifest array"); a.add_argument("--source-root",required=True); a.add_argument("--output",required=True); x=a.parse_args()
try:
 plugins=json.loads(Path(x.plugins).read_text()); r=build_in_worktree(x.repo,x.orig,plugins,x.output,source_root=x.source_root); print(json.dumps({"output":str(r.dol),"sha1":r.sha1},indent=2))
except (OSError,ValueError,PascalPatchError) as e: print(f"error: {e}",file=sys.stderr); raise SystemExit(2)
