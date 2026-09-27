"""Build PascalPatch's native runtime and plugins into <data>/native-plugins.

runtime/native builds pascalpatch-launch.exe and pascalpatch_runtime.dll (injected into
an unmodified melee_port.exe); each plugins/<id>/native/ folder is a CMake
project producing <id>.dll. The
builds need Visual Studio 2022 (or Build Tools) and CMake, and a folder with
nlohmann/json.hpp (melee-unlocked ships one under port/third_party).

    python tooling/native/build_plugins.py --data C:/Users/you/MeleeMods/data \
        --json-include C:/path/to/melee-unlocked/port/third_party
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--data", required=True, help="PascalPatch data root (builds go to <data>/native-plugins)")
    ap.add_argument("--json-include", required=True, help="folder containing nlohmann/json.hpp")
    ap.add_argument("--cmake", default=shutil.which("cmake") or r"C:\Program Files\CMake\bin\cmake.exe")
    ap.add_argument("--only", action="append", default=[], help="plugin id (repeatable)")
    a = ap.parse_args(argv)
    out = Path(a.data).expanduser().resolve() / "native-plugins"; out.mkdir(parents=True, exist_ok=True)
    work = Path(a.data).expanduser().resolve() / "staging" / "native-plugin-build"
    built = []
    projects = [REPO / "runtime/native/CMakeLists.txt", *sorted(REPO.glob("plugins/*/native/CMakeLists.txt"))]
    for src in projects:
        pid = src.parent.parent.name
        if a.only and pid not in a.only:
            continue
        build = work / pid
        subprocess.run([a.cmake, "-S", str(src.parent), "-B", str(build), "-A", "x64",
                        f"-DNLOHMANN_JSON_INCLUDE={a.json_include}"], check=True, stdout=subprocess.DEVNULL)
        subprocess.run([a.cmake, "--build", str(build), "--config", "Release"], check=True)
        outputs = ["pascalpatch-launch.exe", "pascalpatch_runtime.dll"] if pid == "runtime" else [f"{pid}.dll"]
        for name in outputs:
            shutil.copy2(build / "Release" / name, out / name); built.append(str(out / name))
    for path in built:
        print(path)
    return 0 if built else 1


if __name__ == "__main__":
    sys.exit(main())
