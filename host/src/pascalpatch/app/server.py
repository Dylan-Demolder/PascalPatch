"""The PascalPatch desktop app: a local web UI served on 127.0.0.1 and shown in its own window.

Everything the app does goes through the same code as the CLI: profiles (GuiController and
mods.py), builds and launches (``python -m pascalpatch.cli`` in a background job, so the window
never blocks), and plugins (PluginStore). The server binds to loopback only and refuses requests
whose Host or Origin is not the app's own, so a web page open in a browser cannot drive it.
"""
from __future__ import annotations

import base64
import datetime
import functools
import json
import os
import re
import subprocess
import sys
import tempfile
import threading
import time
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlparse

from .. import __version__
from ..errors import PascalPatchError
from ..gui import GuiController
from ..mods import CatalogError, create_profile, delete_profile, set_enabled
from ..native import BUNDLED_BIN, PORT_NAMES, check_melee_port, find_melee_port, find_pascalpatch_launcher
from ..plugin_store import SITE_URL, PluginStore

WEB = Path(__file__).with_name("web")
TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8",
         ".svg": "image/svg+xml", ".png": "image/png", ".ico": "image/x-icon", ".json": "application/json"}
# Built-in plugins that Play loads by itself when a profile needs them (characters, unlocks, a
# quick_match); every other DLL in native-plugins is a local build, loaded only once installed.
PROFILE_PLUGINS = ("unlock-all", "extra-fighters", "move-graft", "quick-match")
FIRST_PARTY = {
    "unlock-all": "Every character and stage selectable, offline. Stage bits are only opened on the select screens, so no unlock notices.",
    "extra-fighters": "New fighters on the character select screen (paged), with their portraits, grid icons and stock icons.",
    "move-graft": "Gives a fighter another fighter's special moves, running the donor's real game code.",
}


@functools.lru_cache(maxsize=256)
def _package_character(path, mtime):
    """(display name, id) from a .melee-character package's character.json."""
    try:
        with zipfile.ZipFile(path) as z:
            c = json.loads(z.read("character.json"))
        return c.get("display_name") or c.get("name"), c.get("id")
    except (OSError, KeyError, ValueError, zipfile.BadZipFile):
        return None, None


def _character_name(entry):
    pkg = Path(entry.get("package", ""))
    name = entry.get("display_name")
    if not name and pkg.is_file():
        name = _package_character(str(pkg), pkg.stat().st_mtime)[0]
    return name or entry.get("character") or pkg.stem.replace("-", " ").title()


