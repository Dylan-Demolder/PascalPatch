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
PORTRAIT_BYTES = 136 * 188 * 2   # a select-screen door image, RGB5A3
ICON_BYTES = 64 * 56 * 2         # a select-screen grid icon, RGB5A3
STOCK_BYTES = 24 * 24 * 2        # a stock icon above the damage in a match, RGB5A3
# The character select screen's kind for each fighter slot (CharacterKind, src/melee/ft/forward.h).
CSS_KINDS = {
    "captain-falcon": 0, "donkey-kong": 1, "fox": 2, "mr-game-and-watch": 3, "kirby": 4, "bowser": 5, "link": 6,
    "luigi": 7, "mario": 8, "marth": 9, "mewtwo": 10, "ness": 11, "peach": 12, "pikachu": 13, "ice-climbers": 14,
    "jigglypuff": 15, "samus": 16, "yoshi": 17, "zelda": 18, "sheik": 19, "falco": 20, "young-link": 21,
    "dr-mario": 22, "roy": 23, "pichu": 24, "ganondorf": 25,
}
# Internal fighter kinds (FighterKind, src/melee/ft/forward.h).
INTERNAL_KINDS = {
    "mario": 0, "fox": 1, "captain-falcon": 2, "donkey-kong": 3, "kirby": 4, "bowser": 5, "link": 6, "sheik": 7,
    "ness": 8, "peach": 9, "ice-climbers": 10, "pikachu": 12, "samus": 13, "yoshi": 14, "jigglypuff": 15,
    "mewtwo": 16, "luigi": 17, "marth": 18, "zelda": 19, "young-link": 20, "dr-mario": 21, "falco": 22,
    "pichu": 23, "mr-game-and-watch": 24, "ganondorf": 25, "roy": 26,
}
# A character installed as a new fighter (install: "new") is added to the game, and the
# fighter it is built on stays: its files are added to the disc under names of its own
# (PlX0.dat, PlX0Nr.dat, PlX0AJ.dat, then PlX1*.dat, ...), and the extra-fighters native
# plugin puts it on the character select screen (paged when it runs out of room) and, for
# each match, lends it one of the fighter kinds VS mode never uses. The codes use digits so
# no name can clash with a fighter's own (the disc's names are matched without case).
NEW_FIGHTER_CODES = tuple(f"{letter}{digit}" for letter in "XYZW" for digit in "0123456789")
# Fighters that change into another kind (Zelda <-> Sheik) or are two (the Ice Climbers) need
# more than one kind, so they can only replace their slot.
NOT_NEW = {"zelda", "sheik", "ice-climbers"}
INSTALL_MODES = ("replace", "new")


def _hsd_file_size_ok(raw):
    return len(raw) >= HSD_HEADER and int.from_bytes(raw[0:4], "big") == len(raw)


