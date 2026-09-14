#!/usr/bin/env python3
import argparse
from pathlib import Path
from meleemod.gui import run
parser=argparse.ArgumentParser(description="MeleeMod GUI")
parser.add_argument("--root",default="."); parser.add_argument("--data",default=None)
parser.add_argument("--dolphin",default=None,help="explicit Dolphin executable")
parser.add_argument("--self-test",action="store_true",help="run Validate and Build through Tk callbacks, then exit")
parser.add_argument("--self-test-launch",action="store_true",help="also run Launch through Tk callbacks using the discovered emulator")
a=parser.parse_args()
try: run(Path(a.root),a.data,dolphin=a.dolphin,self_test=a.self_test or a.self_test_launch,self_test_launch=a.self_test_launch)
except ImportError as exc:
    parser.error(f"Tk GUI dependencies are unavailable: {exc}")
