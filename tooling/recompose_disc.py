#!/usr/bin/env python3
import argparse
from recompose_iso import recompose_iso
p=argparse.ArgumentParser(description="Recompose a user-owned GameCube ISO with a replacement main.dol")
p.add_argument("base_iso"); p.add_argument("main_dol"); p.add_argument("output")
a=p.parse_args(); print(recompose_iso(a.base_iso,a.main_dol,a.output))