def validate_character_entry(entry, profile_mode, index=0):
    """Validate one profile ``characters`` entry and return its overlay plan."""
    where = f"characters[{index}]"
    slot = str(entry.get("slot", "")).strip().lower()
    if slot not in FIGHTER_CODES:
        raise CompositionError("unknown character slot", [ValidationError(where + ".slot", "slot", f"unknown base fighter {slot!r}")])
    install = entry.get("install", "replace")
    if install not in INSTALL_MODES or (install == "new" and slot in NOT_NEW):
        raise CompositionError("unsupported install mode", [ValidationError(where + ".install", "install",
                               f"{slot} can only replace its slot" if install == "new" else f"install must be one of {INSTALL_MODES}")])
    plan = installation_plan(entry["package"], profile_mode)
    expected = clone_slot_basename(slot)
    fighter, report = _checked_file(entry["fighter_file"], expected, ".slot.json", plan["metadata"]["id"], where + ".fighter_file")
    result = {"slot": slot, "iso_path": expected, "fighter_file": str(fighter), "character": report["character"],
              "slot_root": report.get("slot_root"), "sha256": report["output_sha256"], "package": plan["package"],
              "overlays": {expected: str(fighter)}, "install": install}
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
    if entry.get("move_graft"):
        # Borrowed specials: the grafted actions are in the fighter and animation files; the
        # donor's code is attached at run time by the move-graft native plugin.
        if not entry.get("animation_file"):
            raise CompositionError("grafted specials need their animation file", [ValidationError(where + ".animation_file", "required", "move_graft needs animation_file")])
        path = Path(entry["move_graft"]).expanduser().resolve()
        doc = json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}
        if doc.get("character") != plan["metadata"]["id"] or not isinstance(doc.get("grafts"), list):
            raise CompositionError("move-graft config does not match the package", [ValidationError(where + ".move_graft", "provenance", str(path))])
        result["move_graft"] = str(path); result["grafts"] = doc["grafts"]
    # The name and (optional) photo the character select screen shows for the slot; the
    # extra-fighters native plugin puts them on screen at run time.
    result["display_name"] = plan["metadata"].get("display_name") or plan["metadata"]["id"]
    if entry.get("portrait"):
        portrait, _ = _checked_file(entry["portrait"], plan["metadata"]["id"] + ".portrait", ".json", plan["metadata"]["id"], where + ".portrait", hsd=False)
        if portrait.stat().st_size != PORTRAIT_BYTES:
            raise CompositionError("portrait is not a 136x188 RGB5A3 image", [ValidationError(where + ".portrait", "size", str(portrait))])
        result["portrait"] = str(portrait)
    if entry.get("icon"):
        icon, _ = _checked_file(entry["icon"], plan["metadata"]["id"] + ".icon", ".json", plan["metadata"]["id"], where + ".icon", hsd=False)
        if icon.stat().st_size != ICON_BYTES:
            raise CompositionError("icon is not a 64x56 RGB5A3 image", [ValidationError(where + ".icon", "size", str(icon))])
        result["icon"] = str(icon)
    if entry.get("stock"):
        stock, _ = _checked_file(entry["stock"], plan["metadata"]["id"] + ".stock", ".json", plan["metadata"]["id"], where + ".stock", hsd=False)
        if stock.stat().st_size != STOCK_BYTES:
            raise CompositionError("stock icon is not a 24x24 RGB5A3 image", [ValidationError(where + ".stock", "size", str(stock))])
        result["stock"] = str(stock)
    return result


def assign_new_fighters(characters):
    """Give each character installed as a new fighter disc files of its own.

    Its files are added to the disc (``additions``) under a new code instead of replacing
    its base fighter's: PlPr.dat -> PlX0.dat, PlPrNr.dat -> PlX0Nr.dat, PlPrAJ.dat ->
    PlX0AJ.dat. Its grafted specials are keyed by the character, since the fighter kind it
    plays as is picked per match. Returns ``characters``.
    """
    new = [c for c in characters if c.get("install") == "new"]
    if len(new) > len(NEW_FIGHTER_CODES):
        raise CompositionError("too many new fighters", [ValidationError("characters", "install",
                               f"at most {len(NEW_FIGHTER_CODES)} new fighters")])
    for c, code in zip(new, NEW_FIGHTER_CODES):
        base = clone_slot_basename(c["slot"])[:4]   # "PlPr"
        c["base_kind"] = INTERNAL_KINDS[c["slot"]]
        c["iso_path"] = f"Pl{code}.dat"
        c["additions"] = {f"Pl{code}{name[len(base):]}": path for name, path in c["overlays"].items()}
        c["overlays"] = {}
        c["files"] = {role: f"Pl{code}{suffix}.dat" for role, suffix in (("dat", ""), ("costume", "Nr"), ("anim", "AJ"))
                      if f"Pl{code}{suffix}.dat" in c["additions"]}
        if c.get("grafts"):
            c["grafts"] = [{k: v for k, v in {**g, "character": c["character"]}.items() if k != "fighter"} for g in c["grafts"]]
    return characters


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


NATIVE_PLUGIN_ENV = "PASCALPATCH_NATIVE_PLUGINS"
LEGACY_PLUGIN_ENV = "MELEEMOD_NATIVE_PLUGINS"   # the name before the MeleeMod -> PascalPatch rename
# A release download ships the runtime and the built-in plugins prebuilt in <PascalPatch>/bin, so
# players need no compiler; a source checkout has no bin/ and builds into <data>/native-plugins.
BUNDLED_BIN = Path(__file__).resolve().parents[3] / "bin"


def _plugin_env():
    return os.environ.get(NATIVE_PLUGIN_ENV) or os.environ.get(LEGACY_PLUGIN_ENV)


def native_folders(data_root):
    """Where built native binaries are looked for, first match wins: $PASCALPATCH_NATIVE_PLUGINS,
    a release's bin/, then <data>/native-plugins."""
    return [f for f in (_plugin_env(), BUNDLED_BIN if BUNDLED_BIN.is_dir() else None, Path(data_root) / "native-plugins") if f]


