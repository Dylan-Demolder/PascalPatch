#!/usr/bin/env python3
"""Build the PAS-23 modified ISO on the runner from the runner's own clean ISO.

Reads the runner-local clean GALE01 ISO, extracts its main.dol (pure python,
no toolkit required), appends a synthetic PAS-23 marker trailer (game code
sections are left untouched), and recomposes a bootable modified ISO via
``pascalpatch.recompose_iso``. No Nintendo data is committed or transferred; the
only repo inputs are the synthetic files under ``tooling/fixtures/pas23/``.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "host/src"))
from pascalpatch.recompose_iso import recompose_iso

GAME_ID = b"GALE01"
MARKER = b"PAS23-SYNTHETIC-FIXTURE-MARKER-v1:"


def _dol_size(f):
    h = f.read(0x100)
    sections = []
    for i in range(7):
        sections.append((int.from_bytes(h[i * 4:i * 4 + 4], "big"),
                         int.from_bytes(h[0x90 + i * 4:0x94 + i * 4], "big")))
    for i in range(11):
        sections.append((int.from_bytes(h[0x1C + i * 4:0x20 + i * 4], "big"),
                         int.from_bytes(h[0xAC + i * 4:0xB0 + i * 4], "big")))
    vals = [o + s for o, s in sections if o and s]
    if not vals:
        raise ValueError("main.dol has no valid sections")
    return max(vals)


def main(argv=None):
    a = argparse.ArgumentParser(description="Build PAS-23 modified ISO on the runner")
    a.add_argument("--clean", required=True, help="Runner-local clean GALE01 Rev.02 ISO")
    a.add_argument("--fixture", required=True, help="Checked-out synthetic fixture dir")
    a.add_argument("--output", required=True, help="Where to write the modified ISO")
    x = a.parse_args(argv)
    clean = Path(x.clean).expanduser().resolve()
    fixture = Path(x.fixture).expanduser().resolve()
    out = Path(x.output).expanduser().resolve()
    archive = fixture / "leesin-hsd-2e.melee-character"
    data = fixture / "leesin-patched.hsd"
    if not clean.is_file():
        print(f"error: clean ISO not found: {clean}", file=sys.stderr)
        return 2
    for p in (archive, data):
        if not p.is_file():
            print(f"error: fixture file missing: {p}", file=sys.stderr)
            return 2
    with clean.open("rb") as f:
        h = f.read(0x440)
        if len(h) < 0x428 or h[:6] != GAME_ID:
            print("error: not a GALE01 GameCube disc", file=sys.stderr)
            return 2
        if h[7] != 2:
            print(f"error: expected GALE01 Rev.02, got revision byte {h[7]}", file=sys.stderr)
            return 2
        dol_offset = int.from_bytes(h[0x420:0x424], "big")
        f.seek(dol_offset)
        size = _dol_size(f)
        f.seek(dol_offset)
        dol_bytes = f.read(size)
    digest = hashlib.sha256(data.read_bytes()).hexdigest()[:16]
    trailer = MARKER + digest.encode() + b"\0"
    pad = (-(len(dol_bytes) + len(trailer)) % 0x20)
    modified_dol = dol_bytes + trailer + b"\0" * pad
    tmp_dol = out.parent / (out.name + ".pas23.dol")
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp_dol.write_bytes(modified_dol)
    try:
        result = recompose_iso(clean, tmp_dol, out)
    finally:
        try:
            tmp_dol.unlink()
        except OSError:
            pass
    print(json.dumps({"clean": str(clean), "modified": str(result),
                      "expected_archive": str(archive), "expected_data": str(data),
                      "dol_size": len(dol_bytes), "trailer": trailer.decode()}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
