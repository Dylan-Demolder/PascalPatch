import json, tempfile, threading, unittest
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.request import urlopen

from pascalpatch.app.server import App, Handler
from pascalpatch.errors import PascalPatchError
from pascalpatch.mods import create_profile
from pascalpatch.native import release_args

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
        create_profile(REPO, "test-catalog", "Test", "melee.iso")
        self.addCleanup((REPO / "profiles" / "test-catalog.json").unlink)
        body = self.get("/api/profile-mods?id=test-catalog")
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

    def test_a_release_launch_gets_the_release_folders_but_not_the_slippi_login(self):
        game = Path(self.tmp.name) / "MeleeUnlocked-0.8.0"; (game / "Sys").mkdir(parents=True)
        (game / "melee_port.exe").write_bytes(b"MZ")
        self.assertEqual(release_args(game / "melee_port.exe", self.tmp.name), [], "no Sys/codehandler.bin: a source build")
        (game / "Sys" / "codehandler.bin").write_bytes(b"\0")
        args = release_args(game / "melee_port.exe", self.tmp.name)
        flags = dict(zip(args[::2], args[1::2]))
        self.assertEqual(flags["--sys-dir"], str(game / "Sys"))
        self.assertEqual(flags["--settings-path"], str(game / "port-settings.ini"))
        self.assertEqual(Path(flags["--card-dir"]), Path(self.tmp.name).resolve() / "memory-card" / "CardA",
                         "PascalPatch's own memory card, never the player's real save")
        self.assertNotIn("--user-dir", flags)
        self.assertNotIn("--settings-path", release_args(game / "melee_port.exe", self.tmp.name, ["--settings-path=mine.ini"]))

    def test_the_tray_app_starts_the_server_and_quits_it(self):
        # PascalPatch.exe runs `app --tray`, reads the ready line and stops the server with /api/quit
        import os, subprocess, sys
        from urllib.error import HTTPError
        from urllib.request import Request
        with self.assertRaises(HTTPError, msg="only the tray app's server can be quit") as no:
            urlopen(Request(Handler.origin + "/api/quit", data=b"{}", method="POST"), timeout=10)
        self.assertEqual(no.exception.code, 404)
        env = dict(os.environ, PYTHONPATH=str(REPO / "host" / "src"), PYTHONUNBUFFERED="1")
        proc = subprocess.Popen([sys.executable, "-m", "pascalpatch.cli", "--root", self.tmp.name, "--data", self.tmp.name, "app", "--tray"],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
        self.addCleanup(lambda: proc.poll() is None and proc.kill())
        tag, _, rest = proc.stdout.readline().partition(" ")
        self.assertEqual(tag, "PASCALPATCH_READY")
        ready = json.loads(rest)
        self.assertEqual(Path(ready["data"]), Path(self.tmp.name).resolve())
        with urlopen(ready["url"] + "api/status", timeout=10) as r:
            self.assertIn("version", json.loads(r.read()))
        with urlopen(Request(ready["url"] + "api/quit", data=b"{}", method="POST"), timeout=10) as r:
            self.assertEqual(json.loads(r.read()), {"ok": True})
        self.assertEqual(proc.wait(timeout=15), 0)
        proc.stdout.close()

    def test_a_new_install_is_told_to_make_a_profile(self):
        root = Path(self.tmp.name) / "install"; root.mkdir()
        problems = App(root, Path(self.tmp.name) / "data").status()["problems"]
        self.assertTrue(any("Make a profile" in p for p in problems), problems)

    def test_the_studio_button_opens_the_bundled_studio_with_a_known_disc(self):
        import subprocess
        from unittest import mock
        from pascalpatch.app import server
        root = Path(self.tmp.name) / "install"; (root / "profiles").mkdir(parents=True)
        studio = Path(self.tmp.name) / "studio"; (studio / "core" / "src").mkdir(parents=True)
        disc = Path(self.tmp.name) / "melee.iso"; disc.write_bytes(b"GALE01")
        (root / "profiles" / "a.json").write_text(json.dumps({"base_game": str(Path(self.tmp.name) / "gone.iso")}))
        (root / "profiles" / "b.json").write_text(json.dumps({"base_game": str(disc)}))
        app = App(root, Path(self.tmp.name) / "data")
        with mock.patch.object(server, "BUNDLED_STUDIO", studio), mock.patch.object(subprocess, "Popen") as popen:
            self.assertEqual(app.studio_folder(), studio)
            app.open_studio()
        cmd = popen.call_args.args[0]; kw = popen.call_args.kwargs
        self.assertEqual(cmd[1:4], ["-m", "melee_character_studio.cli", "app"])
        self.assertEqual(cmd[cmd.index("--pascalpatch-iso") + 1], str(disc.resolve()))   # the disc that exists
        self.assertEqual((kw["cwd"], kw["env"]["PYTHONPATH"]), (studio, str(studio / "core" / "src")))
        with mock.patch.object(server, "BUNDLED_STUDIO", Path(self.tmp.name) / "none"):
            self.assertIsNone(app.studio_folder())
            with self.assertRaisesRegex(PascalPatchError, "Character Studio folder"):
                app.open_studio()


if __name__ == "__main__":
    unittest.main()
