#!/usr/bin/env python3
"""Open PascalPatch. This used to be the Tk launcher; it now opens the desktop app
(`pascalpatch app`). The Tk launcher is still here with --tk, for the legacy Dolphin path."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description="PascalPatch")
parser.add_argument("--root", default="."); parser.add_argument("--data", default=None)
parser.add_argument("--tk", action="store_true", help="the old Tk launcher (Dolphin profiles) instead of the app")
parser.add_argument("--dolphin", default=None, help="Tk launcher: explicit Dolphin executable")
parser.add_argument("--self-test", action="store_true", help="Tk launcher: run Validate and Build through Tk callbacks, then exit")
parser.add_argument("--self-test-launch", action="store_true", help="Tk launcher: also run Launch through Tk callbacks")
a = parser.parse_args()
if a.tk or a.self_test or a.self_test_launch:
    from pascalpatch.gui import run
    try:
        run(Path(a.root), a.data, dolphin=a.dolphin, self_test=a.self_test or a.self_test_launch, self_test_launch=a.self_test_launch)
    except ImportError as exc:
        parser.error(f"Tk GUI dependencies are unavailable: {exc}")
else:
    from pascalpatch.cli import main
    raise SystemExit(main(["--root", a.root] + (["--data", a.data] if a.data else []) + ["app"]))
