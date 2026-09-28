"""Downloadable native plugins: browse, install, enable, configure and stage them for a launch.

The plugin website (GitHub Pages) publishes two files:

* ``index.json``: the signed registry index (``registry_signing``). It is the only thing that
  can make PascalPatch download and install code: every entry's ZIP is hash-checked while it
  streams, and the signature is checked against the trust store before any entry is used.
* ``catalog.json``: display data for the browser (names, summaries, tags, changelogs). It is
  never trusted for installs; an entry missing from the signed index cannot be installed.

A plugin package is a ZIP with ``plugin.json`` (the manifest below), ``<id>.dll`` and
optionally ``<id>.json`` (its default config) and a README. Packages install to
``<data>/plugins/<id>/<version>/``; user settings live in ``<data>/plugin-settings/<id>.json``
so they survive updates. At launch ``stage`` copies every enabled plugin into the run's mods
folder, and the injected runtime loads them like the first-party ones. The runtime itself stays
offline: only this module (driven from the desktop app) talks to the network, and only when
asked to.
"""
from __future__ import annotations

import hashlib
import json
import re
import shutil
import time
import zipfile
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import Request, urlopen

from .errors import ManifestError, ValidationError
from .manifest import ID_RE
from .registry import install_remote_archive
from .registry_signing import verify_index

SITE_URL = "https://dylan-demolder.github.io/pascalpatch-plugins/"
PLUGIN_ABI = 1                 # the newest runtime ABI this PascalPatch serves (sdk/include/pascalpatch/plugin.h)
SETTING_TYPES = ("bool", "int", "float", "choice", "text", "key")
TRUST_FILE = Path(__file__).with_name("trust.json")


def _read_json(path, default=None):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return default


