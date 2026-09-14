# CLI

All commands accept global `--root` and `--data` options before the subcommand.

```text
meleemod --root PROJECT profile list
meleemod --root PROJECT profile validate PROFILE
meleemod --root PROJECT build PROFILE
meleemod --root PROJECT launch PROFILE --dry-run
meleemod --root PROJECT launch PROFILE --wait --timeout 30
meleemod --root PROJECT logs PROFILE
```

Unsafe profiles require `--allow-unsafe` at launch. `--dry-run` prints the selected executable, `-e` game argument, and log path without starting Dolphin.
