# Real `PlCo.dat` HSD validation

Date: 2026-09-14. The validator was run directly against the user-provided decompilation file:

```text
/home/dyland/Documents/MeleeDecomp/melee/orig/GALE01/files/PlCo.dat
```

No bytes were copied into either project or committed. The validator reported:

```text
file_size=149101
data_size=145824
relocations=805
publics=1
externs=0
version=b'001B'
table_end=149084
symbol_bytes=17
public_symbol=ftLoadCommonData
public_offset=60632
```

This directly validates the HSD container parser against a real GALE01 fighter-data archive. It does not yet decode fighter object graphs, write a compatible replacement, or claim character conversion/playability.

The 805 pre-relocation pointer words were also validated as in-range data offsets (minimum 0; maximum 137632).


## Source-backed public-root interpretation

The local decompilation's `Fighter_LoadCommonData` (`src/melee/ft/fighter.c`) loads `ftLoadCommonData` as a `void **` and consumes 23 four-byte entries. Reading the validated root at data offset 60632 produced these in-range pre-relocation offsets:

```text
40896, 43228, 43540, 43660, 60216, 60448, 51652, 52636,
60584, 60592, 60616, 60624, 42968, 43124, 43184, 43220,
87336, 52868, 52888, 52908, 137544, 43696, 60168
```

This identifies the common-data table boundary without claiming that the entries are decoded fighter objects.


The source maps the 23 entries as: `p_ftCommonData`, `Fighter_804D6550`, `Fighter_804D654C`, `Fighter_804D6548`, `ftPartsTable`, `Fighter_804D6540`, `Fighter_804D653C`, `Fighter_804D6538`, `Fighter_804D6534`, `Fighter_804D6530`, `Fighter_804D652C`, `Fighter_804D6528`, `Fighter_804D6524`, `Fighter_804D6520`, `Fighter_804D651C`, `Fighter_804D6518`, `Fighter_804D6514`, `Fighter_804D6510`, `Fighter_804D650C`, `Fighter_804D6508`, `Fighter_804D6504`, `gCrowdConfig`, and `Fighter_804D64FC`. This mapping is documented for research only; it is not a converter or runtime composition path.