def find_native_plugin(name, data_root):
    """A built native plugin DLL, from the first of native_folders() that has <name>.dll."""
    for folder in native_folders(data_root):
        if folder and (Path(folder) / f"{name}.dll").is_file():
            return (Path(folder) / f"{name}.dll").resolve()
    raise DiscoveryError(f"native plugin {name}.dll not built; run tooling/native/build_plugins.py --data {data_root}")


def stage_native_plugins(characters, data_root, folder, unlock_all=False):
    """Write the runtime's plugin folder; returns its report, or None if no plugin is needed.

    ``unlock_all`` adds the unlock-all plugin (every character selectable, offline only);
    grafted specials in ``characters`` add move-graft with their merged config; characters
    add extra-fighters, which puts their names, portraits and icons on the character select
    screen, their stock icons in matches, and adds the ones installed as new fighters to the
    select screen (call assign_new_fighters first).
    """
    grafts = [g for c in characters for g in c.get("grafts", [])]
    pictures = lambda c: {k: Path(c[k]).name for k in ("portrait", "icon", "stock") if c.get(k)}
    slots = [{"id": Path(c["package"]).stem, "ckind": CSS_KINDS[c["slot"]], "name": c["display_name"], **pictures(c)}
             for c in characters if c.get("display_name") and c["slot"] in CSS_KINDS and c.get("install", "replace") == "replace"]
    fighters = [{"id": c["character"], "name": c.get("display_name") or c["character"], "base": c["base_kind"],
                 "base_ckind": CSS_KINDS[c["slot"]], **c["files"], **pictures(c)}
                for c in characters if c.get("install") == "new"]
    wanted = ((["unlock-all"] if unlock_all else []) + (["move-graft"] if grafts else [])
              + (["extra-fighters"] if slots or fighters else []))
    if not wanted:
        return None
    folder = Path(folder); folder.mkdir(parents=True, exist_ok=True); plugins = []; notes = []
    for name in wanted:
        try:
            dll = find_native_plugin(name, data_root)
        except DiscoveryError as exc:
            if name == "move-graft" or (name == "extra-fighters" and fighters):   # nothing works without them
                raise
            notes.append(f"{exc} ({'characters stay locked' if name == 'unlock-all' else 'the select screen keeps the base names and portraits'} until it is built)")
            continue
        shutil.copy2(dll, folder / dll.name)
        entry = {"id": name, "sha256": hashlib.sha256(dll.read_bytes()).hexdigest()}
        if name == "move-graft":
            (folder / "move-graft.json").write_text(json.dumps({"grafts": grafts}, indent=1) + "\n", encoding="utf-8")
            entry["grafts"] = len(grafts)
        if name == "extra-fighters":
            for c in characters:
                for k in ("portrait", "icon", "stock"):
                    if c.get(k):
                        shutil.copy2(c[k], folder / Path(c[k]).name)
            (folder / "extra-fighters.json").write_text(json.dumps({"fighters": fighters, "slots": slots}, indent=1) + "\n", encoding="utf-8")
            entry["fighters"] = len(fighters); entry["slots"] = len(slots)
            entry["portraits"] = sum(1 for s in slots + fighters if "portrait" in s)
            entry["icons"] = sum(1 for s in slots + fighters if "icon" in s)
            entry["stocks"] = sum(1 for s in slots + fighters if "stock" in s)
        plugins.append(entry)
    if not plugins:
        return {"notes": notes} if notes else None
    return {"folder": str(folder), "plugins": plugins, **({"notes": notes} if notes else {})}


def stage_quick_match(match, data_root, folder):
    """Stage the quick-match plugin set to boot straight into ``match`` (a profile's quick_match).

    Fighters given by slot name ("fox") are passed on as character numbers, which is what the
    plugin reads. Returns the plugin's entry for a staging report.
    """
    dll = find_native_plugin("quick-match", data_root)
    folder = Path(folder); folder.mkdir(parents=True, exist_ok=True)
    shutil.copy2(dll, folder / dll.name)
    m = {k: CSS_KINDS[v] if k in ("p1", "p2") and isinstance(v, str) else v for k, v in match.items()}
    config = {"match": m, "_pascalpatch": {"id": "quick-match", "name": "Quick Match", "source": "profile"}}
    (folder / "quick-match.json").write_text(json.dumps(config, indent=1) + "\n", encoding="utf-8")
    return {"id": "quick-match", "sha256": hashlib.sha256(dll.read_bytes()).hexdigest(), "match": m}


