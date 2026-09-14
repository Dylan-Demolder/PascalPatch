# Input overlay research boundary

The local decompilation shows that Melee text is rendered through `sysdolphin/baselib/sislib.h`, not through a generic plugin console. `HSD_Text` objects carry font, position, color, visibility, SIS buffers, and a render callback. Creation uses `HSD_SisLib_803A5ACC`; formatted text uses `HSD_SisLib_803A70A0`/`HSD_SisLib_803A6B98`; display registration uses `HSD_SisLib_803A611C`.

A correct input overlay therefore needs a game-owned SIS font/archive, a render GObj/lifecycle, and an ordering-safe display hook. The current runtime ABI exposes frame/input data but does not expose those objects or ownership rules. The input-display sample consequently stops at callback/input-history observation and does not guess a text renderer.
