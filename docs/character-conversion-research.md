# Character conversion research boundary

This note records source-backed facts from the local `doldecomp/melee` checkout. It is not an asset converter and does not include Nintendo data.

## Verified format facts

`src/sysdolphin/baselib/archive.h` defines the HSD archive header as 32 bytes:

- `file_size`;
- `data_size`;
- relocation, public-symbol, and external-symbol counts;
- a four-byte version field;
- two padding words.

`src/sysdolphin/baselib/archive.c` verifies the file size, lays out the data and relocation tables, locates public/external symbol tables, and applies relocations. A valid HSD archive therefore needs more than a model byte stream. Relocation targets and symbol offsets must agree with the runtime object graph.

`src/melee/ft/types.h` and `src/melee/ft/fighter.h` show that fighter loading consumes structured `ftData` records and the `gFtDataList` table. The source comments identify `PlCo.dat` as a fighter-data table. These structures connect model parts, attributes, animations, action/state callbacks, and fighter kinds; they are not equivalent to a glTF skeleton or a moveset JSON file.

## Consequence for Character Studio

Character Studio currently validates glTF/GLB authoring assets, retargets skeleton transforms, validates moves/attributes, exports deterministic packages, and stages them outside the game tree. Those operations are intentionally authoring-only.

A truthful Melee conversion requires, at minimum:

1. a user-owned source asset and a supported clone-character target;
2. a validated HSD archive writer with endian, alignment, relocation, and symbol tests;
3. conversion of meshes/materials/joints into the HSD object graph;
4. animation and action-table conversion;
5. `ftData`/`PlCo.dat`-compatible attributes and callback references;
6. safe fighter-table/profile composition;
7. character-select and playable-match evidence in an Offline profile.

None of those steps can be inferred from the current glTF package contract. Until each has direct tests and user-owned runtime evidence, package staging must continue to report `game_integration: false` and must not claim playable-character support.
