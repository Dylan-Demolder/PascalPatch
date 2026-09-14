# Dolphin smoke runner

`tooling/dolphin_smoke.py` runs clean and modified user-provided ISO files with a hard timeout. On timeout it calls `kill()` directly; it does not gracefully terminate Dolphin and therefore does not leave a Confirm Stop modal dialog.

```sh
PYTHONPATH=host/src python tooling/dolphin_smoke.py \
  --dolphin /path/to/dolphin-emu \
  --clean /path/to/user/GALE01.iso \
  --modified /path/to/generated.iso \
  --timeout 20
```

The runner reports process results only. Menu/title and plugin behavior require a separate GUI/log observer.
