# MeleeMod host MVP

MeleeMod is a separate host-side project for building isolated Super Smash Bros. Melee profiles. It never distributes Nintendo game data. Users provide their own GALE01 Rev.02 / Melee 1.02 ISO or extracted game directory.

## Verified today

- GALE01 Rev.02 identification and embedded `main.dol` SHA-1 validation.
- Pinned `doldecomp/melee` build wrapper.
- Isolated profile builds with staging and atomic `current` promotion.
- Exact relative-path filesystem mods with fail-closed conflict detection.
- Vanilla ISO build outputs use a symlink to the original file; the ISO is not copied or changed.
- Strict profile/plugin/mod manifest validation.
- Capability-based safety classification.
- Initial C runtime event ABI, host-compiled tests, SDK sample manifest, dependency-ordered static plugin manifests, and disposable static source-to-DOL composition.
- Character-package path safety and checksum validation.

## Run

```sh
PYTHONPATH=host/src python tooling/inspect_disc.py /path/to/GALE01.iso
PYTHONPATH=host/src python -m meleemod.cli --root . profile list
PYTHONPATH=host/src python -m meleemod.cli --root . profile validate PROFILE_ID
PYTHONPATH=host/src python -m meleemod.cli --root . build PROFILE_ID
PYTHONPATH=host/src python -m meleemod.cli --root . launch PROFILE_ID --dry-run
```

`profile list` and other commands expect `profiles/`, `plugins/`, and `mods/` catalogs under `--root`. Relative base-game and mod paths are resolved from their manifest locations.

Extract a user-owned ISO for filesystem composition with the pinned decompilation toolkit:

```sh
python tooling/extract_disc.py /path/to/GALE01.iso /path/to/extracted-game \
  --dtk /path/to/dtk
```

The current host MVP intentionally rejects PPC plugins during profile builds until the in-game integration spike has been boot-tested. The C ABI and static sample contract are present for that next step.

## Tests

```sh
PYTHONPATH=host/src python -m unittest discover -s host/tests -v
gcc -std=c99 -Wall -Wextra -Werror -Iruntime/include \
  runtime/src/events.c runtime/src/runtime.c runtime/tests/test_events.c \
  -o /tmp/meleemod-test && /tmp/meleemod-test
```

## Legal boundary

This repository contains tooling, schemas and original sample code only. Do not commit, distribute or fetch Nintendo ISO, DOL, extracted assets or copyrighted game data.