class Job:
    def __init__(self, jid, title, cmd, cwd, env):
        self.id, self.title, self.cmd = jid, title, cmd
        self.lines, self.exit, self.started, self.ended = [], None, time.time(), None
        self.proc = subprocess.Popen(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                     text=True, encoding="utf-8", errors="replace",
                                     creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        threading.Thread(target=self._pump, daemon=True).start()

    def _pump(self):
        for line in self.proc.stdout:
            self.lines.append(line.rstrip("\n"))
            del self.lines[:-400]
        self.exit = self.proc.wait(); self.ended = time.time()

    def as_dict(self, full=False):
        d = {"id": self.id, "title": self.title, "exit": self.exit, "running": self.exit is None,
             "started": self.started, "ended": self.ended, "tail": self.lines[-1] if self.lines else ""}
        if full:
            d["lines"] = self.lines
        return d


class App:
    def __init__(self, root, data):
        self.root = Path(root).expanduser().resolve()
        self.data = Path(data).expanduser().resolve()
        self.gui = GuiController(self.root, self.data)
        self.settings_file = self.data / "app.json"
        self.jobs, self._next = {}, 1

    # ---- settings ----
    def settings(self):
        try:
            s = json.loads(self.settings_file.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            s = {}
        s.setdefault("site", SITE_URL); s.setdefault("theme", "dark")
        s.setdefault("port", os.environ.get("MELEE_PORT", "")); s.setdefault("port_cwd", "")
        s.setdefault("studio_repo", ""); s.setdefault("port_args", ["--fps", "unlocked"])
        return s

    def save_settings(self, changes):
        s = self.settings()
        for k in ("site", "theme", "port", "port_cwd", "studio_repo", "port_args"):
            if k in changes:
                s[k] = changes[k]
        if s["port"] and not Path(s["port"]).is_file():
            raise PascalPatchError(f"melee_port.exe not found at {s['port']}")
        if s["port"]:
            check_melee_port(s["port"])
        if not s["site"].startswith("https://"):
            raise PascalPatchError("the plugin site must be an https:// address")
        self.data.mkdir(parents=True, exist_ok=True)
        self.settings_file.write_text(json.dumps(s, indent=1) + "\n", encoding="utf-8")
        return s

    def plugins(self):
        return PluginStore(self.data, self.settings()["site"])

    # ---- status ----
    def status(self):
        s = self.settings(); port = launcher = None; problems = []
        try:
            port = str(find_melee_port(s["port"] or None))
        except PascalPatchError as exc:
            problems.append(str(exc) if s["port"] and Path(s["port"]).is_file() else "melee_port.exe not found: set it in Settings.")
        try:
            launcher = str(find_pascalpatch_launcher(self.data))
        except PascalPatchError:
            problems.append("The PascalPatch runtime is not built: run tooling/native/build_plugins.py.")
        mu_version = None
        if port:
            for parent in Path(port).parents:
                if (parent / "VERSION").is_file():   # a source checkout
                    mu_version = (parent / "VERSION").read_text(encoding="utf-8").strip(); break
                m = re.fullmatch(r"MeleeUnlocked-(\d+(?:\.\d+)+)", parent.name)   # an unzipped release
                if m:
                    mu_version = m.group(1); break
        profiles = self.gui.list_profiles()
        if not profiles:
            problems.append("Make a profile: it only needs your Melee disc (NTSC 1.02 ISO). Open Profiles, then New profile.")
        for row in profiles:
            try:
                disc = json.loads((self.root / "profiles" / f"{row.id}.json").read_text(encoding="utf-8")).get("base_game", "")
            except (OSError, ValueError):
                continue
            if disc and not Path(disc).is_file():
                problems.append(f"The profile {row.name} cannot find its Melee disc ({disc}). Open Profiles to delete it, "
                                "or make a new profile with your disc.")
        return {"version": __version__, "root": str(self.root), "data": str(self.data), "port": port,
                "melee_unlocked": mu_version, "launcher": launcher, "profiles": len(profiles),
                "plugins": len({p["id"] for p in self.plugins().installed()} | {p["id"] for p in self.builtin()}), "problems": problems,
                "running": [j.as_dict() for j in self.jobs.values() if j.exit is None]}

    def builtin(self):
        dlls = {}
        for folder in (self.data / "native-plugins", BUNDLED_BIN):   # a release's bin/ wins over older local builds
            for dll in folder.glob("*.dll") if folder.is_dir() else []:
                dlls[dll.stem] = dll
        out = []
        for dll in (dlls[k] for k in sorted(dlls)):
            if dll.stem.endswith("_runtime"):
                continue
            m = {}
            for base in (Path(__file__).resolve().parents[4], self.root):   # this checkout's plugins/<id>/plugin.json
                try:
                    m = json.loads((base / "plugins" / dll.stem / "plugin.json").read_text(encoding="utf-8")); break
                except (OSError, ValueError):
                    pass
            out.append({"id": dll.stem, "name": m.get("name") or dll.stem.replace("-", " ").title(),
                        "summary": m.get("summary") or FIRST_PARTY.get(dll.stem, "Built from this PascalPatch checkout."),
                        "source": "built-in", "profile_plugin": dll.stem in PROFILE_PLUGINS, "enabled": True, "version": m.get("version") or __version__, "settings_schema": [], "settings": {},
                        "modified": datetime.datetime.fromtimestamp(dll.stat().st_mtime).isoformat(timespec="minutes")})
        return out

    # ---- profiles ----
    def profiles(self):
        out = []
        for row in self.gui.list_profiles():
            path = self.root / "profiles" / f"{row.id}.json"
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                data = {}
            chars = data.get("characters", [])
            current = self.data / "builds" / row.id / "current"
            built = None
            if current.exists():
                built = datetime.datetime.fromtimestamp(current.stat().st_mtime).isoformat(timespec="minutes")
            out.append({"id": row.id, "name": row.name, "mode": row.mode, "compatibility": row.compatibility,
                        "characters": [{"id": Path(c.get("package", "")).stem, "name": _character_name(c),
                                        "slot": c.get("slot"), "install": c.get("install", "replace")} for c in chars],
                        "unlock_all": bool(data.get("unlock_all")), "mods": len(data.get("mods", [])),
                        "plugins": len(data.get("plugins", [])), "built": built, "description": data.get("description", "")})
        return out

    def job(self, title, args):
        s = self.settings()
        cmd = [sys.executable, "-m", "pascalpatch.cli", "--root", str(self.root), "--data", str(self.data)] + args
        env = dict(os.environ, PYTHONPATH=str(Path(__file__).resolve().parents[2]), PYTHONIOENCODING="utf-8")
        j = Job(self._next, title, cmd, str(self.root), env); self.jobs[j.id] = j; self._next += 1
        return j.as_dict()

    def build(self, pid):
        return self.job(f"Build {pid}", ["build", pid])

    def launch(self, pid, rebuild=True):
        s = self.settings()
        args = ["launch", pid, "--runtime", "native", "--allow-unsafe"]
        if not rebuild:
            args.append("--no-build")
        if s["port"]:
            args += ["--port", s["port"]]
        cwd = s["port_cwd"] or (str(Path(s["port"]).parent) if s["port"] else "")
        if cwd:
            args += ["--port-cwd", cwd]
        for a in s.get("port_args", []):
            args.append(f"--port-arg={a}")
        return self.job(f"Play {pid}", args)

    def logs(self):
        out = []
        base = self.data / "logs"
        for f in sorted(base.glob("*/*.log"), key=lambda p: p.stat().st_mtime, reverse=True)[:60] if base.is_dir() else []:
            out.append({"profile": f.parent.name, "name": f.name, "size": f.stat().st_size,
                        "modified": datetime.datetime.fromtimestamp(f.stat().st_mtime).isoformat(timespec="seconds")})
        return out

    def log_text(self, profile, name):
        f = (self.data / "logs" / profile / name).resolve()
        if (self.data / "logs").resolve() not in f.parents or f.suffix != ".log":
            raise PascalPatchError("not a PascalPatch log")
        raw = f.read_bytes()[-200_000:]
        return raw.decode("utf-8", "replace")

    def pick(self, kind, title, start=""):
        """A native Open dialog on this PC, for the path fields: returns {"path": ""} when cancelled."""
        if kind not in ("exe", "iso", "zip", "folder"):
            raise PascalPatchError("unknown kind of file")
        start = str(Path(start).parent if start and Path(start).is_file() else start or "")
        r = subprocess.run([sys.executable, str(Path(__file__).with_name("picker.py")), kind, title, start],
                           capture_output=True, text=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        return {"path": str(Path(r.stdout.strip())) if r.stdout.strip() else ""}

    def open_studio(self):
        repo = self.settings()["studio_repo"]
        if not repo or not Path(repo).is_dir():
            raise PascalPatchError("set the Character Studio folder in Settings first")
        env = dict(os.environ, PYTHONPATH=str(Path(repo) / "core" / "src"))
        s = self.settings()
        # the studio builds rosters into this PascalPatch and launches the game through it
        args = ["app", "--pascalpatch-repo", str(Path(__file__).resolve().parents[4]), "--pascalpatch-root", str(self.root),
                "--pascalpatch-data", str(self.data)]
        if s["port"]: args += ["--port", s["port"]]
        if s["port_cwd"]: args += ["--port-cwd", s["port_cwd"]]
        subprocess.Popen([sys.executable, "-m", "melee_character_studio.cli", *args], cwd=repo, env=env,
                         creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        return {"ok": True}


class Handler(BaseHTTPRequestHandler):
    app: App = None
    origin = ""

    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype="application/json"):
        raw = body if isinstance(body, bytes) else json.dumps(body).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store"); self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'")
        self.end_headers(); self.wfile.write(raw)

    def _trusted(self):
        host = self.headers.get("Host", "")
        origin = self.headers.get("Origin")
        return host == self.origin.split("//", 1)[1] and (origin is None or origin == self.origin)

    def _body(self):
        n = int(self.headers.get("Content-Length") or 0)
        if n > 64 * 1024 * 1024:
            raise PascalPatchError("request too large")
        return json.loads(self.rfile.read(n) or b"{}")

    def do_GET(self):
        if not self._trusted():
            return self._send(403, {"error": "forbidden"})
        url = urlparse(self.path); p = url.path; q = parse_qs(url.query); a = self.app
        try:
            if p == "/api/status": return self._send(200, a.status())
            if p == "/api/profiles": return self._send(200, a.profiles())
            if p == "/api/profile-mods":
                roadmap, enabled, missing = a.gui.profile_mods(q["id"][0])
                keys = ("id", "name", "kind", "version", "online_safe", "description", "status", "category", "tags")
                return self._send(200, {"catalog": [{k: (list(v) if isinstance(v, tuple) else v) for k in keys for v in [getattr(e, k)]} for e in roadmap.values()],
                                        "enabled": sorted(enabled), "missing": sorted(missing)})
            if p == "/api/plugins":
                return self._send(200, {"installed": a.plugins().installed(), "builtin": a.builtin()})
            if p == "/api/store": return self._send(200, a.plugins().listing())
            if p == "/api/jobs": return self._send(200, [j.as_dict() for j in sorted(a.jobs.values(), key=lambda j: -j.id)])
            if p.startswith("/api/jobs/"): return self._send(200, a.jobs[int(p.rsplit("/", 1)[1])].as_dict(full=True))
            if p == "/api/logs": return self._send(200, a.logs())
            if p == "/api/log": return self._send(200, {"text": a.log_text(q["profile"][0], q["name"][0])})
            if p == "/api/settings": return self._send(200, a.settings())
            name = "index.html" if p in ("/", "") else unquote(p).lstrip("/")
            f = (WEB / name).resolve()
            if WEB.resolve() in f.parents and f.is_file():
                return self._send(200, f.read_bytes(), TYPES.get(f.suffix, "application/octet-stream"))
            return self._send(404, {"error": "not found"})
        except (PascalPatchError, CatalogError, KeyError, ValueError, OSError) as exc:
            return self._send(400, {"error": str(exc) or exc.__class__.__name__})

    def do_POST(self):
        if not self._trusted():
            return self._send(403, {"error": "forbidden"})
        p = urlparse(self.path).path; a = self.app
        try:
            b = self._body()
            if p == "/api/build": return self._send(200, a.build(b["id"]))
            if p == "/api/launch": return self._send(200, a.launch(b["id"], b.get("rebuild", True)))
            if p == "/api/profile-mod":
                set_enabled(a.root, b["profile"], b["mod"], bool(b["enabled"])); return self._send(200, {"ok": True})
            if p == "/api/profile/create":
                create_profile(a.root, b["id"], b["name"], b["iso"], mode="offline"); return self._send(200, {"ok": True})
            if p == "/api/profile/delete":
                delete_profile(a.root, b["id"]); return self._send(200, {"ok": True})
            if p == "/api/store/refresh": return self._send(200, a.plugins().refresh())
            if p == "/api/store/install":
                a.plugins().install(b["id"]); return self._send(200, a.plugins().listing())
            if p == "/api/plugins/upload":
                raw = base64.b64decode(b["data"])
                with tempfile.TemporaryDirectory() as t:
                    z = Path(t) / "plugin.zip"; z.write_bytes(raw); a.plugins().install_file(z)
                return self._send(200, {"ok": True})
            if p == "/api/plugins/enable":
                a.plugins().set_enabled(b["id"], b["enabled"]); return self._send(200, {"ok": True})
            if p == "/api/plugins/settings":
                return self._send(200, a.plugins().set_settings(b["id"], b["values"]))
            if p == "/api/plugins/uninstall":
                a.plugins().uninstall(b["id"]); return self._send(200, {"ok": True})
            if p == "/api/settings": return self._send(200, a.save_settings(b))
            if p == "/api/studio": return self._send(200, a.open_studio())
            if p == "/api/pick": return self._send(200, a.pick(b.get("kind", ""), b.get("title", "Choose a file"), b.get("start", "")))
            return self._send(404, {"error": "not found"})
        except (PascalPatchError, CatalogError, KeyError, ValueError, OSError) as exc:
            msg = str(exc) or exc.__class__.__name__
            errs = getattr(exc, "errors", ())
            if errs:
                msg += ": " + "; ".join(f"{e.path} {e.message}" for e in errs)
            return self._send(400, {"error": msg})


def serve(root, data, port=0, window=True):
    Handler.app = App(root, data)
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    Handler.origin = f"http://127.0.0.1:{server.server_address[1]}"
    print(f"PascalPatch {__version__}: {Handler.origin}/  (Ctrl+C to stop)", flush=True)
    if window:
        from .window import open_window
        threading.Thread(target=server.serve_forever, daemon=True).start()
        open_window(Handler.origin + "/", "PascalPatch", Handler.app.data)
        server.shutdown()
    else:
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
    return 0
