# Offline Character Studio package staging

Date: 2026-09-14. A temporary valid character package with a checksummed test asset was passed to `compose_validated_package(..., profile_mode="offline")`. The operation produced an atomic staging directory containing the validated authoring files and:

```json
{
  "game_integration": false,
  "profile_mode": "offline",
  "status": "validated-only"
}
```

The source package and staged output were outside all repositories. This verifies Offline authoring-workspace staging only. It does not compose Melee fighter assets or produce a playable character.
