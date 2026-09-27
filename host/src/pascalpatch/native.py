"""Native-first runtime (melee-unlocked) adapter: Tier B character overlays and launch.

Tier B keeps the vanilla DOL: a character reaches the game only as a composed
replacement for an existing fighter data file inside the profile ISO.
"""
from __future__ import annotations
import hashlib, json, os, shutil
from pathlib import Path
from .character_package import installation_plan
from .errors import CompositionError, DiscoveryError, ValidationError
from .fighter_loader import FIGHTER_CODES, clone_slot_basename

HSD_HEADER = 0x20
PORT_NAMES = ("melee_port.exe", "melee_port")


def _hsd_file_size_ok(raw):
    return len(raw) >= HSD_HEADER and int.from_bytes(raw[0:4], "big") == len(raw)


def validate_character_entry(entry, profile_mode, index=0):
    """Validate one profile ``characters`` entry and return its overlay plan."""
    where = f"characters[{index}]"
    slot = str(entry.get("slot", "")).strip().lower()
    if slot not in FIGHTER_CODES:
        raise CompositionError("unknown character slot", [ValidationError(where + ".slot", "slot", f"unknown base fighter {slot!r}")])
    plan = installation_plan(entry["package"], profile_mode)
    expected = clone_slot_basename(slot)
    fighter, report = _checked_file(entry["fighter_file"], expected, ".slot.json", plan["metadata"]["id"], where + ".fighter_file")
    result = {"slot": slot, "iso_path": expected, "fighter_file": str(fighter), "character": report["character"],
              "slot_root": report.get("slot_root"), "sha256": report["output_sha256"], "package": plan["package"],
              "overlays": {expected: str(fighter)}}
    if entry.get("costume_file"):
        # The default (Nr) costume carries the imported model.
        costume_name = expected[:-4] + "Nr.dat"
        costume, costume_report = _checked_file(entry["costume_file"], costume_name, ".costume.json", plan["metadata"]["id"], where + ".costume_file")
        result["costume_file"] = str(costume); result["costume_sha256"] = costume_report["output_sha256"]
        result["overlays"][costume_name] = str(costume)
    if entry.get("animation_file"):
        # Borrowed/retargeted moves ship as a rebuilt animation archive.
        anim_name = expected[:-4] + "AJ.dat"
        anim, anim_report = _checked_file(entry["animation_file"], anim_name, ".anim.json", plan["metadata"]["id"], where + ".animation_file", hsd=False)
        result["animation_file"] = str(anim); result["animation_sha256"] = anim_report["output_sha256"]
        result["overlays"][anim_name] = str(anim)
    return result


def _checked_file(value, expected, sidecar_suffix, character_id, where, hsd=True):
    path = Path(value).expanduser().resolve()
    if path.name.lower() != expected.lower():
        raise CompositionError("file does not match slot", [ValidationError(where, "slot_mismatch", f"{path.name} cannot replace {expected}")])
    raw = path.read_bytes() if path.is_file() else b""
    if (hsd and not _hsd_file_size_ok(raw)) or not raw:
        raise CompositionError("composed file is not an HSD archive", [ValidationError(where, "hsd", str(path))])
    sidecar = path.with_name(path.name + sidecar_suffix)
    if not sidecar.is_file():
        raise CompositionError("composed file has no Character Studio report", [ValidationError(where, "provenance", f"missing {sidecar.name}")])
    report = json.loads(sidecar.read_text(encoding="utf-8"))
    if report.get("character") != character_id or report.get("output_sha256") != hashlib.sha256(raw).hexdigest():
        raise CompositionError("Character Studio report does not match package or file", [ValidationError(where, "provenance", "character id or output hash differs")])
    return path, report


def find_melee_port(explicit=None):
    candidates = [explicit, os.environ.get("MELEE_PORT")]
    candidates += [shutil.which(name) for name in PORT_NAMES]
    for value in candidates:
        if value and Path(value).is_file():
            return Path(value).resolve()
    raise DiscoveryError("melee_port.exe not found; pass --port or set MELEE_PORT")


def native_command(port, iso, extra_args=()):
    return [str(port), "--iso", str(iso), *[str(x) for x in extra_args]]
