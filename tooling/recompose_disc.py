#!/usr/bin/env python3
import argparse, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parents[1] / "host/src"))
from pascalpatch.recompose_iso import recompose_iso
p = argparse.ArgumentParser(description="Recompose a user-owned GameCube ISO with a replacement main.dol")
p.add_argument("base_iso")
p.add_argument("main_dol")
p.add_argument("output")
p.add_argument("--overlay", action="append", default=[],
               help="ISO file replacement ISO_PATH=HOST_FILE (repeatable)")
a = p.parse_args()
if not a.overlay:
    print(recompose_iso(a.base_iso, a.main_dol, a.output))
else:
    from pascalpatch.iso_files import recompose_iso_with_fighter
    overlays = {}
    for item in a.overlay:
        iso_path, _, host_file = item.partition("=")
        if not iso_path or not host_file:
            raise SystemExit(f"invalid --overlay {item!r}: expected ISO_PATH=HOST_FILE")
        overlays[iso_path] = host_file
    print(recompose_iso_with_fighter(a.base_iso, a.main_dol, a.output, overlays))
