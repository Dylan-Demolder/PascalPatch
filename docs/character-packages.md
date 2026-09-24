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

`meleemod build` validates the package, requires `mode: offline`, checks each
file name against the slot (`PlCa.dat`, default costume `PlCaNr.dat`) and
checks the Character Studio report beside it (`.slot.json`, `.costume.json`:
character id and output SHA-256), then overlays the files into the profile ISO
with the DOL untouched. `meleemod launch --runtime native` runs it on
melee-unlocked. The character replaces the slot's fighter; it is not a new
roster entry.
