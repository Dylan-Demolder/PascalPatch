from __future__ import annotations
import os, tempfile
from pathlib import Path

ALIGN = 0x20
HEADER_DOL = 0x420
HEADER_FST = 0x424
HEADER_FST_SIZE = 0x428
HEADER_MIN = 0x42C
MAX_REPLACEMENT = 64 * 1024 * 1024


def _align(value, alignment=ALIGN):
    return (value + alignment - 1) // alignment * alignment


def _read_header(base_iso):
    base = Path(base_iso).expanduser().resolve()
    if not base.is_file():
        raise ValueError("base ISO must be a file")
    with base.open("rb") as src:
        head = src.read(HEADER_MIN)
        if len(head) != HEADER_MIN:
            raise ValueError("truncated GameCube disc header")
    dol_offset = int.from_bytes(head[HEADER_DOL:HEADER_DOL + 4], "big")
    fst_offset = int.from_bytes(head[HEADER_FST:HEADER_FST + 4], "big")
    fst_size = int.from_bytes(head[HEADER_FST_SIZE:HEADER_FST_SIZE + 4], "big")
    if fst_offset <= dol_offset or fst_size < 12:
        raise ValueError("invalid GameCube disc header")
    return base, dol_offset, fst_offset, fst_size


def _read_fst(base):
    base, dol_offset, fst_offset, fst_size = _read_header(base)
    with base.open("rb") as src:
        src.seek(fst_offset)
        fst = src.read(fst_size)
    if len(fst) != fst_size:
        raise ValueError("truncated FST")
    return base, dol_offset, fst_offset, fst


def _parse_entries(fst):
    entry_count = int.from_bytes(fst[8:12], "big")
    if entry_count < 1 or entry_count * 12 > len(fst):
        raise ValueError("invalid FST entry count")
    strings = fst[entry_count * 12:]
    entries = []
    for index in range(entry_count):
        pos = index * 12
        flag = fst[pos]
        if flag not in (0, 1):
            raise ValueError(f"invalid FST entry flag at index {index}")
        name_off = int.from_bytes(fst[pos + 1:pos + 4], "big")
        if name_off >= len(strings):
            raise ValueError(f"FST name offset out of range at index {index}")
        end = strings.find(b"\0", name_off)
        if end < 0:
            raise ValueError(f"unterminated FST name at index {index}")
        try:
            name = strings[name_off:end].decode("ascii")
        except UnicodeDecodeError:
            raise ValueError(f"non-ASCII FST name at index {index}")
        if "/" in name or "\\" in name or name in (".", ".."):
            raise ValueError(f"unsafe FST name at index {index}")
        offset = int.from_bytes(fst[pos + 4:pos + 8], "big")
        size = int.from_bytes(fst[pos + 8:pos + 12], "big")
        entries.append({"index": index, "is_dir": flag == 1, "name": name,
                        "offset": offset, "size": size})
    for entry in entries:
        if entry["is_dir"] and (entry["size"] <= entry["index"] or entry["size"] > entry_count):
            raise ValueError(f"invalid directory range at index {entry['index']}")
    return entries


def _full_paths(entries):
    dirs = {}
    paths = {}
    for entry in entries:
        index = entry["index"]
        best = None
        for candidate, (_, end) in dirs.items():
            if candidate <= index < end and (best is None or candidate > best):
                best = candidate
        if entry["is_dir"]:
            if index == 0:
                full = ""
            else:
                if best is None:
                    raise ValueError(f"orphan directory entry at index {index}")
                if not entry["name"]:
                    raise ValueError(f"empty directory name at index {index}")
                parent, _ = dirs[best]
                full = (parent + "/" if parent else "") + entry["name"]
            dirs[index] = (full, entry["size"])
            paths[index] = full
        else:
            if best is None:
                raise ValueError(f"orphan file entry at index {index}")
            if not entry["name"]:
                raise ValueError(f"empty file name at index {index}")
            parent, _ = dirs[best]
            paths[index] = (parent + "/" if parent else "") + entry["name"]
    return paths


def _normalize_iso_path(value):
    text = str(value).replace("\\", "/").strip()
    while text.startswith("/"):
        text = text[1:]
    parts = [part for part in text.split("/") if part not in ("", ".")]
    if not parts or any(part == ".." for part in parts):
        raise ValueError(f"unsafe ISO path: {value!r}")
    return "/".join(parts)


def list_iso_files(base_iso):
    """List file entries in a GameCube ISO FST.

    Returns a list of {"path", "offset", "size"} dicts in FST order.
    """
    _, _, _, fst = _read_fst(base_iso)
    entries = _parse_entries(fst)
    paths = _full_paths(entries)
    return [{"path": paths[entry["index"]], "offset": entry["offset"], "size": entry["size"]}
            for entry in entries if not entry["is_dir"]]