# Melee Unlocked 0.8 ships several exes in one folder; plugins load only into the Static Recomp.
WRONG_EXES = {
    "melee_source.exe": "melee_source.exe is the Source Port, which runs Slippi online and cannot load plugins",
    "meleeunlockedlauncher.exe": "MeleeUnlockedLauncher.exe is Melee Unlocked's launcher, not the game",
    "melee_port_playback.exe": "melee_port_playback.exe is the replay player",
}


def check_melee_port(path):
    """Raise a DiscoveryError that names the right file when ``path`` is not a game exe plugins can load into."""
    path = Path(path)
    why = WRONG_EXES.get(path.name.lower())
    if why:
        raise DiscoveryError(f"{why}. Pick melee_port.exe in the same folder.")
    if path.suffix.lower() != ".exe" and path.name not in PORT_NAMES:
        raise DiscoveryError(f"{path.name} is not a program. Pick melee_port.exe in your Melee Unlocked folder.")
    return path


def find_melee_port(explicit=None):
    candidates = [explicit, os.environ.get("MELEE_PORT")]
    candidates += [shutil.which(name) for name in PORT_NAMES]
    for value in candidates:
        if value and Path(value).is_file():
            return check_melee_port(Path(value).resolve())
    raise DiscoveryError("melee_port.exe not found; pass --port or set MELEE_PORT")


def find_pascalpatch_launcher(data_root, explicit=None):
    """pascalpatch-launch.exe with pascalpatch_runtime.dll beside it (built by tooling/native/build_plugins.py)."""
    for folder in ([explicit] if explicit else []) + native_folders(data_root):
        # Builds from before the rename have meleemod-launch.exe + meleemod_runtime.dll; they still work.
        for exe_name, dll in (("pascalpatch-launch.exe", "pascalpatch_runtime.dll"), ("meleemod-launch.exe", "meleemod_runtime.dll")):
            exe = Path(folder) / exe_name if folder else None
            if exe and exe.is_file() and (exe.parent / dll).is_file():
                return exe.resolve()
    raise DiscoveryError(f"pascalpatch-launch.exe not built; run tooling/native/build_plugins.py --data {data_root}")


def release_args(port, data_root, given=()):
    """The folder flags an unzipped Melee Unlocked release needs, as its own launcher passes them.

    A release keeps Slippi's system files in Sys/ beside melee_port.exe; without --sys-dir the game
    looks for a source checkout's port/slippi_sys and stops at boot. The game shares the release's
    port-settings.ini (graphics, controllers), but saves to PascalPatch's own memory card, so
    plugins never touch the player's real save, and gets no --user-dir, so the Slippi login stays
    hidden. Flags already in ``given`` (the user's port arguments) are left to the user.
    """
    folder = Path(port).parent
    if not (folder / "Sys" / "codehandler.bin").is_file():
        return []   # a source build: its defaults are right
    given = {str(a).split("=", 1)[0] for a in given}
    card = Path(data_root).resolve() / "memory-card" / "CardA"
    out = []
    for flag, value in (("--sys-dir", folder / "Sys"), ("--settings-path", folder / "port-settings.ini"), ("--card-dir", card)):
        if flag not in given:
            out += [flag, str(value)]
    if "--card-dir" not in given:
        card.mkdir(parents=True, exist_ok=True)
    return out


def native_command(port, iso, extra_args=(), mods=None, launcher=None, sandbox=None, log=None, settings=None):
    """The port's command line, run through PascalPatch's launcher when one is given.

    The port itself is never modified: the launcher starts it suspended and injects the PascalPatch
    runtime, which loads the plugins in ``mods``, refuses every network request and (with
    ``sandbox``) hides the user's Slippi Launcher login behind a private APPDATA. Plugin settings
    changed in the F2 overlay are saved to ``settings``.
    """
    cmd = [str(port), "--iso", str(iso), *[str(x) for x in extra_args]]
    if not launcher:
        return cmd
    front = [str(launcher)]   # absolute: the launcher runs in the port's working directory
    for flag, value in (("--mods", mods), ("--sandbox", sandbox), ("--log", log), ("--settings", settings)):
        if value:
            front += [flag, str(Path(value).resolve())]
    return front + ["--"] + cmd
