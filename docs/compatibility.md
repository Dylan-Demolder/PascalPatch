# Compatibility

| Profile mode | Allowed content | Native mechanism (retarget 2026-09-24) |
|---|---|---|
| `vanilla` | No selected gameplay-changing content; useful for clean launch | unmodified ISO, no user codes, no texture packs |
| `tournament-safe` | Online-safe capabilities only; unknown capabilities fail closed | presentation-only profile content; upstream already ships run-time optional codes (widescreen, PAL stock icons, no screen shake); every user code still passes MeleeMod capability classification |
| `slippi` | Online-safe capabilities only; gameplay changes are blocked | native Slippi netplay (login picked up from the Slippi Launcher); any change that alters simulation state would desync against Dolphin players — the port's checksum oracle only *detects* this (`DESYNC` in the log); prevention remains this project's classifier |
| `offline` | Offline gameplay, training and character packages may be selected | profile ISO plus baked profile Gecko table (Tier B); static C plugins only after the Tier C fork |

The `online_safe` field is metadata. The host derives the effective result from capabilities and does not trust package claims.

Rules for the native retarget:

- The port's own Gecko classifier answers only "can this code physically run
  in a translated build". It never answers online safety. MeleeMod's
  capability classification stays the fail-closed gate in every mode.
- Unknown capability names keep failing closed, in every tier.
- Tier A installs (texture packs, user codes) are run-time switchable, so
  enforcement happens at profile validate/build time, not at launch.
- Custom characters and gameplay-altering codes remain offline-only; nothing
  about the native runtime changes the checksum contract of Slippi netplay.
- No mode has been verified on the native runtime yet.
