# Compatibility

| Profile mode | Allowed content |
|---|---|
| `vanilla` | No selected gameplay-changing content; useful for clean launch |
| `tournament-safe` | Online-safe capabilities only; unknown capabilities fail closed |
| `slippi` | Online-safe capabilities only; gameplay changes are blocked |
| `offline` | Offline gameplay, training and character packages may be selected |

The `online_safe` field is metadata. The host derives the effective result from capabilities and does not trust package claims.
