from __future__ import annotations
"""Mod catalog and per-profile mod management.

This module is the data layer behind the launcher GUI. It answers three
questions the GUI needs:

* which mods exist in the local catalog
* which of them a given profile has enabled
* how to turn one on or off without corrupting the profile manifest

Profiles stay plain JSON under <root>/profiles/<id>.json so the CLI and the
GUI always agree on the same source of truth.
"""
import json
from dataclasses import dataclass, field
from pathlib import Path

from .manifest import load_json, require_valid

PLUGIN_KIND = "plugin"
ASSET_KIND = "mod"


@dataclass(frozen=True)
class ModEntry:
    id: str
    name: str
    kind: str
    version: str
    capabilities: tuple = ()
    online_safe: bool = True
    dependencies: tuple = ()
    conflicts: tuple = ()
    description: str = ""
    manifest: Path = None
    status: str = "available"
    category: str = "General"
    requirements: tuple = ()
    tags: tuple = ()

    @property
    def gameplay_changing(self):
        caps = set(self.capabilities)
        return bool(caps - {"visual-only"})


class CatalogError(Exception):
    pass


def catalog(root):
    """Return every valid mod manifest under <root>/plugins and <root>/mods."""
    root = Path(root)
    entries = {}
    for kind, folder, filename in ((PLUGIN_KIND, "plugins", "plugin.json"), (ASSET_KIND, "mods", "mod.json")):
        base = root / folder
        if not base.is_dir():
            continue
        for manifest in sorted(base.glob("*/" + filename)):
            try:
                data = load_json(manifest)
                require_valid(data, kind, str(manifest))
            except Exception:
                continue
            ident = data["id"]
            entries[ident] = ModEntry(
                id=ident,
                name=data.get("name", ident),
                kind=kind,
                version=data.get("version", "?"),
                capabilities=tuple(data.get("capabilities", [])),
                online_safe=bool(data.get("online_safe", False)),
                dependencies=tuple(data.get("dependencies", [])),
                conflicts=tuple(data.get("conflicts", [])),
                description=data.get("description", ""),
                manifest=manifest,
                status="available",
                category=data.get("category", "General"),
                requirements=tuple(data.get("requirements", [])),
                tags=tuple(data.get("tags", [])),
            )
    return entries



# Roadmap entries are deliberately data-only. They are visible in the
# launcher, but set_enabled() only accepts entries with real manifests.
PLANNED_CATALOG = (
    {"id":"input-display","name":"Input Display","version":"1.0.0","kind":"plugin","status":"prototype","category":"Training","capabilities":("visual-only",),"online_safe":True,"description":"On-screen controller buttons, sticks and triggers for practice and replay review.","requirements":("runtime input events",),"tags":("overlay","inputs","training")},
    {"id":"training-tools","name":"Training Tools","version":"0.1.0","kind":"plugin","status":"prototype","category":"Training","capabilities":("gameplay-changing",),"online_safe":False,"description":"Frame counter, input history, frame advance and reset practice tools.","requirements":("runtime frame events",),"tags":("training","frame data","offline")},
    {"id":"frame-data-hud","name":"Frame Data HUD","version":"0.1.0","kind":"plugin","status":"planned","category":"Training","capabilities":("visual-only",),"online_safe":True,"description":"Startup, active, recovery and hitstun frame data in a compact in-game HUD.","requirements":("fighter state events",),"tags":("training","overlay","frame data")},
    {"id":"widescreen-plus","name":"Widescreen Plus","version":"0.1.0","kind":"mod","status":"planned","category":"Visuals","capabilities":("visual-only",),"online_safe":True,"description":"A safe widescreen presentation pack with camera and HUD adjustments.","requirements":("camera patch",),"tags":("visual","widescreen","quality of life")},
    {"id":"hd-texture-pack","name":"HD Texture Packs","version":"0.1.0","kind":"mod","status":"planned","category":"Visuals","capabilities":("visual-only",),"online_safe":True,"description":"Per-profile texture replacements with budgets, previews and conflict checks.","requirements":("user-supplied original assets",),"tags":("visual","textures","assets")},
    {"id":"stage-music-manager","name":"Stage Music Manager","version":"0.1.0","kind":"mod","status":"planned","category":"Audio","capabilities":("visual-only",),"online_safe":True,"description":"Choose, shuffle and preview stage music without changing gameplay.","requirements":("audio replacement pipeline",),"tags":("audio","stages","quality of life")},
    {"id":"costume-packs","name":"Costume Packs","version":"0.1.0","kind":"mod","status":"planned","category":"Characters","capabilities":("visual-only",),"online_safe":True,"description":"Installable costume and menu-art packs with profile-local composition.","requirements":("validated asset package",),"tags":("characters","costumes","visual")},
    {"id":"character-packages","name":"Offline Character Packages","version":"0.1.0","kind":"mod","status":"planned","category":"Characters","capabilities":("gameplay-changing",),"online_safe":False,"description":"Install validated .melee-character packages into Offline profiles.","requirements":("Character Studio export", "HSD conversion"),"tags":("characters","offline","studio")},
    {"id":"tournament-hud","name":"Tournament HUD","version":"0.1.0","kind":"plugin","status":"planned","category":"Tournament","capabilities":("visual-only",),"online_safe":True,"description":"Minimal stock-count, timer and player-name overlays for local setups.","requirements":("render overlay API",),"tags":("tournament","overlay","local")},
    {"id":"replay-analyzer","name":"Replay Analyzer","version":"0.1.0","kind":"plugin","status":"planned","category":"Tools","capabilities":("visual-only",),"online_safe":True,"description":"Inspect frame advantage, inputs, openings and neutral patterns from replays.","requirements":("replay parser",),"tags":("replays","analysis","tools")},
    {"id":"stage-expansion","name":"Stage Expansion Pack","version":"0.1.0","kind":"mod","status":"planned","category":"Stages","capabilities":("gameplay-changing",),"online_safe":False,"description":"Offline stage and event content delivered as isolated profile packages.","requirements":("stage composition pipeline",),"tags":("stages","offline","content")},
)


