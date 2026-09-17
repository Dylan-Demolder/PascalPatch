# Dolphin smoke runner

`tooling/dolphin_smoke.py` runs clean and modified user-provided ISO files with a hard timeout. On timeout it calls `kill()` directly; it does not gracefully terminate Dolphin and therefore does not leave a Confirm Stop modal dialog.

```sh
PYTHONPATH=host/src python tooling/dolphin_smoke.py \
  --dolphin /path/to/dolphin-emu \
  --clean /path/to/user/GALE01.iso \
  --modified /path/to/generated.iso \
  --movie /path/to/menu-to-match.dtm \
  --expected-input-automation-ready INPUT_AUTOMATION_READY \
  --timeout 20
```

When a supplied movie file exists and Dolphin starts, the JSON includes `tooling_markers: ["INPUT_AUTOMATION_READY"]`; this is a bounded tooling signal that movie playback was requested, not a runtime gameplay marker. Menu/title and plugin behavior still require the Dolphin log and runtime markers.

The manual `.github/workflows/integration-smoke.yml` workflow is self-hosted and accepts all game/emulator paths as inputs. It never uploads or stores game data.
