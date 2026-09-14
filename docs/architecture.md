# Architecture

```text
profile JSON + plugin/mod manifests
              │
              ▼
host validator → discovery → staged composer → atomic build
              │                              │
              │                              └── extracted game directory or untouched ISO
              ▼
       CLI / future GUI → Dolphin adapter

future: composed static PPC runtime + versioned SDK ABI
future: Character Studio exports validated .melee-character packages
```

The source ISO and source profile are never mutated. A build either promotes a complete staged directory or leaves the previous `current` build in place. Safety is derived from capabilities: unknown or gameplay-changing content is not accepted in Slippi or tournament-safe profiles.
