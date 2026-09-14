from dataclasses import dataclass

@dataclass(frozen=True)
class ValidationError:
    path: str
    code: str
    message: str
    def as_dict(self): return {"path": self.path, "code": self.code, "message": self.message}

class MeleeModError(Exception):
    def __init__(self, message, errors=()):
        super().__init__(message); self.errors=tuple(errors)

class ManifestError(MeleeModError): pass
class DiscoveryError(MeleeModError): pass
class CompositionError(MeleeModError): pass
