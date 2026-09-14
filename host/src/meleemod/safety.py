from __future__ import annotations
from .errors import ManifestError, ValidationError

ONLINE_MODES={"tournament-safe","slippi"}
def effective_compatibility(profile,plugins,mods):
    caps={c for p in plugins for c in p.get("capabilities",[])} | {m.get("capability","visual-only") for m in mods}
    if "unknown" in caps: return "unknown"
    if "gameplay-changing" in caps: return "offline-only"
    return "online-safe"

def validate_safety(profile,plugins,mods):
    errors=[]; eff=effective_compatibility(profile,plugins,mods)
    if profile.get("mode") in ONLINE_MODES and eff != "online-safe":
        errors.append(ValidationError("profile.mode","unsafe_combination",f"{eff} content cannot be launched in {profile['mode']} mode"))
    if profile.get("mode")=="tournament-safe" and profile.get("mode") not in {"tournament-safe"}:
        errors.append(ValidationError("profile.mode","mode","invalid tournament mode"))
    return eff,errors
