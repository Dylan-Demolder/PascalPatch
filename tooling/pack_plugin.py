"""Package a plugin you built into the ZIP PascalPatch installs.

    python tooling/pack_plugin.py sdk/template build/Release/my-plugin.dll

Checks plugin.json with the same rules as the PascalPatch app, checks that the DLL is a 64-bit
Windows DLL named <id>.dll that exports pp_plugin_load, and writes <id>-<version>.zip (plugin.json,
the DLL, README.md, and LICENSE when there is one) next to the plugin folder, or into --out.
In the app: Plugins > Install from file. The same ZIP layout is what the plugin site serves.
--install does that step too: it installs the ZIP into the PascalPatch data folder (the one the app
uses, or --data), so the next Play loads the new build.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "host" / "src"))
from pascalpatch.plugin_store import validate_manifest  # noqa: E402

FIXED_TIME = (2020, 1, 1, 0, 0, 0)   # the same inputs always give the same bytes


def check_dll(dll: Path) -> list[str]:
    """Problems with the DLL: not PE, not x64, not a DLL, or no pp_plugin_load export."""
    data = dll.read_bytes()
    if data[:2] != b"MZ":
        return [f"{dll.name} is not a Windows DLL"]
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        return [f"{dll.name} is not a Windows DLL"]
    machine, sections = struct.unpack_from("<HH", data, pe + 4)
    characteristics = struct.unpack_from("<H", data, pe + 22)[0]
    problems = []
    if machine != 0x8664:
        problems.append(f"{dll.name} is not 64-bit (build with -A x64)")
    if not characteristics & 0x2000:
        problems.append(f"{dll.name} is an executable, not a DLL")
    if b"pp_plugin_load\0" not in data:
        problems.append(f'{dll.name} does not export pp_plugin_load (extern "C" __declspec(dllexport))')
    return problems


def pack(folder: Path, dll: Path, out: Path) -> Path:
    manifest = json.loads((folder / "plugin.json").read_text(encoding="utf-8"))
    errors = [f"{e.path}: {e.message}" for e in validate_manifest(manifest)]
    if dll.name != manifest.get("entry"):
        errors.append(f"the DLL is {dll.name}, but plugin.json's entry is {manifest.get('entry')}")
    errors += check_dll(dll)
    if not (folder / "README.md").is_file():
        errors.append("README.md is missing (the plugin site and the app show it)")
    if errors:
        raise SystemExit("cannot pack:\n  " + "\n  ".join(errors))
    out.mkdir(parents=True, exist_ok=True)
    target = out / f"{manifest['id']}-{manifest['version']}.zip"
    files = [(folder / "plugin.json", "plugin.json"), (dll, dll.name), (folder / "README.md", "README.md")]
    for name in ("LICENSE", "LICENSE.txt"):
        if (folder / name).is_file():
            files.append((folder / name, name))
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
        for src, name in files:
            info = zipfile.ZipInfo(name, FIXED_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, src.read_bytes())
    return target


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", type=Path, help="the plugin folder (plugin.json, README.md)")
    ap.add_argument("dll", type=Path, help="the built <id>.dll")
    ap.add_argument("--out", type=Path, help="where to write the ZIP (default: beside the plugin folder)")
    ap.add_argument("--install", action="store_true", help="also install it, as Plugins > Install from file does")
    ap.add_argument("--data", type=Path, help="the PascalPatch data folder for --install (default: the app's)")
    a = ap.parse_args(argv)
    target = pack(a.folder, a.dll, a.out or a.folder.resolve().parent)
    print(target)
    if a.install:
        from pascalpatch.plugin_store import PluginStore
        from pascalpatch.store import default_data_root
        installed = PluginStore(a.data or default_data_root()).install_file(target)
        print(f"installed into {installed}\nPlay (or relaunch the game) to load it. In game, F2 shows its tab.")
    else:
        print("In the PascalPatch app: Plugins > Install from file, then Play. In game, F2 shows its tab.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
