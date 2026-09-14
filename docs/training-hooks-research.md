# Training hook research boundary

This note records source-backed candidates from the local `doldecomp/melee` checkout. It does not claim that a training hook is implemented.

## Frame and reset

The current runtime frame boundary is the safe place for sampling. A frame-advance controller must additionally own game-mode pause/reset state; advancing the plugin callback alone would not freeze fighter simulation. Reset must be tied to the game-mode/fighter initialization path rather than clearing only the input ring. The decomp source exposes many mode-specific state machines, so an address or field inferred from a generic plugin callback would be unsafe.

## Hit events

`src/melee/ft/ft_07C1.c` contains `ft_8007C2E0`, which performs fighter hit-capsule collision work and updates knockback-related state. `src/melee/ft/fighter.c` processes damage and hitlag transitions; the source shows `deal_dmg_cb`, shield/reflect/absorb callbacks, and `x195c_hitlag_frames` updates. These are better hook candidates than polling a guessed damage field, but they require a game-specific event ABI carrying attacker/victim identity, damage, and frame number.

No callback is added yet because the current static composition does not safely expose the relevant fighter structures or preserve the decompilation's callback ordering. The validated runtime currently claims only frame/input events and the training heartbeat.
