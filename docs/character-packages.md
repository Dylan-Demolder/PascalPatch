# Character package staging

`compose_validated_package` validates a `.melee-character` package and stages its safe authoring files outside the game tree. It is available for Offline workspace preparation only. It writes `staging.json` with `game_integration: false`.

This is not Melee asset conversion and does not make a playable character. Unknown compatibility, POSIX or Windows traversal paths, executable payloads, symlinks, invalid checksums, and non-Offline gameplay packages fail closed.

## Native profile installation (Tier B)

An offline profile can install a Character Studio character into an existing
fighter slot:

```json
"characters": [
  {"package": "leesin.melee-character", "slot": "captain-falcon",
   "fighter_file": "PlCa.dat", "costume_file": "PlCaNr.dat"}
]
```

`pascalpatch build` validates the package, requires `mode: offline`, checks each
file name against the slot (`PlCa.dat`, default costume `PlCaNr.dat`) and
checks the Character Studio report beside it (`.slot.json`, `.costume.json`:
character id and output SHA-256), then overlays the files into the profile ISO
with the DOL untouched. `pascalpatch launch --runtime native` runs it on
melee-unlocked. The character replaces the slot's fighter; it is not a new
roster entry.

### Many characters in one profile

A profile can hold one `characters` entry per slot. `pascalpatch build` rejects two entries that use the same slot. Character Studio's `build-roster` writes a profile like this directly, with one entry per built character:

```sh
# in MeleeCharacterStudio
PYTHONPATH=core/src python -m melee_character_studio.cli build-roster roster/roster.json \
  --iso /path/to/GALE01.iso --out ~/melee-roster-build \
  --profile /path/to/MeleeMod/profiles/custom-roster.json
# in PascalPatch
pascalpatch --root /path/to/MeleeMod build custom-roster
pascalpatch --root /path/to/MeleeMod launch custom-roster --runtime native --allow-unsafe --port /path/to/melee_port.exe
```

The generated entries look like this:

```json
{"package": "~/melee-roster-build/hulk/hulk.melee-character", "slot": "donkey-kong",
 "fighter_file": "~/melee-roster-build/hulk/PlDk.dat", "costume_file": "~/melee-roster-build/hulk/PlDkNr.dat"}
```

An entry has `animation_file` (`PlXxAJ.dat` plus its `.anim.json` report) only when the character borrows moves from another fighter. The build output and your ISO stay outside this repository. `profiles/custom-roster.json` is ignored by git for that reason (see `.gitignore`).

A character's `character.json` may carry `description`, `features`, `attributes` and `attribute_scales`. They are recorded in `schemas/character.schema.json`, and PascalPatch only passes them through.
