import json, tempfile, threading, unittest
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.request import urlopen

from pascalpatch.app.server import App, Handler
from pascalpatch.errors import PascalPatchError

REPO = Path(__file__).resolve().parents[2]


class AppServerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        Handler.app = App(REPO, self.tmp.name)
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        Handler.origin = f"http://127.0.0.1:{self.server.server_address[1]}"
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def tearDown(self):
        self.server.shutdown(); self.server.server_close(); self.tmp.cleanup()

    def get(self, path):
        with urlopen(Handler.origin + path, timeout=10) as r:
            return json.loads(r.read())

    def test_profile_mods_lists_the_catalog(self):
        body = self.get("/api/profile-mods?id=vanilla")
        self.assertTrue(body["catalog"], "the roadmap catalog is never empty")
        self.assertTrue(all(isinstance(e["id"], str) and "name" in e for e in body["catalog"]))
        self.assertIsInstance(body["enabled"], list)

    def test_built_in_plugins_use_their_plugin_json(self):
        folder = Path(self.tmp.name) / "native-plugins"; folder.mkdir()
        (folder / "frame-data.dll").write_bytes(b"MZ")
        manifest = json.loads((REPO / "plugins" / "frame-data" / "plugin.json").read_text(encoding="utf-8"))
        [row] = self.get("/api/plugins")["builtin"]
        self.assertEqual((row["name"], row["version"], row["summary"]),
                         (manifest["name"], manifest["version"], manifest["summary"]))

    def test_settings_refuse_the_source_port_and_name_the_right_exe(self):
        game = Path(self.tmp.name) / "MeleeUnlocked-0.8.0"; game.mkdir()
        for name in ("melee_port.exe", "melee_source.exe", "MeleeUnlockedLauncher.exe"):
            (game / name).write_bytes(b"MZ")
        app = Handler.app
        for wrong in ("melee_source.exe", "MeleeUnlockedLauncher.exe"):
            with self.assertRaisesRegex(PascalPatchError, "Pick melee_port.exe"):
                app.save_settings({"port": str(game / wrong)})
        self.assertEqual(app.save_settings({"port": str(game / "melee_port.exe")})["port"], str(game / "melee_port.exe"))
        self.assertEqual(self.get("/api/status")["melee_unlocked"], "0.8.0", "read from the unzipped release's folder name")


if __name__ == "__main__":
    unittest.main()
