"""Dependency-free Ed25519 signatures for registry indexes and trust metadata."""
from __future__ import annotations
import base64, hashlib, json, time
from typing import Callable

_Q = 2**255 - 19
_L = 2**252 + 27742317777372353535851937790883648493
_D = (-121665 * pow(121666, _Q - 2, _Q)) % _Q
_I = pow(2, (_Q - 1) // 4, _Q)
_MASK = (1 << 255) - 1
_B = (15112221349535400772501151409588531511454012693041857206046113283949847762202,
      46316835694926478169428394003475163141307993866256225615783033603165251855960,
      1,
      (15112221349535400772501151409588531511454012693041857206046113283949847762202 * 46316835694926478169428394003475163141307993866256225615783033603165251855960) % _Q)
_ID = (0, 1, 1, 0)


def _add(p, q):
    x1, y1, z1, t1 = p; x2, y2, z2, t2 = q
    a = ((y1 - x1) * (y2 - x2)) % _Q
    b = ((y1 + x1) * (y2 + x2)) % _Q
    c = (2 * _D * t1 * t2) % _Q
    d = (2 * z1 * z2) % _Q
    e = (b - a) % _Q; f = (d - c) % _Q; g = (d + c) % _Q; h = (b + a) % _Q
    return (e * f % _Q, g * h % _Q, f * g % _Q, e * h % _Q)


def _mul(point, scalar):
    result = _ID
    addend = point
    while scalar:
        if scalar & 1:
            result = _add(result, addend)
        addend = _add(addend, addend)
        scalar >>= 1
    return result


def _encode(point):
    x, y, z, _ = point
    zi = pow(z, _Q - 2, _Q)
    xx = (x * zi) % _Q; yy = (y * zi) % _Q
    return (yy | ((xx & 1) << 255)).to_bytes(32, "little")


def _decode(raw):
    if not isinstance(raw, bytes) or len(raw) != 32:
        raise ValueError("invalid Ed25519 point length")
    value = int.from_bytes(raw, "little")
    sign = value >> 255; y = value & _MASK
    if y >= _Q:
        raise ValueError("non-canonical Ed25519 point")
    xx = ((y * y - 1) * pow(_D * y * y + 1, _Q - 2, _Q)) % _Q
    x = pow(xx, (_Q + 3) // 8, _Q)
    if (x * x - xx) % _Q:
        x = (x * _I) % _Q
    if (x * x - xx) % _Q or (x == 0 and sign):
        raise ValueError("invalid Ed25519 point")
    if (x & 1) != sign:
        x = _Q - x
    return (x, y, 1, (x * y) % _Q)


def public_key(seed: bytes) -> bytes:
    if not isinstance(seed, bytes) or len(seed) != 32:
        raise ValueError("Ed25519 seed must contain 32 bytes")
    digest = hashlib.sha512(seed).digest()
    scalar = int.from_bytes(digest[:32], "little")
    scalar &= (1 << 254) - 8; scalar |= 1 << 254
    return _encode(_mul(_B, scalar))


def sign(seed: bytes, message: bytes) -> bytes:
    if not isinstance(message, bytes):
        raise TypeError("message must be bytes")
    digest = hashlib.sha512(seed).digest()
    scalar = int.from_bytes(digest[:32], "little")
    scalar &= (1 << 254) - 8; scalar |= 1 << 254
    public = _encode(_mul(_B, scalar)); nonce = int.from_bytes(hashlib.sha512(digest[32:] + message).digest(), "little") % _L
    rpoint = _encode(_mul(_B, nonce)); challenge = int.from_bytes(hashlib.sha512(rpoint + public + message).digest(), "little") % _L
    return rpoint + ((nonce + challenge * scalar) % _L).to_bytes(32, "little")


def verify(public: bytes, message: bytes, signature: bytes) -> bool:
    try:
        if not isinstance(public, bytes) or len(public) != 32 or not isinstance(signature, bytes) or len(signature) != 64:
            return False
        a = _decode(public); r = _decode(signature[:32]); s = int.from_bytes(signature[32:], "little")
        if s >= _L:
            return False
        challenge = int.from_bytes(hashlib.sha512(signature[:32] + public + message).digest(), "little") % _L
        left = _mul(_B, s); right = _add(r, _mul(a, challenge))
        return _encode(_mul(left, 8)) == _encode(_mul(right, 8))
    except (TypeError, ValueError, OverflowError):
        return False


def canonical_json(value: object) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def key_id(public: bytes) -> str:
    if not isinstance(public, bytes) or len(public) != 32:
        raise ValueError("public key must contain 32 bytes")
    return hashlib.sha256(public).hexdigest()[:16]


def _signed_payload(document: dict) -> bytes:
    return canonical_json({k: v for k, v in document.items() if k != "signature"})


def make_trust(keys: dict[str, dict]) -> dict:
    if not isinstance(keys, dict) or not keys:
        raise ValueError("trust store requires keys")
    return {"schema": "meleemod/trust/1", "keys": keys}


def sign_document(document: dict, seed: bytes, signer: str | None = None) -> dict:
    if not isinstance(document, dict):
        raise ValueError("signed document must be an object")
    public = public_key(seed); kid = signer or key_id(public)
    result = dict(document); result["key_id"] = kid
    result["signature"] = base64.b64encode(sign(seed, _signed_payload(result))).decode("ascii")
    return result


def _trusted_public(trust: dict, kid: str, now: int | None) -> bytes:
    if not isinstance(trust, dict) or trust.get("schema") != "meleemod/trust/1":
        raise ValueError("unsupported trust metadata")
    record = trust.get("keys", {}).get(kid)
    if not isinstance(record, dict) or record.get("status", "trusted") != "trusted":
        raise ValueError("registry signing key is not trusted")
    if now is None: now = int(time.time())
    if record.get("not_before") is not None and now < int(record["not_before"]): raise ValueError("registry key is not yet valid")
    if record.get("not_after") is not None and now > int(record["not_after"]): raise ValueError("registry key is expired")
    raw = bytes.fromhex(record.get("public_key", ""))
    if key_id(raw) != kid: raise ValueError("registry key ID mismatch")
    return raw


def verify_document(document: dict, trust: dict, now: int | None = None) -> dict:
    if not isinstance(document, dict) or document.get("key_id") is None or not isinstance(document.get("signature"), str):
        raise ValueError("signed registry document is incomplete")
    raw = base64.b64decode(document["signature"], validate=True)
    public = _trusted_public(trust, document["key_id"], now)
    if not verify(public, _signed_payload(document), raw):
        raise ValueError("registry signature verification failed")
    return {k: v for k, v in document.items() if k not in {"signature", "key_id"}}


def sign_index(entries: list[dict], seed: bytes, signer: str | None = None) -> dict:
    return sign_document({"schema": "meleemod/registry-index/1", "entries": entries}, seed, signer)


def verify_index(document: dict, trust: dict, now: int | None = None) -> list[dict]:
    payload = verify_document(document, trust, now)
    if payload.get("schema") != "meleemod/registry-index/1" or not isinstance(payload.get("entries"), list):
        raise ValueError("invalid registry index schema")
    from .registry import validate_entry
    for index, entry in enumerate(payload["entries"]):
        errors = validate_entry(entry)
        if errors:
            raise ValueError(f"invalid registry entry at index {index}")
    return payload["entries"]


def sign_trust_update(keys: dict[str, dict], seed: bytes, signer: str | None = None) -> dict:
    return sign_document({"schema": "meleemod/trust/1", "keys": keys}, seed, signer)


def verify_trust_update(document: dict, current: dict, now: int | None = None) -> dict:
    payload = verify_document(document, current, now)
    if payload.get("schema") != "meleemod/trust/1" or not isinstance(payload.get("keys"), dict) or not payload["keys"]:
        raise ValueError("invalid trust update schema")
    return payload


def fetch_index(fetch: Callable[[int], bytes], trust: dict, max_bytes: int = 1024 * 1024, now: int | None = None) -> list[dict]:
    raw = fetch(max_bytes + 1)
    if not isinstance(raw, bytes) or len(raw) > max_bytes:
        raise ValueError("registry index exceeds size limit")
    try: document = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc: raise ValueError("invalid registry index JSON") from exc
    return verify_index(document, trust, now)



def _fetch_https_document(url: str, timeout: float, max_bytes: int) -> dict:
    from urllib.parse import urlparse
    from urllib.request import Request, urlopen
    parsed = urlparse(url)
    if parsed.scheme != "https" or not parsed.netloc:
        raise ValueError("registry updates require an HTTPS URL")
    request = Request(url, headers={"Accept": "application/json", "User-Agent": "meleemod-registry/1"})
    with urlopen(request, timeout=timeout) as response:
        declared = response.headers.get("Content-Length")
        if declared is not None and int(declared) > max_bytes:
            raise ValueError("registry index exceeds size limit")
        raw = response.read(max_bytes + 1)
    if len(raw) > max_bytes:
        raise ValueError("registry index exceeds size limit")
    try: return json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc: raise ValueError("invalid registry index JSON") from exc


def fetch_https_index(url: str, trust: dict, timeout: float = 5.0, max_bytes: int = 1024 * 1024, now: int | None = None) -> list[dict]:
    """Fetch and verify an index without writing unverified data to disk."""
    return verify_index(_fetch_https_document(url, timeout, max_bytes), trust, now)


def update_https_index(url: str, cache, trust: dict, timeout: float = 5.0, max_bytes: int = 1024 * 1024, now: int | None = None):
    """Verify an HTTPS index, then atomically replace its signed local cache."""
    from pathlib import Path
    import os, tempfile
    document = _fetch_https_document(url, timeout, max_bytes)
    verify_index(document, trust, now)
    target = Path(cache).expanduser().resolve(); target.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix="." + target.name + ".", dir=target.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(document, stream, ensure_ascii=False, sort_keys=True, separators=(",", ":")); stream.write("\n"); stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, target)
    except Exception:
        try: os.unlink(temporary)
        except FileNotFoundError: pass
        raise
    return target
