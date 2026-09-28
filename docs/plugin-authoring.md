# Plugin authoring

The plugin guide lives in [docs/plugins](plugins/README.md):

- [Getting started](plugins/getting-started.md): build the SDK template, install it, and see it in game.
- [plugin.json](plugins/plugin-json.md), [the host API](plugins/host-api.md), [reading the game](plugins/game-data.md), [drawing](plugins/hud.md), [input](plugins/input.md), [savestates](plugins/savestates.md), [hooks](plugins/hooks.md).
- [Testing](plugins/testing.md), [design](plugins/design.md), the [cookbook](plugins/cookbook.md).
- [Publishing and contributing](plugins/publishing.md): getting a plugin onto the plugin site.

## Static plugins (legacy Dolphin path)

The older static ABI (`plugin_init`/`plugin_shutdown`) is still validated by the host for Dolphin profiles. Under it, plugins are composed as PPC source into a disposable decompilation worktree and linked into a DOL. It is not dynamic loading. See [static-plugins.md](static-plugins.md).
