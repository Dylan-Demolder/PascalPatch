# Plugin authoring

The initial ABI is intentionally small and versioned. Plugins must expose `plugin_init` and `plugin_shutdown`, declare an API version and list capabilities.

The current host can validate manifests and produce a dependency-ordered static manifest. It does not yet link or load PPC plugins into a DOL. Do not advertise the hello plugin as runtime-loadable until Spike 002 and a PPC integration test pass.

Callbacks must be bounded, must not block the game loop, and must use ownership rules defined by the ABI headers.
