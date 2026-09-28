"""Build the PascalPatch release download: unzip it and run pascalpatch.cmd, no compiler needed.

    python tooling/release/package.py --out dist

The zip holds this checkout's tracked files (the app, the SDK, the docs and the runtime and plugin
sources, so the GPL source travels with the binaries) minus the folders only the project itself
uses (DEV_ONLY), plus:

    bin/     pascalpatch-launch.exe, pascalpatch_runtime.dll and the built-in plugins
             (unlock-all, extra-fighters, move-graft, quick-match), built here with
             tooling/native/build_plugins.py, or taken from --bin
    python/  the official Windows embeddable Python from python.org, checked against its SHA-256
    studio/  Character Studio with its example characters, from a MeleeCharacterStudio checkout
             (--studio; by default the one beside this repository), so the app's Character Studio
             button works with nothing else installed

Every other plugin comes from Browse in the app, signed, as before. The build needs Visual Studio
2022 (or Build Tools) and CMake, unless --bin points at binaries built already.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PYTHON_VERSION = "3.13.15"
PYTHON_URL = f"https://www.python.org/ftp/python/{PYTHON_VERSION}/python-{PYTHON_VERSION}-embed-amd64.zip"
PYTHON_SHA256 = "d1f04d990aee1253d8569e8e5104e30fa9f5fa830899f14843448872d936a2cf"   # from python.org's release page
BUILT_IN = ("unlock-all", "extra-fighters", "move-graft", "quick-match")
# Kept out of the download so its folder shows players little more than pascalpatch.cmd; they
# stay in the repository, which the release's tag points at.
DEV_ONLY = (".github/", "design/", "spikes/", "website/", "tools/", ".gitignore", "backend.py", "package.json", "pyproject.toml")
STUDIO_DEV_ONLY = (".github/", ".gitignore")
BINARIES = ("pascalpatch-launch.exe", "pascalpatch_runtime.dll", *(f"{p}.dll" for p in BUILT_IN))


def version():
    """PascalPatch's version: the runtime's (runtime/native/pp_runtime.h), which the host shares."""
    text = (REPO / "runtime" / "native" / "pp_runtime.h").read_text(encoding="utf-8")
    ver = re.search(r'VERSION = "([^"]+)"', text).group(1)
    host = re.search(r'__version__ = "([^"]+)"', (REPO / "host/src/pascalpatch/__init__.py").read_text(encoding="utf-8")).group(1)
    if host != ver:
        raise SystemExit(f"the runtime is {ver} but the app says {host}: bump both together")
    return ver


def build_binaries(work):
    data = work / "data"
    args = [sys.executable, str(REPO / "tooling" / "native" / "build_plugins.py"), "--data", str(data), "--only", "runtime"]
    for pid in BUILT_IN:
        args += ["--only", pid]
    subprocess.run(args, check=True)
    return data / "native-plugins"


def python_embed(cache):
    zpath = Path(cache) / f"python-{PYTHON_VERSION}-embed-amd64.zip" if cache else None
    if zpath and zpath.is_file():
        blob = zpath.read_bytes()
    else:
        with urllib.request.urlopen(PYTHON_URL, timeout=120) as r:
            blob = r.read()
        if zpath:
            zpath.parent.mkdir(parents=True, exist_ok=True); zpath.write_bytes(blob)
    got = hashlib.sha256(blob).hexdigest()
    if got != PYTHON_SHA256:
        raise SystemExit(f"{PYTHON_URL}: SHA-256 {got} is not the published {PYTHON_SHA256}")
    return blob


def tracked_files(repo=REPO, skip=DEV_ONLY):
    out = subprocess.run(["git", "-C", str(repo), "ls-files", "-z"], check=True, capture_output=True).stdout
    return [f for f in out.decode("utf-8").split("\0")
            if f and (repo / f).is_file() and not f.startswith(skip)]


def studio_checkout(path):
    """The Character Studio checkout to bundle, with its version and commit."""
    studio = Path(path).resolve()
    if not (studio / "core" / "src" / "melee_character_studio").is_dir() or not (studio / "examples" / "roster.json").is_file():
        raise SystemExit(f"no Character Studio checkout with examples at {studio} (pass --studio)")
    ver = re.search(r'^version = "([^"]+)"', (studio / "pyproject.toml").read_text(encoding="utf-8"), re.M).group(1)
    commit = subprocess.run(["git", "-C", str(studio), "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
    return studio, ver, commit


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=str(REPO / "dist"), help="folder for the zip (default: dist/)")
    ap.add_argument("--bin", help="folder with the built binaries, instead of building them")
    ap.add_argument("--python-cache", help="folder to keep the downloaded embeddable Python in")
    ap.add_argument("--studio", default=str(REPO.parent / "MeleeCharacterStudio"), help="the MeleeCharacterStudio checkout to bundle")
    a = ap.parse_args(argv)
    ver = version()
    top = f"PascalPatch-{ver}"
    studio, studio_ver, studio_commit = studio_checkout(a.studio)
    out = Path(a.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    zpath = out / f"{top}-windows.zip"
    with tempfile.TemporaryDirectory() as tmp:
        binaries = Path(a.bin).resolve() if a.bin else build_binaries(Path(tmp))
        missing = [b for b in BINARIES if not (binaries / b).is_file()]
        if missing:
            raise SystemExit(f"not built: {', '.join(missing)} (in {binaries})")
        py = zipfile.ZipFile(io.BytesIO(python_embed(a.python_cache)))
        with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for f in tracked_files():
                z.write(REPO / f, f"{top}/{f}")
            for b in BINARIES:
                z.write(binaries / b, f"{top}/bin/{b}")
            for f in tracked_files(studio, STUDIO_DEV_ONLY):
                z.write(studio / f, f"{top}/studio/{f}")
            z.writestr(f"{top}/studio/VERSION.txt", f"Character Studio {studio_ver} ({studio_commit})\r\n")
            for info in py.infolist():
                data = py.read(info)
                if info.filename.endswith("._pth"):
                    # the embeddable Python ignores PYTHONPATH: its ._pth file is the whole import path
                    paths = b"..\\host\\src\r\n..\\studio\\core\\src\r\n"
                    data = data.replace(b".\r\n", b".\r\n" + paths, 1) if b".\r\n" in data else data + b"\r\n" + paths
                z.writestr(f"{top}/python/{info.filename}", data)
            z.writestr(f"{top}/python/README.txt",
                       f"Python {PYTHON_VERSION} (Windows embeddable package, python.org), so PascalPatch runs\r\n"
                       "without installing Python. Its license is LICENSE.txt in this folder.\r\n")
    digest = hashlib.sha256(zpath.read_bytes()).hexdigest()
    (out / f"{zpath.name}.sha256").write_bytes(f"{digest}  {zpath.name}\n".encode("ascii"))   # LF: sha256sum -c reads it
    print(zpath)
    print(f"sha256 {digest}  {zpath.stat().st_size / 1e6:.1f} MB  (Character Studio {studio_ver}, {studio_commit[:7]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
