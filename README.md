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
PYTHONPATH=host/src python tooling/meleemod_gui.py --root .
```

`profile list` and other commands expect `profiles/`, `plugins/`, and `mods/` catalogs under `--root`. Relative base-game and mod paths are resolved from their manifest locations.

Extract a user-owned ISO for filesystem composition with the pinned decompilation toolkit:

```sh
python tooling/extract_disc.py /path/to/GALE01.iso /path/to/extracted-game \
  --dtk /path/to/dtk
```

PPC static profiles require an extracted game directory and explicit decompilation/runtime/source paths. Plugin entrypoints are initialized on the first game-loop frame; in-game behavior still requires emulator observation. The optional Tk GUI is a thin view over the same validated core APIs.

## Tests

```sh
PYTHONPATH=host/src python -m unittest discover -s host/tests -v
gcc -std=c99 -Wall -Wextra -Werror -Iruntime/include \
  runtime/src/events.c runtime/src/runtime.c runtime/tests/test_events.c \
  -o /tmp/meleemod-test && /tmp/meleemod-test
```

## Legal boundary

This repository contains tooling, schemas and original sample code only. Do not commit, distribute or fetch Nintendo ISO, DOL, extracted assets or copyrighted game data.


## Static code profiles

A code profile must explicitly provide `decomp_repo`, `decomp_orig` and `plugin_source_root`. Plugins using the SDK context ABI also require `runtime_root`. Each selected plugin must declare a relative `.c` `source` and a C entrypoint. The host creates a disposable worktree and stages the generated DOL. Static code profiles can also recompose a staged ISO with `tooling/recompose_disc.py`; the source ISO is never modified. Filesystem asset mods still require an extracted game directory.


## Bounded emulator smoke

For clean/modified comparisons, use the user-data-only runner:

```sh
PYTHONPATH=host/src python tooling/dolphin_smoke.py \
  --dolphin /path/to/dolphin-emu \
  --clean "/path/to/Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso" \
  --modified /path/to/generated.iso \
  --timeout 30
```

The runner kills the complete emulator process group on timeout. It does not leave a Confirm Stop dialog or orphan process.


## Verified demo mod

The repository includes a harmless `demo-mod` catalog entry at `plugins/demo-mod/plugin.json`. Create a local runnable profile with `python tooling/create_demo_profile.py /path/to/game.iso /path/to/MeleeDecomp/melee`. It is a visual-only static PPC plugin that logs initialization and its first frame callback. Use a temporary profile with your own GALE01 Rev.02 input, then run `profile validate` and `build`; do not commit the ISO or generated game output. Direct loader/Dolphin evidence is in `docs/evidence/demo-mod.md`.
