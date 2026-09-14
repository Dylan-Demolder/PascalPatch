# Static plugin composition

The first code-plugin path is source composition, not runtime relocatable loading. `tooling/build_static_plugin.py` uses the pinned tools from the decompilation checkout when available and creates a disposable Git worktree, generates one bundle source, includes that bundle into the linked `gmmain.c` unit, builds a GALE01 DOL and copies only the generated DOL to the requested output. The original checkout and extracted game tree are not modified.

Each plugin manifest must provide a C-identifier `entrypoint` and a relative `.c` `source`. Plugin source must use the target GameCube ABI. This is an explicit build-time code execution boundary; install only code you trust.

The adapter was validated with a harmless `OSReport` plugin. A generated marker was present in the output DOL. The real hello-plugin and PPC-safe runtime ABI also compile and link; its log string is present in the DOL. Plugin initialization is deferred until the first game-loop frame, after startup services are ready. The clean, static, and recomposed runtime-plugin ISOs boot to the same Melee memory-card prompt in standalone Dolphin 2606. Runtime callback output after accepting that prompt, shutdown ordering and dynamic modules remain separate work.
