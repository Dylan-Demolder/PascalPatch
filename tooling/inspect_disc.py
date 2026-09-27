#!/usr/bin/env python3
import argparse, json, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"host/src"))
from pascalpatch.discovery import inspect_game
from pascalpatch.errors import PascalPatchError
p=argparse.ArgumentParser(); p.add_argument("path"); a=p.parse_args()
try:
 x=inspect_game(a.path); print(json.dumps({"path":str(x.path),"kind":x.kind,"game_id":x.game_id,"revision":x.revision,"main_dol_sha1":x.main_dol_sha1},indent=2))
except PascalPatchError as e: print(f"error: {e}",file=sys.stderr); raise SystemExit(2)
