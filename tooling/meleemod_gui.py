#!/usr/bin/env python3
import argparse
from pathlib import Path
from meleemod.gui import run
parser=argparse.ArgumentParser(description="MeleeMod GUI")
parser.add_argument("--root",default="."); parser.add_argument("--data",default=None)
parser.add_argument("--self-test",action="store_true",help="run Validate and Build through Tk callbacks, then exit")
a=parser.parse_args()
try: run(Path(a.root),a.data,self_test=a.self_test)
except ImportError as exc:
    parser.error(f"Tk GUI dependencies are unavailable: {exc}")
