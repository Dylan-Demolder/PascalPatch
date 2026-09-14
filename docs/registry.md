# Registry trust and updates

Registry package entries continue to use deterministic SHA-256 hashes. Signed indexes add a second trust layer in `host/src/meleemod/registry_signing.py`.

## Signed format

An index has this signed payload:

```json
{"schema":"meleemod/registry-index/1","entries":[...]}
```

The document adds `key_id` and a base64 Ed25519 `signature`. JSON is canonicalized with sorted keys and compact separators before signing. Index entries are validated again after signature verification.

Trust metadata has schema `meleemod/trust/1` and records each key's public key, status, and optional validity window. Unknown, revoked, not-yet-valid, expired, malformed, or mismatched keys fail closed. A trusted key can sign a trust update that adds a replacement key, which supports explicit key rotation.

`fetch_https_index` requires HTTPS, enforces a bounded response size and timeout, verifies the signature before returning entries, and never writes unverified data. `update_https_index` verifies first and atomically replaces a signed cache. `install_remote` implements the bounded file-package path: it requires HTTPS, enforces a response limit and timeout, hashes the streamed bytes before staging, and atomically promotes only a matching package. Remote directory/archive extraction and package-specific validation remain deliberately separate capabilities.

The implementation has no third-party runtime dependency and is tested against the RFC 8032 Ed25519 test vector, tamper rejection, revocation, key rotation, HTTPS enforcement, and atomic cache updates.