def _write_json(path, value):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(value, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(path)


def validate_manifest(m, entry=None):
    """Errors in a plugin.json (and its match with the signed index entry, when given)."""
    e = []
    if not isinstance(m, dict):
        return [ValidationError("plugin.json", "type", "expected an object")]
    for k in ("id", "name", "version", "abi", "entry", "summary"):
        if k not in m:
            e.append(ValidationError(f"plugin.json.{k}", "required", "missing field"))
    pid = m.get("id")
    if pid is not None and (not isinstance(pid, str) or not ID_RE.fullmatch(pid)):
        e.append(ValidationError("plugin.json.id", "id", "must be a lowercase identifier"))
    if "entry" in m and m["entry"] != f"{pid}.dll":
        e.append(ValidationError("plugin.json.entry", "entry", "must be <id>.dll"))
    if "abi" in m and (not isinstance(m["abi"], int) or not 1 <= m["abi"] <= PLUGIN_ABI):
        e.append(ValidationError("plugin.json.abi", "abi", f"needs runtime ABI {m['abi']}; this PascalPatch serves up to {PLUGIN_ABI}"))
    if "min_runtime" in m and not (isinstance(m["min_runtime"], str) and re.fullmatch(r"\d+\.\d+(\.\d+)?", m["min_runtime"])):
        e.append(ValidationError("plugin.json.min_runtime", "version", "must be a version like \"0.3\""))
    for i, s in enumerate(m.get("settings", [])):
        where = f"plugin.json.settings[{i}]"
        if not isinstance(s, dict) or not isinstance(s.get("key"), str) or s.get("type") not in SETTING_TYPES:
            e.append(ValidationError(where, "setting", f"needs a key and a type in {SETTING_TYPES}")); continue
        if s["type"] == "choice" and not s.get("options"):
            e.append(ValidationError(where, "options", "a choice needs options"))
    if entry is not None:
        for k in ("id", "version"):
            if m.get(k) != entry.get(k):
                e.append(ValidationError(f"plugin.json.{k}", "mismatch", f"does not match the signed index ({entry.get(k)!r})"))
    return e


def coerce_setting(spec, value):
    """A setting value checked against its manifest spec; raises ValueError when it does not fit."""
    t = spec["type"]
    if t == "bool":
        if isinstance(value, bool): return value
        raise ValueError(f"{spec['key']} must be true or false")
    if t in ("int", "float"):
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise ValueError(f"{spec['key']} must be a number")
        v = int(value) if t == "int" else float(value)
        if "min" in spec and v < spec["min"] or "max" in spec and v > spec["max"]:
            raise ValueError(f"{spec['key']} must be between {spec.get('min')} and {spec.get('max')}")
        return v
    if t == "choice":
        values = [o["value"] if isinstance(o, dict) else o for o in spec["options"]]
        if value not in values: raise ValueError(f"{spec['key']} must be one of {values}")
        return value
    if not isinstance(value, str) or len(value) > 256:
        raise ValueError(f"{spec['key']} must be text (256 characters at most)")
    return value


class PluginStore:
    """The plugins under one PascalPatch data root."""

    def __init__(self, data_root, site_url=SITE_URL, trust=None, opener=None):
        self.data = Path(data_root).expanduser().resolve()
        self.site = site_url if site_url.endswith("/") else site_url + "/"
        self.trust = trust if trust is not None else _read_json(TRUST_FILE)
        self.opener = opener or urlopen
        self.root = self.data / "plugins"
        self.settings_dir = self.data / "plugin-settings"
        self.cache = self.data / "cache" / "plugin-site"
        self.state_file = self.data / "plugins.json"

    # ---- state: which plugins are installed (active version) and enabled ----
    def state(self):
        s = _read_json(self.state_file, {}) or {}
        s.setdefault("plugins", {})
        return s

    def _save_state(self, s):
        _write_json(self.state_file, s)

    # ---- the website ----
    def _fetch(self, name, max_bytes):
        url = self.site + name
        if urlparse(url).scheme != "https":
            raise ManifestError("the plugin site must be served over HTTPS")
        req = Request(url, headers={"Accept": "application/json", "User-Agent": "pascalpatch-app/1"})
        with self.opener(req, timeout=8) as r:
            raw = r.read(max_bytes + 1)
        if len(raw) > max_bytes:
            raise ManifestError(f"{name} exceeds its size limit")
        return json.loads(raw.decode("utf-8"))

    def refresh(self):
        """Download the index (verified before it is kept) and the catalog; returns the listing."""
        if not self.trust:
            raise ManifestError("no plugin trust store; reinstall PascalPatch")
        index = self._fetch("index.json", 1024 * 1024)
        try:
            verify_index(index, self.trust)
        except ValueError as exc:
            raise ManifestError(f"the plugin index failed verification: {exc}") from exc
        try:
            catalog = self._fetch("catalog.json", 4 * 1024 * 1024)
        except Exception:   # display data only: the store still works without it
            catalog = {"plugins": []}
        _write_json(self.cache / "index.json", index)
        _write_json(self.cache / "catalog.json", catalog)
        _write_json(self.cache / "fetched.json", {"at": int(time.time()), "site": self.site})
        return self.listing()

    def _signed_entries(self):
        doc = _read_json(self.cache / "index.json")
        if not doc or not self.trust:
            return []
        try:
            return verify_index(doc, self.trust)
        except ValueError:
            return []

    def listing(self):
        """Every plugin the site offers, merged with what is installed here."""
        entries = {}
        for e in self._signed_entries():   # newest version per id (index order breaks ties)
            if e["id"] not in entries or _version_key(e["version"]) > _version_key(entries[e["id"]]["version"]):
                entries[e["id"]] = e
        cat = {c.get("id"): c for c in (_read_json(self.cache / "catalog.json", {}) or {}).get("plugins", []) if isinstance(c, dict)}
        st = self.state()["plugins"]; out = []
        for pid, e in sorted(entries.items()):
            c = cat.get(pid, {}); mine = st.get(pid, {})
            out.append({
                "id": pid, "name": c.get("name", pid), "summary": c.get("summary", ""), "description": c.get("description", ""),
                "author": c.get("author", e["maintainer"]), "tags": c.get("tags", []), "icon": c.get("icon"),
                "homepage": c.get("homepage"), "changelog": c.get("changelog", []),
                "version": e["version"], "license": e["license"], "compatibility": e["compatibility"],
                "installed": mine.get("version"), "enabled": bool(mine.get("enabled")),
                "update": bool(mine.get("version")) and _version_key(e["version"]) > _version_key(mine["version"]),
            })
        fetched = _read_json(self.cache / "fetched.json", {}) or {}
        return {"site": self.site, "fetched": fetched.get("at"), "plugins": out}

    # ---- installing ----
    def install(self, pid, version=None):
        entries = [e for e in self._signed_entries() if e["id"] == pid and (version is None or e["version"] == version)]
        if not entries:
            raise ManifestError(f"{pid} {version or ''} is not in the signed plugin index; refresh the store first".replace("  ", " "))
        entry = max(entries, key=lambda e: _version_key(e["version"]))
        if not entry["source"].startswith(self.site) and urlparse(entry["source"]).netloc != "github.com":
            raise ManifestError(f"{pid} downloads from an unexpected host: {entry['source']}")

        def check(staging):
            m = _read_json(staging / "plugin.json")
            errors = validate_manifest(m, entry)
            if errors:
                raise ManifestError(f"{pid}: invalid plugin.json", errors)
            if not (staging / m["entry"]).is_file():
                raise ManifestError(f"{pid}: package has no {m['entry']}")

        target = install_remote_archive(entry, self.root, opener=self.opener, validator=check,
                                        allow_executables=(f"{pid}.dll",), timeout=30.0)
        self._activate(pid, entry["version"], source="site", sha256=entry["sha256"])
        return target

    def install_file(self, zip_path):
        """Install a plugin ZIP from disk (a developer's own build). Marked as local: not verified."""
        zip_path = Path(zip_path)
        with zipfile.ZipFile(zip_path) as z:
            m = json.loads(z.read("plugin.json").decode("utf-8"))
            errors = validate_manifest(m)
            if errors:
                raise ManifestError("invalid plugin.json", errors)
            pid, ver = m["id"], m["version"]
            allowed = {"plugin.json", m["entry"], f"{pid}.json", "README.md", "LICENSE", "LICENSE.txt"}
            names = [i.filename for i in z.infolist() if not i.is_dir()]
            bad = [n for n in names if n not in allowed and not n.startswith("assets/")]
            if bad or any(".." in Path(n).parts or n.startswith(("/", "\\")) or ":" in n for n in names):
                raise ManifestError("unexpected files in plugin package", [ValidationError(n, "member", "not allowed") for n in bad])
            target = self.root / pid / ver; staging = target.parent / ("." + ver + ".staging")
            shutil.rmtree(staging, ignore_errors=True); staging.mkdir(parents=True)
            for n in names:
                out = (staging / n).resolve()
                if staging.resolve() not in out.parents:
                    raise ManifestError("unsafe plugin package")
                out.parent.mkdir(parents=True, exist_ok=True); out.write_bytes(z.read(n))
            shutil.rmtree(target, ignore_errors=True); staging.rename(target)
        self._activate(pid, ver, source="file", sha256=hashlib.sha256(zip_path.read_bytes()).hexdigest())
        return target

    def _activate(self, pid, version, source, sha256):
        s = self.state(); prev = s["plugins"].get(pid, {})
        s["plugins"][pid] = {"version": version, "enabled": prev.get("enabled", True), "source": source,
                             "sha256": sha256, "installed_at": int(time.time())}
        self._save_state(s)
        for old in (self.root / pid).iterdir():   # keep only the active version
            if old.is_dir() and old.name != version and not old.name.startswith("."):
                shutil.rmtree(old, ignore_errors=True)

    def uninstall(self, pid, keep_settings=True):
        s = self.state()
        if pid not in s["plugins"]:
            raise ManifestError(f"{pid} is not installed")
        del s["plugins"][pid]; self._save_state(s)
        shutil.rmtree(self.root / pid, ignore_errors=True)
        if not keep_settings:
            (self.settings_dir / f"{pid}.json").unlink(missing_ok=True)

    def set_enabled(self, pid, enabled):
        s = self.state()
        if pid not in s["plugins"]:
            raise ManifestError(f"{pid} is not installed")
        s["plugins"][pid]["enabled"] = bool(enabled); self._save_state(s)

    # ---- installed plugins and their settings ----
    def folder(self, pid):
        v = self.state()["plugins"].get(pid, {}).get("version")
        return self.root / pid / v if v else None

    def manifest(self, pid):
        f = self.folder(pid)
        return _read_json(f / "plugin.json") if f else None

    def installed(self):
        out = []
        for pid, st in sorted(self.state()["plugins"].items()):
            m = self.manifest(pid) or {"id": pid, "name": pid, "version": st.get("version"), "summary": "(package missing: reinstall)"}
            out.append({**{k: m.get(k) for k in ("id", "name", "version", "summary", "author", "abi", "homepage")},
                        "enabled": bool(st.get("enabled")), "source": st.get("source"), "settings_schema": m.get("settings", []),
                        "settings": self.get_settings(pid), "ok": self.manifest(pid) is not None})
        return out

    def get_settings(self, pid):
        m = self.manifest(pid) or {}
        saved = _read_json(self.settings_dir / f"{pid}.json", {}) or {}
        values = {}
        for spec in m.get("settings", []):
            try:
                values[spec["key"]] = coerce_setting(spec, saved[spec["key"]]) if spec["key"] in saved else spec.get("default")
            except ValueError:
                values[spec["key"]] = spec.get("default")
        return values

    def set_settings(self, pid, changes):
        m = self.manifest(pid)
        if m is None:
            raise ManifestError(f"{pid} is not installed")
        specs = {s["key"]: s for s in m.get("settings", [])}
        values = self.get_settings(pid)
        for k, v in changes.items():
            if k not in specs:
                raise ManifestError(f"{pid} has no setting {k!r}")
            try:
                values[k] = coerce_setting(specs[k], v)
            except ValueError as exc:
                raise ManifestError(str(exc)) from exc
        _write_json(self.settings_dir / f"{pid}.json", values)
        return values

    # ---- launch ----
    def stage(self, mods_dir):
        """Copy every enabled plugin into a run's mods folder; returns what was staged.

        Each plugin gets ``<id>.dll`` and ``<id>.json``: its default config with the user's
        settings under ``"settings"``. The runtime passes that path to the plugin's load
        function, so a plugin reads its settings the same way it reads its config.
        """
        mods_dir = Path(mods_dir); mods_dir.mkdir(parents=True, exist_ok=True); staged = []
        for pid, st in sorted(self.state()["plugins"].items()):
            f = self.folder(pid); m = self.manifest(pid)
            if not st.get("enabled") or not m or not (f / m["entry"]).is_file():
                continue
            if (mods_dir / m["entry"]).exists():   # a profile's own copy (first-party) wins
                continue
            shutil.copy2(f / m["entry"], mods_dir / m["entry"])
            config = _read_json(f / f"{pid}.json", {}) or {}
            config["settings"] = self.get_settings(pid)
            # What the F2 overlay shows for the plugin: its name, version and settings widgets.
            config["_pascalpatch"] = {"id": pid, "name": m.get("name", pid), "version": m["version"],
                                      "source": "store", "settings_schema": m.get("settings", [])}
            _write_json(mods_dir / f"{pid}.json", config)
            staged.append({"id": pid, "version": m["version"]})
        return staged


def _version_key(v):
    parts = []
    for p in str(v).replace("-", ".").split("."):
        parts.append((0, int(p), "") if p.isdigit() else (1, 0, p))
    return parts
