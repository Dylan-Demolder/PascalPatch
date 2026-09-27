from dataclasses import dataclass

@dataclass(frozen=True)
class ValidationError:
    path: str
    code: str
    message: str
    def as_dict(self): return {"path": self.path, "code": self.code, "message": self.message}

class PascalPatchError(Exception):
    def __init__(self, message, errors=()):
        super().__init__(message); self.errors=tuple(errors)

MeleeModError = PascalPatchError   # the name before the rename

class ManifestError(PascalPatchError): pass
class DiscoveryError(PascalPatchError): pass
class CompositionError(PascalPatchError): pass
