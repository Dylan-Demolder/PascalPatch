#!/usr/bin/env python3
"""Extract a user-owned GALE01 ISO with doldecomp-toolkit; never edits the ISO."""
import argparse, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"host/src"))
from meleemod.discovery import inspect_iso
a=argparse.ArgumentParser(); a.add_argument("iso"); a.add_argument("output"); a.add_argument("--dtk",default="dtk"); x=a.parse_args()
inspect_iso(x.iso); out=Path(x.output).expanduser(); out.parent.mkdir(parents=True,exist_ok=True)
cmd=[x.dtk,"disc","extract",x.iso,str(out)]
print("running:"," ".join(map(str,cmd))); raise SystemExit(subprocess.call(cmd))
