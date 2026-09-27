import json, tempfile, unittest
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parents[1] / "src"))
from pascalpatch.fighter_loader import (
    FighterLoaderError, clone_slot_basename, fighter_loader_plugin_source,
    find_clone_slot, symbol_for_character_id, validate_fighter_symbol,
    write_fighter_loader_plugin,
)
from pascalpatch.iso_files import (
    extract_iso_file, list_iso_files, overlay_iso_files, recompose_iso_with_fighter,
)


def _make_iso(path):
    names = b"\x00files\x00PlMr.dat\x00PlCo.dat\x00boot.bin\x00"
    entries = [
        (1, 0, 0, 5),
        (1, 1, 0, 4),
        (0, 7, 0x980, 64),
        (0, 16, 0x9C0, 32),
        (0, 25, 0x9E0, 16),
    ]
    fst = bytearray()
    for flag, name_off, offset, size in entries:
        fst += bytes([flag]) + name_off.to_bytes(3, "big")
        fst += offset.to_bytes(4, "big") + size.to_bytes(4, "big")
    fst += names
    raw = bytearray(0x9F0)
    raw[0x420:0x424] = (0x500).to_bytes(4, "big")
    raw[0x424:0x428] = (0x900).to_bytes(4, "big")
    raw[0x428:0x42C] = len(fst).to_bytes(4, "big")
    raw[0x500:0x900] = b"D" * 0x400
    raw[0x900:0x900 + len(fst)] = fst
    raw[0x980:0x9C0] = b"M" * 64
    raw[0x9C0:0x9E0] = b"C" * 32
    raw[0x9E0:0x9F0] = b"B" * 16
    Path(path).write_bytes(bytes(raw))
    return path


class IsoFilesTests(unittest.TestCase):
    def test_listing_and_extraction(self):
        with tempfile.TemporaryDirectory() as td:
            base = _make_iso(Path(td) / "base.iso")
            files = list_iso_files(base)
            self.assertEqual([f["path"] for f in files],
                             ["files/PlMr.dat", "files/PlCo.dat", "boot.bin"])
            self.assertEqual(extract_iso_file(base, "files/PlMr.dat"), b"M" * 64)
            self.assertEqual(extract_iso_file(base, "/boot.bin"), b"B" * 16)
            with self.assertRaises(FileNotFoundError):
                extract_iso_file(base, "files/missing.dat")
            with self.assertRaises(ValueError):
                extract_iso_file(base, "../escape.dat")

    def test_overlay_same_size_replacement(self):
        with tempfile.TemporaryDirectory() as td:
            t = Path(td)
            base = _make_iso(t / "base.iso")
            before = Path(base).read_bytes()
            out = t / "out.iso"
            overlay_iso_files(base, {"files/PlMr.dat": b"N" * 64}, out)
            self.assertEqual(Path(base).read_bytes(), before)
            self.assertEqual(extract_iso_file(out, "files/PlMr.dat"), b"N" * 64)
            self.assertEqual(extract_iso_file(out, "files/PlCo.dat"), b"C" * 32)
            self.assertEqual(extract_iso_file(out, "boot.bin"), b"B" * 16)
            self.assertEqual(Path(out).read_bytes()[0x500:0x900], b"D" * 0x400)
            self.assertGreaterEqual(Path(out).stat().st_size, Path(base).stat().st_size)
            self.assertEqual(Path(out).stat().st_size % 0x8000, 0)
            with self.assertRaises(FileNotFoundError):
                overlay_iso_files(base, {"files/Nope.dat": b"x"}, t / "bad.iso")
            with self.assertRaises(ValueError):
                overlay_iso_files(base, {"../escape.dat": b"x"}, t / "bad.iso")
            with self.assertRaises(FileNotFoundError):
                overlay_iso_files(base, {"files": b"x"}, t / "bad.iso")

    def test_overlay_larger_replacement_shifts_later_files(self):
        with tempfile.TemporaryDirectory() as td:
            t = Path(td)
            base = _make_iso(t / "base.iso")
            out = t / "out.iso"
            overlay_iso_files(base, {"files/PlMr.dat": b"Q" * 200}, out)
            self.assertEqual(extract_iso_file(out, "files/PlMr.dat"), b"Q" * 200)
            self.assertEqual(extract_iso_file(out, "files/PlCo.dat"), b"C" * 32)
            self.assertEqual(extract_iso_file(out, "boot.bin"), b"B" * 16)
            for entry in list_iso_files(out):
                self.assertEqual(entry["offset"] % 0x20, 0)
            self.assertEqual(Path(out).read_bytes()[0x500:0x900], b"D" * 0x400)

    def test_overlay_adds_new_root_files(self):
        with tempfile.TemporaryDirectory() as td:
            t = Path(td)
            base = _make_iso(t / "base.iso")
            out = t / "out.iso"
            overlay_iso_files(base, {"files/PlMr.dat": b"N" * 64}, out, {"PlX0.dat": b"X" * 100, "PlX0Nr.dat": b"Y" * 7})
            self.assertEqual([f["path"] for f in list_iso_files(out)],
                             ["files/PlMr.dat", "files/PlCo.dat", "boot.bin", "PlX0.dat", "PlX0Nr.dat"])
            self.assertEqual(extract_iso_file(out, "PlX0.dat"), b"X" * 100)
            self.assertEqual(extract_iso_file(out, "PlX0Nr.dat"), b"Y" * 7)
            self.assertEqual(extract_iso_file(out, "files/PlMr.dat"), b"N" * 64)
            self.assertEqual(extract_iso_file(out, "boot.bin"), b"B" * 16)
            head = Path(out).read_bytes()[0x428:0x430]
            size, most = int.from_bytes(head[:4], "big"), int.from_bytes(head[4:], "big")
            self.assertEqual(size, 7 * 12 + 34 + len(b"PlX0.dat\0PlX0Nr.dat\0"))
            self.assertGreaterEqual(most, size)
            with self.assertRaises(ValueError):
                overlay_iso_files(base, {}, t / "bad.iso", {"boot.bin": b"x"})
            with self.assertRaises(ValueError):
                overlay_iso_files(base, {}, t / "bad.iso", {"files/new.dat": b"x"})

    def test_recompose_with_fighter_end_to_end(self):
        with tempfile.TemporaryDirectory() as td:
            t = Path(td)
            base = _make_iso(t / "base.iso")
            dol = t / "new.dol"
            dol.write_bytes(b"E" * 0x440)
            out = t / "out.iso"
            recompose_iso_with_fighter(base, dol, out, {"files/PlMr.dat": b"Z" * 100})
            self.assertEqual(extract_iso_file(out, "files/PlMr.dat"), b"Z" * 100)
            self.assertEqual(extract_iso_file(out, "files/PlCo.dat"), b"C" * 32)
            data = Path(out).read_bytes()
            self.assertEqual(data[0x500:0x940], b"E" * 0x440)
            self.assertEqual(Path(base).read_bytes()[0x500:0x900], b"D" * 0x400)


