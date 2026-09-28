# Contributing

**Writing a plugin?** You do not need to change this repository. Follow [docs/plugins](docs/plugins/README.md), then send it to the plugin site as described in [publishing](docs/plugins/publishing.md). Pull requests here are for PascalPatch itself: the runtime, the app, the SDK headers (new `melee.h` fields and helpers are welcome) and the docs.

Run the host and runtime tests before submitting changes. Do not commit Nintendo ISO/DOL files, extracted assets, emulator caches, or user paths. Changes that touch the runtime must document the target ABI and whether they were tested in a real emulator.

```sh
PYTHONPATH=host/src python -m unittest discover -s host/tests -v
gcc -std=c99 -Wall -Wextra -Werror -Iruntime/include runtime/src/events.c runtime/src/runtime.c runtime/tests/test_events.c -o /tmp/pascalpatch-test
/tmp/pascalpatch-test
```