def all_catalog(root):
    """Return installed manifests plus visible roadmap entries."""
    entries = catalog(root)
    for item in PLANNED_CATALOG:
        if item["id"] not in entries:
            entries[item["id"]] = ModEntry(**item)
    return entries

def profile_path(root, profile_id):
    return Path(root) / "profiles" / (profile_id + ".json")


def read_profile(root, profile_id):
    path = profile_path(root, profile_id)
    if not path.is_file():
        raise CatalogError("no such profile: " + profile_id)
    return path, json.loads(path.read_text(encoding="utf-8"))


def write_profile(root, profile_id, data):
    path = profile_path(root, profile_id)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return path


def enabled_ids(data):
    return set(data.get("plugins", [])) | set(data.get("mods", []))


def _bucket(entry):
    return "plugins" if entry.kind == PLUGIN_KIND else "mods"


def resolve(root, profile_id):
    """Return (entries, enabled, missing) for one profile."""
    _, data = read_profile(root, profile_id)
    entries = catalog(root)
    enabled = enabled_ids(data)
    missing = sorted(i for i in enabled if i not in entries)
    return entries, enabled, missing


def set_enabled(root, profile_id, mod_id, enabled):
    """Turn a mod on or off for one profile and persist the manifest.

    Returns a dict describing what changed, including any dependency
    additions or dependents that had to be removed.
    """
    entries = catalog(root)
    if mod_id not in entries:
        raise CatalogError("unknown mod: " + mod_id)
    path, data = read_profile(root, profile_id)
    plugins = list(data.get("plugins", []))
    mods = list(data.get("mods", []))
    changed = []

    def add(ident):
        entry = entries[ident]
        bucket = plugins if entry.kind == PLUGIN_KIND else mods
        if ident not in bucket:
            for dep in entry.dependencies:
                if dep not in plugins and dep not in mods:
                    if dep in entries:
                        add(dep)
                        changed.append("+ dependency " + dep)
            bucket.append(ident)
            changed.append("+ " + ident)

    def remove(ident, cascade=False):
        entry = entries.get(ident)
        buckets = [plugins, mods]
        for bucket in buckets:
            if ident in bucket:
                bucket.remove(ident)
                changed.append("- " + ident)
        if not cascade:
            return
        for other_id, other in entries.items():
            if ident in other.dependencies and (other_id in plugins or other_id in mods):
                remove(other_id, cascade=True)
                changed.append("- " + other_id + " (requires " + ident + ")")

    if enabled:
        add(mod_id)
    else:
        remove(mod_id, cascade=True)

    # Conflict checks stay visible rather than silently blocking the user.
    warnings = []
    active = set(plugins) | set(mods)
    for ident in sorted(active):
        entry = entries.get(ident)
        if not entry:
            continue
        for other in entry.conflicts:
            if other in active:
                warnings.append(ident + " conflicts with " + other)
        for dep in entry.dependencies:
            if dep not in active:
                warnings.append(ident + " needs " + dep)

    data["plugins"] = plugins
    data["mods"] = mods
    write_profile(root, profile_id, data)
    return {"profile": profile_id, "mod": mod_id, "enabled": enabled, "changed": changed, "warnings": warnings}


def create_profile(root, profile_id, name, base_game, mode="offline", builder=None):
    """Create a new profile manifest. builder supplies static-build paths."""
    import re as _re
    if not _re.fullmatch(r"[a-z0-9][a-z0-9._-]{1,63}", profile_id):
        raise CatalogError("profile ID must be lowercase letters, digits, dot, dash or underscore")
    path = profile_path(root, profile_id)
    if path.exists():
        raise CatalogError("profile already exists: " + profile_id)
    data = {
        "id": profile_id,
        "name": name,
        "game_version": "GALE01-1.02",
        "base_game": str(base_game),
        "plugins": [],
        "mods": [],
        "mode": mode,
        "online_safe": mode in ("tournament-safe", "slippi"),
    }
    if builder:
        data.update(builder)
    write_profile(root, profile_id, data)
    return data


def delete_profile(root, profile_id):
    path = profile_path(root, profile_id)
    if not path.is_file():
        raise CatalogError("no such profile: " + profile_id)
    path.unlink()
    return path
