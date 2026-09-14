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
```

This directly validates the HSD container parser against a real GALE01 fighter-data archive. It does not yet decode fighter object graphs, write a compatible replacement, or claim character conversion/playability.
