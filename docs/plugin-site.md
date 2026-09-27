# The plugin site

The PascalPatch app's **Browse** page and the website at <https://dylan-demolder.github.io/pascalpatch-plugins/> read the same files:

| File | What it is |
|---|---|
| `index.json` | the signed index (`pascalpatch/registry-index/1`, Ed25519). The app installs only what it lists, and checks each package's SHA-256. |
| `catalog.json` | names, summaries, tags and settings for display. Never trusted for installs. |
| `packages/<id>-<version>.zip` | the packages (plugin.json, README, DLL). Served from GitHub Releases by default. |
| `index.html`, `assets/` | the website (source in `website/`). |

## Publishing

On the maintainer's machine (the signing seed never leaves it):

```sh
python tooling/site/publish.py --out C:/Users/you/MeleeMods/pascalpatch-plugins \
  --seed C:/Users/you/MeleeMods/keys/plugin-index.seed \
  --dlls C:/Users/you/MeleeMods/data/native-plugins \
  --plugin input-display --plugin unlock-all
```

It builds each ZIP with fixed timestamps, so the same inputs always give the same bytes. It keeps the versions already in `index.json`. Publishing an existing id+version with different bytes is refused: bump the version instead.

Then:

1. commit and push the `--out` folder (the `pascalpatch-plugins` repository). Its GitHub Actions workflow deploys it to Pages;
2. run the `gh release create <id>-v<version> ...zip` commands that `publish.py` prints, which upload the packages.

With `--host pages`, packages are served from Pages instead and there are no releases to make.

## Previewing the website

```sh
python -m http.server 8791 --bind 127.0.0.1 --directory C:/Users/you/MeleeMods/pascalpatch-plugins
```

The app only installs from an `https://` site, so a local copy is for checking the pages, not for installing. The site's **Open in PascalPatch** button opens `http://127.0.0.1:8790/#browse/<id>` in the running app.
