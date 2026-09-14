# User setup

1. Obtain your own legally owned GALE01 Rev.02 / Melee 1.02 game data.
2. Inspect it without modifying it:

```sh
PYTHONPATH=host/src python tooling/inspect_disc.py /path/to/game.iso
```

3. If you need filesystem asset composition, extract to a separate directory with `tooling/extract_disc.py` and `dtk`.
4. Put a profile JSON under `profiles/` and reference plugin/mod IDs from the local catalogs.
5. Run `profile validate`, then `build`.

Do not place an ISO, DOL or extracted Nintendo assets in this repository.
