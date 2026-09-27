import hashlib, io, json, sys, tempfile, unittest, zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "src"))
from pascalpatch.errors import ManifestError
from pascalpatch.plugin_store import PluginStore, validate_manifest
from pascalpatch.registry_signing import key_id, make_trust, public_key, sign_index

SITE = "https://plugins.example/"
SEED = bytes(range(32))


def _zip(files):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as z:
        for name, data in files.items():
            z.writestr(name, data)
    return buf.getvalue()


def _manifest(pid="frame-meter", version="1.0.0", **extra):
    m = {"id": pid, "name": "Frame Meter", "version": version, "abi": 1, "entry": f"{pid}.dll",
         "summary": "Shows frame advantage.", "settings": [
             {"key": "enabled_in_training", "type": "bool", "default": True, "label": "Training only"},
             {"key": "opacity", "type": "float", "default": 0.8, "min": 0, "max": 1},
             {"key": "position", "type": "choice", "default": "top", "options": ["top", "bottom"]}]}
    m.update(extra)
    return m


class Site:
    """A fake HTTPS plugin site: URLs under SITE map to an in-memory dict."""

    def __init__(self):
        self.files = {}

    def publish(self, packages, seed=SEED, catalog=None):
        entries = []
        for name, data in packages.items():
            self.files["releases/" + name] = data
            m = json.loads(zipfile.ZipFile(io.BytesIO(data)).read("plugin.json"))
            entries.append({"id": m["id"], "version": m["version"], "source": SITE + "releases/" + name,
                            "sha256": hashlib.sha256(data).hexdigest(), "license": "MIT",
                            "compatibility": "offline-only", "dependencies": [], "maintainer": "tester"})
        self.files["index.json"] = json.dumps(sign_index(entries, seed)).encode()
        self.files["catalog.json"] = json.dumps(catalog or {"plugins": [{"id": "frame-meter", "name": "Frame Meter", "tags": ["training"]}]}).encode()
        return entries

    def open(self, request, timeout=None):
        url = request.full_url if hasattr(request, "full_url") else request
        key = url[len(SITE):]
        if key not in self.files:
            raise OSError(f"404 {url}")
        data = self.files[key]

        class R(io.BytesIO):
            headers = {"Content-Length": str(len(data))}
            def __enter__(self): return self
            def __exit__(self, *a): return False
        return R(data)


def _trust(seed=SEED):
    pub = public_key(seed)
    return make_trust({key_id(pub): {"public_key": pub.hex(), "status": "trusted"}})


class PluginStoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.site = Site()
        self.store = PluginStore(self.tmp.name, SITE, trust=_trust(), opener=self.site.open)
        self.pkg = _zip({"plugin.json": json.dumps(_manifest()), "frame-meter.dll": b"MZ fake", "frame-meter.json": '{"font": 2}'})

    def test_refresh_install_settings_and_stage(self):
        self.site.publish({"frame-meter-1.0.0.zip": self.pkg})
        listing = self.store.refresh()
        self.assertEqual([p["id"] for p in listing["plugins"]], ["frame-meter"])
        self.assertEqual(listing["plugins"][0]["tags"], ["training"])
        self.assertIsNone(listing["plugins"][0]["installed"])
        self.store.install("frame-meter")
        [inst] = self.store.installed()
        self.assertTrue(inst["enabled"]); self.assertEqual(inst["version"], "1.0.0")
        self.assertEqual(inst["settings"], {"enabled_in_training": True, "opacity": 0.8, "position": "top"})
        self.store.set_settings("frame-meter", {"opacity": 0.5, "position": "bottom"})
        with self.assertRaises(ManifestError):
            self.store.set_settings("frame-meter", {"opacity": 3})
        with self.assertRaises(ManifestError):
            self.store.set_settings("frame-meter", {"nope": 1})
        mods = Path(self.tmp.name) / "run" / "mods"
        staged = self.store.stage(mods)
        self.assertEqual(staged, [{"id": "frame-meter", "version": "1.0.0"}])
        self.assertEqual((mods / "frame-meter.dll").read_bytes(), b"MZ fake")
        cfg = json.loads((mods / "frame-meter.json").read_text())
        self.assertEqual(cfg["font"], 2); self.assertEqual(cfg["settings"]["position"], "bottom")
        self.store.set_enabled("frame-meter", False)
        self.assertEqual(self.store.stage(Path(self.tmp.name) / "run2"), [])

    def test_update_replaces_old_version_and_keeps_settings(self):
        self.site.publish({"frame-meter-1.0.0.zip": self.pkg}); self.store.refresh(); self.store.install("frame-meter")
        self.store.set_settings("frame-meter", {"opacity": 0.25})
        v2 = _zip({"plugin.json": json.dumps(_manifest(version="1.1.0")), "frame-meter.dll": b"MZ v2"})
        self.site.publish({"frame-meter-1.0.0.zip": self.pkg, "frame-meter-1.1.0.zip": v2}); self.store.refresh()
        self.assertTrue(self.store.listing()["plugins"][0]["update"])
        self.store.install("frame-meter")
        self.assertEqual(sorted(p.name for p in (Path(self.tmp.name) / "plugins" / "frame-meter").iterdir()), ["1.1.0"])
        self.assertEqual(self.store.get_settings("frame-meter")["opacity"], 0.25)
        self.assertFalse(self.store.listing()["plugins"][0]["update"])

    def test_untrusted_or_tampered_index_is_refused(self):
        self.site.publish({"frame-meter-1.0.0.zip": self.pkg}, seed=bytes(32))   # signed by another key
        with self.assertRaises(ManifestError):
            self.store.refresh()
        self.assertEqual(self.store.listing()["plugins"], [])
        self.site.publish({"frame-meter-1.0.0.zip": self.pkg}); self.store.refresh()
        self.site.files["releases/frame-meter-1.0.0.zip"] = self.pkg + b"x"   # swapped after signing
        with self.assertRaises(ManifestError):
            self.store.install("frame-meter")
        self.assertEqual(self.store.installed(), [])

    def test_only_the_declared_dll_may_ship(self):
        sneaky = _zip({"plugin.json": json.dumps(_manifest()), "frame-meter.dll": b"MZ", "helper.exe": b"MZ"})
        self.site.publish({"frame-meter-1.0.0.zip": sneaky}); self.store.refresh()
        with self.assertRaises(ManifestError):
            self.store.install("frame-meter")
        self.assertEqual(self.store.installed(), [])
        # a package whose plugin.json disagrees with its signed entry
        self.assertTrue(validate_manifest(_manifest(version="9.9.9"), {"id": "frame-meter", "version": "1.0.0"}))

    def test_install_from_file(self):
        path = Path(self.tmp.name) / "dev.zip"; path.write_bytes(self.pkg)
        self.store.install_file(path)
        [inst] = self.store.installed(); self.assertEqual(inst["source"], "file")
        bad = Path(self.tmp.name) / "bad.zip"; bad.write_bytes(_zip({"plugin.json": json.dumps(_manifest()), "frame-meter.dll": b"MZ", "x.exe": b"MZ"}))
        with self.assertRaises(ManifestError):
            self.store.install_file(bad)
        self.store.uninstall("frame-meter")
        self.assertEqual(self.store.installed(), [])

    def test_manifest_validation(self):
        self.assertEqual(validate_manifest(_manifest()), [])
        self.assertTrue(validate_manifest(_manifest(abi=99)))
        self.assertTrue(validate_manifest(_manifest(entry="other.dll")))
        self.assertTrue(validate_manifest(_manifest(settings=[{"key": "x", "type": "choice"}])))

    def test_shipped_trust_store_is_valid(self):
        trust = json.loads((Path(__file__).parents[1] / "src/pascalpatch/trust.json").read_text())
        self.assertEqual(trust["schema"], "pascalpatch/trust/1")
        for kid, rec in trust["keys"].items():
            self.assertEqual(key_id(bytes.fromhex(rec["public_key"])), kid)


if __name__ == "__main__":
    unittest.main()
