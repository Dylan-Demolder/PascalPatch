# ISO recomposition

Static plugin profiles can now build from a user-owned GALE01 Rev.02 ISO. The host:

1. builds the replacement DOL in a disposable decompilation worktree;
2. computes a new 32-byte-aligned FST position after the replacement DOL;
3. shifts the original FST and all recorded file offsets by the required delta;
4. writes a new staged ISO and atomically promotes it as the profile build;
5. never writes to the source ISO.

The recomposer changes only disc layout metadata and the DOL bytes. It preserves all other disc bytes. `dtk disc verify` accepts the resulting ISO as a lossless GameCube ISO, although its Redump hash is expected to differ.

Filesystem asset mods still require an extracted game directory. ISO recomposition is for static code/plugin profiles only.

Manual use:

```sh
PYTHONPATH=host/src python tooling/recompose_disc.py \
  /path/to/user/GALE01.iso /path/to/generated/main.dol /path/to/output.iso
```
