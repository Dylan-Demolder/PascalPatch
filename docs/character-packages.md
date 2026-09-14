# Character package staging

`compose_validated_package` validates a `.melee-character` package and stages its safe authoring files outside the game tree. It is available for Offline workspace preparation only. It writes `staging.json` with `game_integration: false`.

This is not Melee asset conversion and does not make a playable character. Unknown compatibility, unsafe paths, executable payloads, invalid checksums, and non-Offline gameplay packages fail closed.
