import hashlib, json, sys, tempfile, unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))

from meleemod.errors import CompositionError, ManifestError
from meleemod.native import native_command, validate_character_entry
from meleemod.profile import load_profile
from meleemod.store import BuildStore


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
            def fake_overlay(base, replacements, output):
                seen.update(replacements); Path(output).write_bytes(b"overlaid")
            with patch("meleemod.store.inspect_game", return_value=SimpleNamespace(kind="iso", main_dol_sha1="x")), patch("meleemod.store.overlay_iso_files", side_effect=fake_overlay):
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
            def fake_overlay(base, replacements, output):
                seen.update(replacements); Path(output).write_bytes(b"overlaid")
            with patch("meleemod.store.inspect_game", return_value=SimpleNamespace(kind="iso", main_dol_sha1="x")), patch("meleemod.store.overlay_iso_files", side_effect=fake_overlay):
                BuildStore(root / "data").build(load_profile(root / "profiles/custom-roster.json", root))
            self.assertEqual(sorted(seen), sorted(n for c in slots.values() for n in (c + ".dat", c + "Nr.dat")))
            doc["characters"].append(dict(entries[0])); (root / "profiles/custom-roster.json").write_text(json.dumps(doc))
            with self.assertRaises((ManifestError, CompositionError)):
                BuildStore(root / "data").build(load_profile(root / "profiles/custom-roster.json", root))

    def test_native_command_shape(self):
        self.assertEqual(native_command("melee_port.exe", "g.iso", ["--frames", 10]), ["melee_port.exe", "--iso", "g.iso", "--frames", "10"])


if __name__ == "__main__":
    unittest.main()
