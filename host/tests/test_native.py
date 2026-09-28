import hashlib, json, sys, tempfile, unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from pascalpatch.errors import CompositionError, ManifestError
from pascalpatch.manifest import validate_quick_match
from pascalpatch.native import assign_new_fighters, native_command, stage_native_plugins, stage_quick_match, validate_character_entry
from pascalpatch.profile import load_profile
from pascalpatch.store import BuildStore


def _package(root, compatibility="offline-gameplay"):
    package = root / "ember.melee-character"
    package.mkdir()
    (package / "character.json").write_text(json.dumps({"id": "ember", "display_name": "Ember", "version": "1.0.0", "author": "t", "license": "CC0", "target_game_version": "GALE01-1.02", "compatibility": compatibility}))
    (package / "moveset.json").write_text(json.dumps({"base_fighter": "falco", "moves": []}))
    (package / "checksums.json").write_text(json.dumps({n: hashlib.sha256((package / n).read_bytes()).hexdigest() for n in ("character.json", "moveset.json")}))
    return package


def _fighter(root, name="PlFc.dat", character="ember"):
    raw = bytearray(0x40)
    raw[0:4] = len(raw).to_bytes(4, "big")
    path = root / name
    path.write_bytes(raw)
    (root / (name + ".slot.json")).write_text(json.dumps({"character": character, "slot_root": "ftDataFalco", "output_sha256": hashlib.sha256(raw).hexdigest()}))
    return path


