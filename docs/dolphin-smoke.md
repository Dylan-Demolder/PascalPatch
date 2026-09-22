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

Set `PAS_DOLPHIN_PLATFORM=headless` to force Dolphin's headless platform in the smoke subprocess. Pass `--diagnostics-dir` to capture per-ISO `launcher.txt`, `stdout.log`, `stderr.log`, and `coredumpctl.txt`; the launcher records the resolved binary, SHA-256, version, help output, and dynamic linkage. These files are diagnostic evidence only; gameplay claims still require real runtime markers.

The manual `.github/workflows/integration-smoke.yml` workflow is self-hosted and accepts all game/emulator paths as inputs. It never uploads or stores game data.
