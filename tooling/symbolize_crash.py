#!/usr/bin/env python3
import argparse, json
from pascalpatch.diagnostics import symbolize_native
p=argparse.ArgumentParser(description="Symbolize a PascalPatch native crash address list")
p.add_argument("elf"); p.add_argument("addresses",nargs="+"); p.add_argument("--tool")
a=p.parse_args(); print(json.dumps(symbolize_native(a.elf,a.addresses,a.tool),indent=2))