def extract_iso_file(base_iso, iso_path):
    """Read one file out of a GameCube ISO without modifying the ISO."""
    target = _normalize_iso_path(iso_path)
    base, _, _, fst = _read_fst(base_iso)
    entries = _parse_entries(fst)
    paths = _full_paths(entries)
    for entry in entries:
        if not entry["is_dir"] and paths[entry["index"]] == target:
            if entry["size"] < 0:
                raise ValueError("invalid file size in FST")
            with base.open("rb") as src:
                src.seek(entry["offset"])
                data = src.read(entry["size"])
            if len(data) != entry["size"]:
                raise ValueError("ISO file data is truncated")
            return data
    raise FileNotFoundError(f"no such file in ISO: {target}")


def _coerce_replacement(value):
    if isinstance(value, (bytes, bytearray)):
        return bytes(value)
    path = Path(value).expanduser()
    if not path.is_file():
        raise ValueError(f"replacement source must be a file: {value!r}")
    return path.read_bytes()


def overlay_iso_files(base_iso, replacements, output):
    """Write a new ISO with existing FST files replaced by new bytes.

    Only files already present in the FST can be replaced; no entries are
    added or renamed, so the FST byte size is unchanged. Files are repacked
    after the FST with 0x20 alignment and the FST offsets/sizes are updated.
    The DOL/header region is copied verbatim and the input is never modified.
    """
    if not replacements:
        raise ValueError("at least one file replacement is required")
    wanted = {}
    for iso_path, source in replacements.items():
        target = _normalize_iso_path(iso_path)
        data = _coerce_replacement(source)
        if len(data) > MAX_REPLACEMENT:
            raise ValueError(f"replacement too large: {target}")
        if target in wanted:
            raise ValueError(f"duplicate replacement: {target}")
        wanted[target] = data
    base, _, fst_offset, fst = _read_fst(base_iso)
    out = Path(output).expanduser().resolve()
    if base == out:
        raise ValueError("output must differ from base ISO")
    entries = _parse_entries(fst)
    paths = _full_paths(entries)
    by_path = {}
    for entry in entries:
        if entry["is_dir"]:
            continue
        full = paths[entry["index"]]
        if full in by_path:
            raise ValueError(f"duplicate FST path: {full}")
        by_path[full] = entry
    for target in wanted:
        if target not in by_path:
            raise FileNotFoundError(f"no such file in ISO: {target}")
    iso_size = base.stat().st_size
    for entry in entries:
        if entry["is_dir"]:
            continue
        if entry["offset"] < 0 or entry["size"] < 0 or entry["offset"] + entry["size"] > iso_size:
            raise ValueError("FST file range is outside the ISO")
    new_fst = bytearray(fst)
    files_start = _align(fst_offset + len(fst))
    ordered = sorted((entry for entry in entries if not entry["is_dir"]),
                     key=lambda entry: (entry["offset"], entry["index"]))
    layout = []
    cursor = files_start
    for entry in ordered:
        full = paths[entry["index"]]
        data = wanted.get(full)
        size = len(data) if data is not None else entry["size"]
        cursor = _align(cursor)
        layout.append((entry, full, data, cursor, size))
        cursor += size
    total = max(cursor, files_start)
    for entry, _, _, new_offset, new_size in layout:
        pos = entry["index"] * 12
        new_fst[pos + 4:pos + 8] = new_offset.to_bytes(4, "big")
        new_fst[pos + 8:pos + 12] = new_size.to_bytes(4, "big")
    out.parent.mkdir(parents=True, exist_ok=True)
    fd, stage_name = tempfile.mkstemp(prefix=out.name + "-", dir=out.parent)
    os.close(fd)
    stage = Path(stage_name)
    try:
        with base.open("rb") as src, stage.open("w+b") as dst:
            dst.truncate(total)
            src.seek(0)
            remaining = fst_offset
            while remaining:
                chunk = src.read(min(1024 * 1024, remaining))
                if not chunk:
                    raise ValueError("source ISO ended unexpectedly")
                dst.write(chunk)
                remaining -= len(chunk)
            dst.seek(fst_offset)
            dst.write(new_fst)
            for entry, _, data, new_offset, new_size in layout:
                dst.seek(new_offset)
                if data is not None:
                    dst.write(data)
                else:
                    src.seek(entry["offset"])
                    remaining = new_size
                    while remaining:
                        chunk = src.read(min(1024 * 1024, remaining))
                        if not chunk:
                            raise ValueError("source ISO ended unexpectedly")
                        dst.write(chunk)
                        remaining -= len(chunk)
            dst.flush()
            os.fsync(dst.fileno())
        os.replace(stage, out)
    finally:
        try:
            stage.unlink()
        except FileNotFoundError:
            pass
    return out


def recompose_iso_with_fighter(base_iso, main_dol, output, overlays):
    """Recompose a GameCube ISO with a replacement DOL plus file overlays.

    Runs the existing DOL/FST-shift recomposition first, then applies
    ISO file replacements so a composed fighter archive is referenced by
    the game image. Returns the output path.
    """
    from .recompose_iso import recompose_iso
    out = Path(output).expanduser().resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    fd, stage_name = tempfile.mkstemp(prefix=out.name + "-dol-", dir=out.parent)
    os.close(fd)
    stage = Path(stage_name)
    try:
        recompose_iso(base_iso, main_dol, stage)
        return overlay_iso_files(stage, overlays, out)
    finally:
        try:
            stage.unlink()
        except FileNotFoundError:
            pass
