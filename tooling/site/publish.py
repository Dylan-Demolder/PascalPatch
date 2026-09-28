"""Build the PascalPatch plugin site: packages, the signed index, the catalog and the pages.

    python tooling/site/publish.py --out C:/Users/you/MeleeMods/pascalpatch-plugins \
        --seed C:/Users/you/MeleeMods/keys/plugin-index.seed --dlls C:/Users/you/MeleeMods/data/native-plugins \
        --plugin input-display --plugin unlock-all

For each plugin (plugins/<id>/plugin.json in this repo, its DLL from --dlls) it writes a
reproducible ZIP to <out>/packages/, adds a signed entry to <out>/index.json (keeping the
versions already published there), and describes it in <out>/catalog.json. The site's pages
(website/) are copied alongside, with a GitHub Actions workflow that deploys <out> to Pages.

Signing happens here, on the maintainer's machine: the seed never goes to GitHub, and the
workflow only publishes files that are already signed. A published id+version is immutable;
building it again must give the same bytes.

Packages are served from GitHub Releases (``--host releases``, the default: upload
<out>/packages/*.zip to the release named in the printed commands) or from Pages itself
(``--host pages``). The PascalPatch app accepts either.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import shutil
import sys
import time
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "host/src"))
from pascalpatch.plugin_store import validate_manifest  # noqa: E402
from pascalpatch.registry import validate_entry  # noqa: E402
from pascalpatch.registry_signing import key_id, public_key, sign_index, verify_index  # noqa: E402

SITE = "https://dylan-demolder.github.io/pascalpatch-plugins/"
RELEASES = "https://github.com/Dylan-Demolder/pascalpatch-plugins/releases/download/"
TRUST = REPO / "host/src/pascalpatch/trust.json"
EPOCH = (2020, 1, 1, 0, 0, 0)   # fixed ZIP timestamps: the same inputs give the same package bytes

WORKFLOW = """name: Deploy plugin site
on:
  push:
    branches: [main]
  workflow_dispatch:
permissions:
  contents: read
  pages: write
  id-token: write
concurrency:
  group: pages
  cancel-in-progress: true
jobs:
  deploy:
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - uses: actions/checkout@v4
      # index.json is signed on the maintainer's machine; this job only checks it is well formed.
      - run: python3 -c "import json; d=json.load(open('index.json')); assert d['schema'].startswith('pascalpatch/registry-index/'), d['schema']"
      - uses: actions/configure-pages@v5
      - uses: actions/upload-pages-artifact@v3
        with:
          path: .
      - id: deployment
        uses: actions/deploy-pages@v4
