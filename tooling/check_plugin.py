"""Check a plugin's source folder the way the plugin site's review does, and optionally build it.

    python tooling/check_plugin.py path/to/my-plugin                  # checks only
    python tooling/check_plugin.py path/to/my-plugin --build --out dist
    python tooling/check_plugin.py community/ledge-trainer --community --build --out dist

Checks: plugin.json is valid; README.md and native/CMakeLists.txt are there; the folder holds
source, not binaries or game files; the source uses no networking. --community adds the plugin
site's rules: the folder is named after the plugin's id, and the licence is GPL-2.0-or-later.
--build configures and builds native/ with CMake (Visual Studio, x64) against this repository's
SDK, then packs the DLL with pack_plugin.py into --out. Exit status 1 when anything fails.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "host" / "src"))
sys.path.insert(0, str(REPO / "tooling"))
from pascalpatch.plugin_store import validate_manifest  # noqa: E402
from pack_plugin import pack  # noqa: E402

# Built files and anything that could be (or hold) game data never go in a source folder.
BINARY = {".dll", ".exe", ".lib", ".obj", ".o", ".a", ".pdb", ".exp", ".ilk", ".zip", ".7z", ".rar",
          ".iso", ".gcm", ".ciso", ".rvz", ".wbfs", ".dol", ".dat", ".usd", ".hsd", ".ssm", ".sem",
          ".thp", ".mth", ".hps", ".gct", ".sav", ".gci", ".raw"}
MAX_FILE = 2 * 1024 * 1024
SOURCE = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".txt", ".cmake"}
# Plugins are offline-only: no networking libraries or calls.
NETWORK = re.compile(r"winsock|ws2_32|winhttp|wininet|urlmon|URLDownloadTo|InternetOpen|curl[/.]|libcurl|"
                     r"\bWSAStartup\b|\bgetaddrinfo\b|\bWSASocket", re.IGNORECASE)
# Allowed, but a reviewer looks twice.
REVIEW = re.compile(r"\bCreateProcess\w*|\bShellExecute\w*|\bsystem\s*\(|\bWinExec\b|\bLoadLibrary\w*|"
                    r"\bVirtualProtect\b|\bWriteProcessMemory\b|\bRegSetValue\w*|\bDeleteFile\w*")
IGNORED_DIRS = {".git", "build", "out", ".vs", "dist"}


def files_of(folder: Path):
    for p in sorted(folder.rglob("*")):
        if p.is_file() and not (set(p.relative_to(folder).parts[:-1]) & IGNORED_DIRS):
            yield p


def check(folder: Path, community: bool) -> tuple[list[str], list[str], dict]:
    errors, notes = [], []
    mpath = folder / "plugin.json"
    if not mpath.is_file():
        return [f"{folder}: plugin.json is missing"], notes, {}
    try:
        manifest = json.loads(mpath.read_text(encoding="utf-8"))
    except ValueError as e:
        return [f"plugin.json is not valid JSON: {e}"], notes, {}
    errors += [f"{e.path}: {e.message}" for e in validate_manifest(manifest)]
    if not (folder / "README.md").is_file():
        errors.append("README.md is missing (the app and the plugin site show it)")
    if not (folder / "native" / "CMakeLists.txt").is_file():
        errors.append("native/CMakeLists.txt is missing (the maintainer builds the DLL from it)")
    if community:
        if folder.name != manifest.get("id"):
            errors.append(f"the folder is {folder.name}/, but the plugin's id is {manifest.get('id')!r}: "
                          "name the folder after the id")
        if manifest.get("license") != "GPL-2.0-or-later":
            errors.append('plugin.json: "license" must be "GPL-2.0-or-later" for the plugin site')
        if not manifest.get("author"):
            errors.append('plugin.json: add "author" (your name or handle)')
    for p in files_of(folder):
        rel = p.relative_to(folder).as_posix()
        if p.suffix.lower() in BINARY:
            errors.append(f"{rel}: built files and game files do not belong in the source "
                          "(the maintainer builds the DLL; game data is never shipped)")
            continue
        if p.stat().st_size > MAX_FILE:
            errors.append(f"{rel}: over 2 MB; keep source folders small")
            continue
        if p.suffix.lower() in SOURCE:
            text = p.read_text(encoding="utf-8", errors="replace")
            for n, line in enumerate(text.splitlines(), 1):
                if NETWORK.search(line):
                    errors.append(f"{rel}:{n}: networking is not allowed (plugins are offline-only): {line.strip()[:80]}")
                elif REVIEW.search(line):
                    notes.append(f"{rel}:{n}: for the reviewer: {line.strip()[:80]}")
    return errors, notes, manifest


def build(folder: Path, manifest: dict, out: Path) -> Path:
    bdir = folder / "build"
    sdk = (REPO / "sdk" / "include").as_posix()
    subprocess.run(["cmake", "-S", str(folder / "native"), "-B", str(bdir), "-A", "x64",
                    f"-DPASCALPATCH_SDK={sdk}"], check=True)
    subprocess.run(["cmake", "--build", str(bdir), "--config", "Release"], check=True)
    dll = bdir / "Release" / manifest["entry"]
    if not dll.is_file():
        raise SystemExit(f"the build made no {manifest['entry']}: the CMake target must be named {manifest['id']} "
                         '(add_library(<id> SHARED ...) with PREFIX "")')
    return pack(folder, dll, out)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folders", nargs="+", type=Path, help="plugin source folders (plugin.json, README.md, native/)")
    ap.add_argument("--community", action="store_true", help="also apply the plugin site's rules")
    ap.add_argument("--build", action="store_true", help="build and pack each plugin that passes")
    ap.add_argument("--out", type=Path, default=Path("dist"), help="where --build writes the ZIPs (default: dist)")
    a = ap.parse_args(argv)
    failed = False
    for folder in a.folders:
        folder = folder.resolve()
        errors, notes, manifest = check(folder, a.community)
        print(f"== {folder.name}")
        for n in notes:
            print(f"   note  {n}")
        for e in errors:
            print(f"   error {e}")
        if errors:
            failed = True
            continue
        if a.build:
            try:
                print(f"   built {build(folder, manifest, a.out)}")
            except (subprocess.CalledProcessError, SystemExit) as e:
                print(f"   error build failed: {e}")
                failed = True
                continue
        print("   ok")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
