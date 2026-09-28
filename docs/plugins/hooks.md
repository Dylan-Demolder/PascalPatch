# Hooks and game functions

Most plugins only read memory and draw. When a plugin must change what the game does, such as skipping a screen, choosing the match's stage, or adding a fighter, it stands in front of one of the game's own functions.

## How the game runs

The port runs Melee's PowerPC code translated ahead of time into x64. So a plugin never patches PowerPC instructions or writes Gecko codes. Instead the runtime keeps a table from every game function's address to its translated code, and gives plugins three tools built on it:

- **`hook(addr, fn, user)`** puts your `fn` in front of the game function at `addr`. Every call to it now reaches you first, whether it comes through a pointer or a direct `bl`. It returns an address that still runs the original.
- **`call(cpu, addr)`** runs a game function with the registers in `cpu`.
- **`trampoline(fn, user)`** gives your `fn` a game address, which you can store in any game table or callback field.

Addresses are NTSC 1.02, exactly as the decomp lists them.

## The calling convention

A hooked or trampolined function receives `pp_cpu* cpu`: the game's registers at the moment of the call. The game uses the standard PowerPC convention:

- integer and pointer arguments in **r3, r4, r5, ...**; float arguments in **f1, f2, ...**
- the result in **r3** (integer or pointer) or **f1** (float)

```cpp
uint32_t a0 = H->reg(cpu, 3);        // first argument
double   x  = H->freg(cpu, 1);       // first float argument
H->set_reg(cpu, 3, 1);               // return 1
```

## A hook, step by step

Quick Match ([quick_match.cpp](../../plugins/quick-match/native/quick_match.cpp)) boots straight into a match. It does this by standing behind two functions of the game's mode system:

```cpp
constexpr uint32_t BOOT_ON_LEAVE     = 0x801BF9A8;   // gmboot.c bootOnLeave
constexpr uint32_t DEBUG_VS_ON_ENTER = 0x801B13B8;   // gmvsmode.c onEnterDebugVs
uint32_t g_boot_leave = 0, g_debug_enter = 0;         // the originals

// Boot is leaving for the title screen: send it to the debug VS mode instead.
void boot_on_leave(pp_cpu* cpu, void*) {
  H->call(cpu, g_boot_leave);                // 1. run the original, with the game's arguments
  H->wr8(0x80479D30 + 1, 0x0E);              // 2. then change what it decided (the pending mode)
}

// The debug VS mode has filled in its own test match: overwrite it with ours.
void debug_vs_on_enter(pp_cpu* cpu, void*) {
  uint32_t state = H->reg(cpu, 3);           // read arguments before the call clobbers r3
  H->call(cpu, g_debug_enter);
  uint32_t start = H->rd32(state + 0x10);    // the match's StartMeleeData
  H->wr16(start + 0x0E, 0x20);               // the stage: Final Destination
  ...
}

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char*) {
  ...
  g_boot_leave  = H->hook(BOOT_ON_LEAVE, boot_on_leave, nullptr);
  g_debug_enter = H->hook(DEBUG_VS_ON_ENTER, debug_vs_on_enter, nullptr);
  if (!g_boot_leave || !g_debug_enter) { H->log(ID, "could not hook the boot and debug VS scenes"); return 1; }
  return 0;
}
```

The shape is always the same:

1. **Read the arguments you need first.** `call` leaves the callee's results in `cpu`, so r3 and the other argument registers are gone afterwards.
2. **Run the original** through the address `hook` returned. Skip this only when you mean to replace the function entirely, and then set r3 or f1 yourself if it returns something.
3. **Adjust** what it did, or what it returned.
4. **Check `hook`'s result** at load time. A 0 means the address is not a function the port has translated.

Two hooks on the same function chain: the later one runs first, and its "original" is the earlier hook.

### When a hook misses calls

The runtime patches the translated function's entry, so direct calls are caught too, with two exceptions. Both are reported in the log:

- **Shared translations.** A few tiny game functions share one translation with others (HLE stand-ins for library code). Hooking one would hook them all, so these catch pointer calls only. The log says `host function shared with ...`.
- **Recursion.** While your hook is running the original, a call the original makes to the same function goes straight through.

## Calling game functions

`call` needs a `pp_cpu`, and only hook and trampoline callbacks have one. To call the game from `on_frame`, hook a function that runs where you need it (a scene's think function, a per-frame update) and call from there.

```cpp
uint32_t heap_alloc(pp_cpu* cpu, uint32_t size) {
  H->set_reg(cpu, 3, size);          // arguments in r3...
  H->call(cpu, 0x8037F1E4);          // HSD_MemAlloc
  return H->reg(cpu, 3);             // ...result in r3
}
```

Setting registers for a call overwrites the ones your hook received. Read everything you need from `cpu` before your first `call`. If the hook's own caller expects a result, set it again after your calls.

## Trampolines and game memory

Some game data stores function addresses: callback tables, the `on_enter` of a game-mode state, a GObj's think procedure. `trampoline` gives you an address that runs your code, to store there:

```cpp
uint32_t my_think = H->trampoline(think, nullptr);
H->wr32(gobj + 0x..., my_think);
```

When the game needs to read data you make (a table, a string, an image descriptor), `guest_alloc(size)` gives zeroed, 32-byte-aligned game memory that is never freed. The pool is shared and only a few KB, so allocate once at load and reuse it. For big things, allocate from the game's own heap with `HSD_MemAlloc` inside a hook, the way Extra Fighters does for portrait images.

## Finding addresses

The [doldecomp/melee](https://github.com/doldecomp/melee) decompilation names nearly every function and global in the game:

- **Functions and globals:** its symbol map (`config/GALE01/symbols.txt`) lists each name with its NTSC 1.02 address.
- **What a function does:** the decompiled C source under `src/melee/`, for example `gm/gmboot.c` and `gm/gmvsmode.c` above.
- **Struct layouts:** the `types.h` files beside the source.

Good hook targets are functions called at a clear moment: a scene's enter or leave, a match's start, a fighter's spawn, a menu's per-frame think. Log from the hook first to see when and how often it runs, then change behaviour.

Name every address in your source with a constant and a comment giving the decomp's file and function name. The next person to read your plugin, and the reviewer, will need it.

## Staying safe

- **Offline only.** Never hook networking or Slippi code. The runtime blocks the network anyway.
- **Change the least you can.** Standing behind a function and adjusting its result is much safer than replacing it.
- **Leave the memory card alone** unless that is the plugin's whole point, and then say so in the README. Unlock All, for example, opens the stages only while the select screens are up, and puts the flags back before the match starts.
- **Undo on exit.** If a hook changes a scene's flow, make sure the menus still work afterwards. Test the whole path: into the match, out to the results, back to the menus.