class NativeTests(unittest.TestCase):
    def test_character_entry_requires_matching_slot_and_provenance(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            package, fighter = _package(root), _fighter(root)
            plan = validate_character_entry({"package": str(package), "slot": "Falco", "fighter_file": str(fighter)}, "offline")
            self.assertEqual((plan["iso_path"], plan["slot_root"]), ("PlFc.dat", "ftDataFalco"))
            with self.assertRaisesRegex(CompositionError, "does not match slot"):
                validate_character_entry({"package": str(package), "slot": "fox", "fighter_file": str(fighter)}, "offline")
            fighter.write_bytes(fighter.read_bytes()[:-4] + b"XXXX")
            with self.assertRaisesRegex(CompositionError, "Character Studio report"):
                validate_character_entry({"package": str(package), "slot": "falco", "fighter_file": str(fighter)}, "offline")

    def test_character_profiles_are_offline_only(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); (root / "profiles").mkdir()
            _package(root); _fighter(root)
            doc = {"id": "ember-offline", "name": "P", "game_version": "GALE01-1.02", "base_game": "../base.iso", "plugins": [], "mods": [], "mode": "slippi", "online_safe": True,
                   "characters": [{"package": "../ember.melee-character", "slot": "falco", "fighter_file": "../PlFc.dat"}]}
            (root / "profiles/p.json").write_text(json.dumps(doc))
            with self.assertRaises(ManifestError):
                load_profile(root / "profiles/p.json", root)
            doc["mode"] = "offline"; (root / "profiles/p.json").write_text(json.dumps(doc))
            self.assertEqual(load_profile(root / "profiles/p.json", root).compatibility, "offline-only")

    def test_iso_build_overlays_fighter_file_and_keeps_dol(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); (root / "profiles").mkdir(); (root / "base.iso").write_bytes(b"disc")
            _package(root); _fighter(root)
            doc = {"id": "ember-offline", "name": "P", "game_version": "GALE01-1.02", "base_game": "../base.iso", "plugins": [], "mods": [], "mode": "offline", "online_safe": False,
                   "characters": [{"package": "../ember.melee-character", "slot": "falco", "fighter_file": "../PlFc.dat"}]}
            (root / "profiles/p.json").write_text(json.dumps(doc))
            seen = {}
            def fake_overlay(base, replacements, output, additions=None):
                seen.update(replacements); Path(output).write_bytes(b"overlaid")
            with patch("pascalpatch.store.inspect_game", return_value=SimpleNamespace(kind="iso", main_dol_sha1="x")), patch("pascalpatch.store.overlay_iso_files", side_effect=fake_overlay):
                result = BuildStore(root / "data").build(load_profile(root / "profiles/p.json", root))
            self.assertEqual(list(seen), ["PlFc.dat"])
            meta = json.loads(result.metadata.read_text())
            self.assertEqual(meta["plugin_composition"], "native-tier-b-data-overlay")
            self.assertEqual(meta["characters"][0]["character"], "ember")
            self.assertEqual(result.output.read_bytes(), b"overlaid")

    def test_costume_file_overlays_default_costume(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            package = _package(root); fighter = _fighter(root); costume = _fighter(root, "PlFcNr.dat")
            costume.with_name("PlFcNr.dat.slot.json").rename(costume.with_name("PlFcNr.dat.costume.json"))
            plan = validate_character_entry({"package": str(package), "slot": "falco", "fighter_file": str(fighter), "costume_file": str(costume)}, "offline")
            self.assertEqual(sorted(plan["overlays"]), ["PlFc.dat", "PlFcNr.dat"])
            with self.assertRaisesRegex(CompositionError, "does not match slot"):
                validate_character_entry({"package": str(package), "slot": "falco", "fighter_file": str(fighter), "costume_file": str(fighter)}, "offline")
            anim = root / "PlFcAJ.dat"; anim.write_bytes(b"\0" * 64)
            (root / "PlFcAJ.dat.anim.json").write_text(json.dumps({"character": "ember", "output_sha256": hashlib.sha256(anim.read_bytes()).hexdigest()}))
            plan = validate_character_entry({"package": str(package), "slot": "falco", "fighter_file": str(fighter), "animation_file": str(anim)}, "offline")
            self.assertEqual(sorted(plan["overlays"]), ["PlFc.dat", "PlFcAJ.dat"])

    def test_roster_profile_overlays_every_slot(self):
        # Mirrors the profile Character Studio's build-roster writes: one entry per slot.
        slots = {"donkey-kong": "PlDk", "captain-falcon": "PlCa", "jigglypuff": "PlPr", "link": "PlLk", "fox": "PlFx",
                 "bowser": "PlKp", "marth": "PlMs", "samus": "PlSs", "mewtwo": "PlMt", "ganondorf": "PlGn"}
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); (root / "profiles").mkdir(); (root / "base.iso").write_bytes(b"disc")
            entries = []
            for slot, code in slots.items():
                d = root / "build" / slot; d.mkdir(parents=True)
                package = _package(d)
                entry = {"package": str(package), "slot": slot, "fighter_file": str(_fighter(d, code + ".dat"))}
                costume = _fighter(d, code + "Nr.dat")
                costume.with_name(code + "Nr.dat.slot.json").rename(costume.with_name(code + "Nr.dat.costume.json"))
                entry["costume_file"] = str(costume); entries.append(entry)
            doc = {"id": "custom-roster", "name": "Roster", "game_version": "GALE01-1.02", "base_game": "../base.iso", "plugins": [], "mods": [],
                   "mode": "offline", "online_safe": False, "characters": entries}
            (root / "profiles/custom-roster.json").write_text(json.dumps(doc))
            seen = {}
            def fake_overlay(base, replacements, output, additions=None):
                seen.update(replacements); Path(output).write_bytes(b"overlaid")
            with patch("pascalpatch.store.inspect_game", return_value=SimpleNamespace(kind="iso", main_dol_sha1="x")), patch("pascalpatch.store.overlay_iso_files", side_effect=fake_overlay):
                BuildStore(root / "data").build(load_profile(root / "profiles/custom-roster.json", root))
            self.assertEqual(sorted(seen), sorted(n for c in slots.values() for n in (c + ".dat", c + "Nr.dat")))
            doc["characters"].append(dict(entries[0])); (root / "profiles/custom-roster.json").write_text(json.dumps(doc))
            with self.assertRaises((ManifestError, CompositionError)):
                BuildStore(root / "data").build(load_profile(root / "profiles/custom-roster.json", root))

    def test_quick_match_is_validated_and_staged_with_character_numbers(self):
        self.assertEqual(validate_quick_match({"p1": "falco", "p2": 9, "p2_player": "cpu7", "stage": "bf", "rules": "stock4"}), [])
        self.assertEqual(validate_quick_match({}), [])
        bad = validate_quick_match({"p1": "hulk", "p2": 26, "p2_player": "cpu0", "stage": "hyrule", "rules": "coins"})
        self.assertEqual(sorted(e.path for e in bad), ["quick_match.p1", "quick_match.p2", "quick_match.p2_player", "quick_match.rules", "quick_match.stage"])
        self.assertTrue(validate_quick_match({"p3": "fox"}))
        with tempfile.TemporaryDirectory() as td:
            root = Path(td); plugins = root / "plugins"; plugins.mkdir(); (plugins / "quick-match.dll").write_bytes(b"dll")
            with patch.dict("os.environ", {"PASCALPATCH_NATIVE_PLUGINS": str(plugins)}):
                entry = stage_quick_match({"p1": "falco", "p2": 9, "stage": "fd"}, root / "data", root / "mods")
            config = json.loads((root / "mods/quick-match.json").read_text())
            self.assertEqual(config["match"], {"p1": 20, "p2": 9, "stage": "fd"})
            self.assertEqual(entry["match"], config["match"])
            self.assertEqual((root / "mods/quick-match.dll").read_bytes(), b"dll")

    def test_portrait_stages_extra_fighters_with_name_and_photo(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            package, fighter = _package(root), _fighter(root)
            photo = root / "ember.portrait"; photo.write_bytes((bytes(range(256)) * 200)[:136 * 188 * 2])
            (root / "ember.portrait.json").write_text(json.dumps({"character": "ember", "output_sha256": hashlib.sha256(photo.read_bytes()).hexdigest()}))
            icon = root / "ember.icon"; icon.write_bytes(b"\x80\x00" * 64 * 56)
            (root / "ember.icon.json").write_text(json.dumps({"character": "ember", "output_sha256": hashlib.sha256(icon.read_bytes()).hexdigest()}))
            stock = root / "ember.stock"; stock.write_bytes(b"\x80\x00" * 24 * 24)
            (root / "ember.stock.json").write_text(json.dumps({"character": "ember", "output_sha256": hashlib.sha256(stock.read_bytes()).hexdigest()}))
            entry = {"package": str(package), "slot": "falco", "fighter_file": str(fighter), "portrait": str(photo), "icon": str(icon), "stock": str(stock)}
            plan = validate_character_entry(entry, "offline")
            self.assertEqual((plan["display_name"], plan["portrait"]), ("Ember", str(photo.resolve())))
            plugins = root / "plugins"; plugins.mkdir(); (plugins / "extra-fighters.dll").write_bytes(b"dll")
            with patch.dict("os.environ", {"PASCALPATCH_NATIVE_PLUGINS": str(plugins)}):
                report = stage_native_plugins([plan], root / "data", root / "mods")
            config = json.loads((root / "mods/extra-fighters.json").read_text())
            self.assertEqual(config["slots"], [{"id": "ember", "ckind": 20, "name": "Ember", "portrait": "ember.portrait", "icon": "ember.icon", "stock": "ember.stock"}])
            self.assertEqual((root / "mods/ember.icon").read_bytes(), icon.read_bytes())
            self.assertEqual((root / "mods/ember.portrait").read_bytes(), photo.read_bytes())
            self.assertEqual((report["plugins"][0]["portraits"], report["plugins"][0]["icons"], report["plugins"][0]["stocks"]), (1, 1, 1))
            photo.write_bytes(b"short")   # wrong size (and no longer the reported file)
            with self.assertRaises(CompositionError):
                validate_character_entry(entry, "offline")

    def test_new_fighter_gets_files_of_its_own(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            package, fighter = _package(root), _fighter(root)
            anim = root / "PlFcAJ.dat"; anim.write_bytes(b"\0" * 64)
            (root / "PlFcAJ.dat.anim.json").write_text(json.dumps({"character": "ember", "output_sha256": hashlib.sha256(anim.read_bytes()).hexdigest()}))
            graft = root / "move-graft.json"
            graft.write_text(json.dumps({"character": "ember", "grafts": [{"fighter": 22, "slot": "n", "donor": 1, "states": [341, 341]}]}))
            entry = {"package": str(package), "slot": "falco", "fighter_file": str(fighter), "animation_file": str(anim), "move_graft": str(graft), "install": "new"}
            plan = assign_new_fighters([validate_character_entry(entry, "offline")])[0]
            self.assertEqual((plan["base_kind"], plan["overlays"], sorted(plan["additions"])), (22, {}, ["PlX0.dat", "PlX0AJ.dat"]))
            self.assertEqual(plan["files"], {"dat": "PlX0.dat", "anim": "PlX0AJ.dat"})
            self.assertEqual(plan["grafts"][0]["character"], "ember")
            self.assertNotIn("fighter", plan["grafts"][0])
            plugins = root / "plugins"; plugins.mkdir()
            for name in ("extra-fighters", "move-graft"): (plugins / f"{name}.dll").write_bytes(b"dll")
            with patch.dict("os.environ", {"PASCALPATCH_NATIVE_PLUGINS": str(plugins)}):
                stage_native_plugins([plan], root / "data", root / "mods")
            config = json.loads((root / "mods/extra-fighters.json").read_text())
            self.assertEqual((config["slots"], config["fighters"]), ([], [{"id": "ember", "name": "Ember", "base": 22, "base_ckind": 20,
                                                                             "dat": "PlX0.dat", "anim": "PlX0AJ.dat"}]))
            self.assertEqual(json.loads((root / "mods/move-graft.json").read_text())["grafts"][0]["character"], "ember")
            second = assign_new_fighters([validate_character_entry(entry, "offline") for _ in range(2)])[1]
            self.assertEqual(second["files"]["dat"], "PlX1.dat")
            with self.assertRaises(CompositionError):
                validate_character_entry(dict(entry, slot="zelda"), "offline")

    def test_native_command_shape(self):
        self.assertEqual(native_command("melee_port.exe", "g.iso", ["--frames", 10]), ["melee_port.exe", "--iso", "g.iso", "--frames", "10"])
        # Through the launcher the port command is passed on untouched: no port-side flags are added.
        self.assertEqual(native_command("melee_port.exe", "g.iso", ["--frames", 10], mods="m", launcher="pascalpatch-launch.exe", sandbox="s", log="l"),
                         ["pascalpatch-launch.exe", "--mods", str(Path("m").resolve()), "--sandbox", str(Path("s").resolve()), "--log", str(Path("l").resolve()), "--", "melee_port.exe", "--iso", "g.iso", "--frames", "10"])


if __name__ == "__main__":
    unittest.main()
