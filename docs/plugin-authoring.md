# Plugin authoring

The initial ABI is intentionally small and versioned. Plugins must expose `plugin_init` and `plugin_shutdown`, declare an API version and list capabilities.

The current host validates manifests, produces a dependency-ordered static manifest, composes trusted PPC source into a disposable decompilation worktree, and links it into a DOL. Static composition is not dynamic runtime loading. The first-party hello and frame-probe plugins have direct standalone-Dolphin evidence, but arbitrary plugins still require their own bounded validation.

Callbacks must be bounded, must not block the game loop, and must use ownership rules defined by the ABI headers.
