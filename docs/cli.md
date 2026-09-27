# CLI

All commands accept global `--root` and `--data` options before the subcommand.

```text
pascalpatch --root PROJECT profile list
pascalpatch --root PROJECT profile validate PROFILE
pascalpatch --root PROJECT build PROFILE
pascalpatch --root PROJECT launch PROFILE --dry-run
pascalpatch --root PROJECT launch PROFILE --wait --timeout 30
pascalpatch --root PROJECT logs PROFILE
```

Unsafe profiles require `--allow-unsafe` at launch. `--dry-run` prints the selected executable, `-e` game argument, and log path without starting Dolphin.