class FighterLoaderTests(unittest.TestCase):
    def test_symbol_derivation_and_validation(self):
        self.assertEqual(symbol_for_character_id("leesin-hsd-2e"), "ftDataLeesinhsd2e")
        self.assertEqual(validate_fighter_symbol("ftDataLeesinhsd2e"), "ftDataLeesinhsd2e")
        with self.assertRaises(FighterLoaderError):
            symbol_for_character_id("---")
        with self.assertRaises(FighterLoaderError):
            validate_fighter_symbol("ftData Bad!")
        with self.assertRaises(FighterLoaderError):
            validate_fighter_symbol("notAFighterSymbol")

    def test_clone_slot_resolution(self):
        self.assertEqual(clone_slot_basename("mario"), "PlMr.dat")
        self.assertEqual(clone_slot_basename("Captain-Falcon"), "PlCa.dat")
        with self.assertRaises(FighterLoaderError):
            clone_slot_basename("waluigi")
        listing = [{"path": "files/PlMr.dat", "offset": 0x380, "size": 64},
                   {"path": "files/PlCo.dat", "offset": 0x3C0, "size": 32}]
        self.assertEqual(find_clone_slot(listing, "mario"), "files/PlMr.dat")
        with self.assertRaises(FighterLoaderError):
            find_clone_slot(listing, "fox")
        ambiguous = listing + [{"path": "extra/PlMr.dat", "offset": 0x400, "size": 8}]
        with self.assertRaises(FighterLoaderError):
            find_clone_slot(ambiguous, "mario")

    def test_clone_slot_codes_match_ntsc_102_disc(self):
        # Codes that differ from the fighter name on GALE01.
        self.assertEqual(clone_slot_basename("bowser"), "PlKp.dat")
        self.assertEqual(clone_slot_basename("ice-climbers"), "PlPp.dat")
        self.assertEqual(clone_slot_basename("pichu"), "PlPc.dat")

    def test_loader_plugin_source(self):
        source = fighter_loader_plugin_source("ftDataLeesinhsd2e", "CUSTOM_FIGHTER_VISIBLE")
        self.assertIn("ftDataLeesinhsd2e", source)
        self.assertIn("CUSTOM_FIGHTER_VISIBLE", source)
        self.assertIn("pascalpatch_fighter_loader_init", source)
        plain = fighter_loader_plugin_source("ftDataLeesinhsd2e")
        self.assertIn("ftDataLeesinhsd2e", plain)
        self.assertNotIn("observation", plain)
        self.assertEqual(fighter_loader_plugin_source("ftDataLeesinhsd2e"),
                         fighter_loader_plugin_source("ftDataLeesinhsd2e"))
        with self.assertRaises(FighterLoaderError):
            fighter_loader_plugin_source("bogus", "CUSTOM_FIGHTER_VISIBLE")
        with self.assertRaises(FighterLoaderError):
            fighter_loader_plugin_source("ftDataLeesinhsd2e", "lowercase-bad")
        with self.assertRaises(FighterLoaderError):
            fighter_loader_plugin_source("ftDataLeesinhsd2e", None, entrypoint="9bad")

    def test_write_loader_plugin(self):
        with tempfile.TemporaryDirectory() as td:
            target = write_fighter_loader_plugin("ftDataLeesinhsd2e", Path(td) / "gen",
                                                 "CUSTOM_FIGHTER_VISIBLE")
            self.assertTrue(target.is_file())
            text = target.read_text()
            self.assertIn("ftDataLeesinhsd2e", text)
            with self.assertRaises(FighterLoaderError):
                write_fighter_loader_plugin("ftDataLeesinhsd2e", td, filename="../escape.c")


if __name__ == "__main__":
    unittest.main()