"""


def package(plugin_dir: Path, dll: Path, manifest: dict) -> bytes:
    """A reproducible ZIP: plugin.json, <id>.dll, and the optional config, README and icon."""
    pid = manifest["id"]
    files = {"plugin.json": (json.dumps(manifest, indent=2) + "\n").encode(), f"{pid}.dll": dll.read_bytes()}
    for name in (f"{pid}.json", "README.md", "icon.png"):
        if (plugin_dir / name).is_file():
            files[name] = (plugin_dir / name).read_bytes()
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        for name in sorted(files):
            info = zipfile.ZipInfo(name, EPOCH); info.compress_type = zipfile.ZIP_DEFLATED; info.external_attr = 0o644 << 16
            z.writestr(info, files[name])
    return buf.getvalue()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", required=True, help="the site folder (the pascalpatch-plugins repository's checkout)")
    ap.add_argument("--seed", required=True, help="the index signing seed (32 bytes, kept off GitHub)")
    ap.add_argument("--dlls", required=True, help="folder with the built <id>.dll files (build_plugins.py's output)")
    ap.add_argument("--plugin", action="append", default=[], help="plugin id to publish (repeatable; default: none, pages only)")
    ap.add_argument("--host", choices=("releases", "pages"), default="releases")
    ap.add_argument("--site", default=SITE)
    ap.add_argument("--replace", action="store_true",
                    help="let a version already in the index be rebuilt; only for versions whose release is still a "
                         "draft (nobody can have installed them)")
    a = ap.parse_args(argv)
    out = Path(a.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    seed = Path(a.seed).read_bytes()
    if len(seed) != 32:
        raise SystemExit("the seed must be 32 bytes")
    trust = json.loads(TRUST.read_text(encoding="utf-8"))
    kid = key_id(public_key(seed))
    if kid not in trust.get("keys", {}):
        raise SystemExit(f"key {kid} is not in {TRUST}; PascalPatch would reject this index")

    entries = []
    if (out / "index.json").is_file():   # keep every version already published
        entries = verify_index(json.loads((out / "index.json").read_text(encoding="utf-8")), trust)
    catalog = json.loads((out / "catalog.json").read_text(encoding="utf-8")) if (out / "catalog.json").is_file() else {"plugins": []}
    cat = {c["id"]: c for c in catalog.get("plugins", [])}
    uploads = []

    for pid in a.plugin:
        pdir = REPO / "plugins" / pid
        manifest = json.loads((pdir / "plugin.json").read_text(encoding="utf-8"))
        errors = validate_manifest(manifest)
        if errors or manifest["id"] != pid:
            raise SystemExit(f"{pid}: plugin.json: {errors or 'id does not match the folder'}")
        dll = Path(a.dlls) / f"{pid}.dll"
        if not dll.is_file():
            raise SystemExit(f"{pid}: {dll} not built; run tooling/native/build_plugins.py")
        data = package(pdir, dll, manifest)
        digest = hashlib.sha256(data).hexdigest()
        name = f"{pid}-{manifest['version']}.zip"
        tag = f"{pid}-v{manifest['version']}"
        source = f"{RELEASES}{tag}/{name}" if a.host == "releases" else f"{a.site}packages/{name}"
        old = next((e for e in entries if e["id"] == pid and e["version"] == manifest["version"]), None)
        if old and old["sha256"] != digest:
            if not a.replace:
                raise SystemExit(f"{pid} {manifest['version']} is already published with different bytes; bump the version "
                                 "(or, while its release is still a draft, pass --replace)")
            entries.remove(old)
            old = None
        (out / "packages").mkdir(exist_ok=True)   # only once the version is known to be publishable
        (out / "packages" / name).write_bytes(data)
        entry = {"id": pid, "version": manifest["version"], "source": source, "sha256": digest,
                 "license": manifest.get("license", "GPL-2.0-or-later"), "compatibility": "offline-only",
                 "dependencies": manifest.get("dependencies", []), "maintainer": manifest.get("author", "PascalPatch")}
        bad = validate_entry(entry)
        if bad:
            raise SystemExit(f"{pid}: index entry: {bad}")
        if not old:
            entries.append(entry)
        readme = (pdir / "README.md").read_text(encoding="utf-8") if (pdir / "README.md").is_file() else ""
        prev = cat.get(pid, {})
        log = [c for c in prev.get("changelog", []) if c.get("version") != manifest["version"]]
        if manifest.get("changes"):
            log.insert(0, {"version": manifest["version"], "notes": manifest["changes"]})
        cat[pid] = {"id": pid, "name": manifest["name"], "summary": manifest["summary"], "description": readme.strip(),
                    "author": manifest.get("author", "PascalPatch"), "tags": manifest.get("tags", []),
                    "homepage": manifest.get("homepage"), "abi": manifest["abi"], "min_runtime": manifest.get("min_runtime"),
                    "settings": manifest.get("settings", []),
                    "icon": manifest.get("icon"), "changelog": log, "updated": int(time.time())}
        if a.host == "releases":
            uploads.append((tag, out / "packages" / name))
        print(f"{pid} {manifest['version']}  {digest}  {source}")

    entries.sort(key=lambda e: (e["id"], e["version"]))
    (out / "index.json").write_text(json.dumps(sign_index(entries, seed), indent=2) + "\n", encoding="utf-8")
    catalog = {"schema": "pascalpatch/plugin-catalog/1", "generated": int(time.time()), "site": a.site,
               "plugins": [cat[k] for k in sorted(cat)]}
    (out / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")

    # the pages (website/ in this repo) and the deploy workflow
    web = REPO / "website"
    shutil.copy2(web / "index.html", out / "index.html")
    shutil.copytree(web / "assets", out / "assets", dirs_exist_ok=True)
    shutil.copy2(REPO / "design/dist/pascal-ui.css", out / "assets/pascal-ui.css")
    (out / ".nojekyll").write_text("", encoding="utf-8")
    if a.host == "releases":   # the ZIPs go to GitHub Releases, not into the repository
        (out / ".gitignore").write_text("packages/\n", encoding="utf-8")
    wf = out / ".github/workflows/pages.yml"; wf.parent.mkdir(parents=True, exist_ok=True); wf.write_text(WORKFLOW, encoding="utf-8")
    if not (out / "README.md").is_file():
        (out / "README.md").write_text(
            "# PascalPatch plugins\n\nThe plugin site for PascalPatch, served by GitHub Pages at "
            f"{a.site}\n\n`index.json` is signed offline by the maintainer (Ed25519, key `{kid}`); the "
            "PascalPatch app installs only what it lists, after checking each package's SHA-256. "
            "`catalog.json` is display data only.\n\nTo propose a plugin, open a pull request that adds its "
            "source and `plugin.json`; packages are built and signed by the maintainer.\n", encoding="utf-8")
    print(f"index: {len(entries)} entr{'y' if len(entries) == 1 else 'ies'} signed by {kid} -> {out / 'index.json'}")
    for tag, path in uploads:
        print(f'  gh release create {tag} "{path}" --repo Dylan-Demolder/pascalpatch-plugins --title "{tag}" --notes "{tag}"')
    return 0


if __name__ == "__main__":
    sys.exit(main())
