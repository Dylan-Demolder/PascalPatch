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

The real user-provided `orig/GALE01/files/PlCo.dat` now passes the bounded HSD container validator; its object graph is not decoded yet. `src/melee/ft/types.h` and `src/melee/ft/fighter.h` show that fighter loading consumes structured `ftData` records and the `gFtDataList` table. The source comments identify `PlCo.dat` as a fighter-data table. These structures connect model parts, attributes, animations, action/state callbacks, and fighter kinds; they are not equivalent to a glTF skeleton or a moveset JSON file.

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


`Fighter_LoadCommonData` in the local source resolves `ftLoadCommonData` as a `void **` and consumes 23 four-byte entries. The real archive inspection now confirms those 23 entries are in-range offsets. This is a common-data table, not a complete playable-fighter conversion target; its pointed-to structures still require typed decoding and safe composition.

## PAS-28 bounded milestone and remaining gates

The current verified milestone is **custom archive injection plus boot**: the Studio package is composed into `ftDataLeesinhsd2e`, overlaid into the Mario clone slot, and the modified ISO reaches the Melee title in Dolphin. This does not prove that character-select instantiation reaches a valid fighter object or that a match can start.

The remaining playability work is bounded to these gates, in order:

1. Decode the 23 `ftLoadCommonData` entries and the referenced `ftData` records with typed structs and relocation-range tests.
2. Map Studio attributes into the target clone's `ftAttributes`, preserving required defaults and validating ranges against the decomp structs.
3. Convert moveset entries into action-state tables and callback references; reject any missing or unsupported callback rather than borrowing an unverified address.
4. Compose the converted data into the target `PlCo.dat`/fighter table and add a decomp-built loader hook that calls `OSReport` after the custom symbol resolves.
5. Run character select and one offline match on the self-hosted Dolphin runner, requiring log markers for symbol resolution, attribute-table validation, callback/action-table validation, character-select completion, and match start.

Until gates 1–5 have direct tests and Dolphin evidence, the package contract must continue to report `game_integration: false`; `archive_exists`, `data_exists`, or a booting ISO are not playability evidence. The workflow now rebuilds the DOL with the generated source overlay and requires `symbol_observed`, `in_game_observed`, `character_select_complete`, and `offline_match_started`; a successful runner execution is still required before playability is claimed.
