#!/usr/bin/env python3
"""Build the PAS-24 modified ISO on the runner from the runner's own clean ISO.

Reads the runner-local clean GALE01 ISO, extracts its main.dol (pure python,
no toolkit required), appends a synthetic PAS-24 marker trailer (game code
sections are left untouched), recomposes a bootable modified ISO via
``meleemod.recompose_iso``, then overlays the Studio-composed custom fighter
archive onto its clone-slot file (``Pl<Code>.dat`` for the fixture's
``base_fighter``) via ``meleemod.iso_files`` so the game's fighter loader
resolves the custom bytes for that slot. The overlay is verified by reading
the file back out of the modified ISO and comparing bytes. No Nintendo data
is committed or transferred; the only repo inputs are the synthetic files
under ``tooling/fixtures/pas23/``.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "host/src"))
from meleemod.recompose_iso import recompose_iso
from meleemod.iso_files import extract_iso_file, list_iso_files, overlay_iso_files
from meleemod.fighter_loader import (
    FighterLoaderError,
    find_clone_slot,
    symbol_for_character_id,
    write_fighter_loader_plugin,
)

GAME_ID = b"GALE01"
MARKER = b"PAS24-FIGHTER-LOADER-MARKER-v1:"


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
    a = argparse.ArgumentParser(description="Build PAS-24 modified ISO on the runner")
    a.add_argument("--clean", required=True, help="Runner-local clean GALE01 Rev.02 ISO")
    a.add_argument("--fixture", required=True, help="Checked-out synthetic fixture dir")
    a.add_argument("--output", required=True, help="Where to write the modified ISO")
    x = a.parse_args(argv)
    clean = Path(x.clean).expanduser().resolve()
    fixture = Path(x.fixture).expanduser().resolve()
    out = Path(x.output).expanduser().resolve()
    archive = fixture / "leesin-hsd-2e.melee-character"
    data = fixture / "leesin-patched.hsd"
    project = fixture / "project"
    if not clean.is_file():
        print(f"error: clean ISO not found: {clean}", file=sys.stderr)
        return 2
    for p in (archive, data):
        if not p.is_file():
            print(f"error: fixture file missing: {p}", file=sys.stderr)
            return 2
    try:
        character = json.loads((project / "character.json").read_text())
        moveset = json.loads((project / "moveset.json").read_text())
    except (OSError, json.JSONDecodeError) as exc:
        print(f"error: fixture project metadata unreadable: {exc}", file=sys.stderr)
        return 2
    base_fighter = moveset.get("base_fighter", "mario")
    try:
        symbol = symbol_for_character_id(character["id"])
    except (KeyError, FighterLoaderError) as exc:
        print(f"error: cannot derive fighter symbol: {exc}", file=sys.stderr)
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
    fighter_bytes = data.read_bytes()
    digest = hashlib.sha256(fighter_bytes).hexdigest()[:16]
    trailer = MARKER + symbol.encode() + b":" + digest.encode() + b"\0"
    pad = (-(len(dol_bytes) + len(trailer)) % 0x20)
    modified_dol = dol_bytes + trailer + b"\0" * pad
    tmp_dol = out.parent / (out.name + ".pas24.dol")
    stage_iso = out.parent / (out.name + ".pas24.stage.iso")
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp_dol.write_bytes(modified_dol)
    try:
        recompose_iso(clean, tmp_dol, stage_iso)
    finally:
        try:
            tmp_dol.unlink()
        except OSError:
            pass
    try:
        slot_path = find_clone_slot(list_iso_files(stage_iso), base_fighter)
        overlay_iso_files(stage_iso, {slot_path: fighter_bytes}, out)
        roundtrip = extract_iso_file(out, slot_path)
    except (ValueError, FileNotFoundError, FighterLoaderError) as exc:
        print(f"error: fighter overlay failed: {exc}", file=sys.stderr)
        try:
            stage_iso.unlink()
        except OSError:
            pass
        return 2
    try:
        stage_iso.unlink()
    except OSError:
        pass
    if roundtrip != fighter_bytes:
        print(f"error: overlay verification failed for {slot_path}", file=sys.stderr)
        return 2
    plugin = write_fighter_loader_plugin(symbol, out.parent / "pas24-fighter-loader")
    print(json.dumps({"clean": str(clean), "modified": str(out),
                      "expected_archive": str(archive), "expected_data": str(data),
                      "dol_size": len(dol_bytes), "trailer": trailer.decode(),
                      "base_fighter": base_fighter, "slot_iso_path": slot_path,
                      "fighter_symbol": symbol,
                      "fighter_sha256": hashlib.sha256(fighter_bytes).hexdigest(),
                      "overlay_verified": True,
                      "loader_plugin": str(plugin)}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
