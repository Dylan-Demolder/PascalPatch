#!/usr/bin/env python3
import argparse
from pathlib import Path
from meleemod.gui import run
parser=argparse.ArgumentParser(description="MeleeMod GUI")
parser.add_argument("--root",default="."); parser.add_argument("--data",default=None)
a=parser.parse_args(); run(Path(a.root),a.data)
