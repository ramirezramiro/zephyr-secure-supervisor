# app_crypto.c

`src/app_crypto.c` sits between zephyr-secure-supervisor and the AES primitive (`simple_aes.c`). It provides a protocol-agnostic CTR helper so persistence and telemetry can enable encryption without knowing about block sizes or IV math. `CONFIG_APP_CRYPTO_BACKEND_*` lets you toggle between:

- **Pure AES backend** – the original flow where the key/IV come straight from Kconfig.
- **Curve25519 backend** – built on the vendored TweetNaCl ref10 ladder; it loads (or auto-generates) a device scalar from NVS, clamps it, mixes it with the peer public key, derives the shared secret, and from that secret derives both the AES key and a MAC key for each session.

## Responsibilities
- Initialize the cipher with a static key stored in Kconfig or board-specific data.
- Manage IVs/counters per payload, ensuring every telemetry frame includes both IV and ciphertext once encryption is enabled.
- Expose `app_crypto_encrypt()` that accepts plaintext buffers and returns sealed blobs suitable for persistence or logging.
- Guard all memory operations with `safe_memory` helpers to satisfy MISRA rules.

## Integrations
- Called by `sensor_hts221.c` once the plaintext sample threshold is met.
- Used by `persist_state.c` when storing reset counters or overrides (AES stays on even in Curve25519 mode because the shared secret becomes the AES key).
- Asks `persist_state` for the Curve25519 scalar; on first boot the scalar is seeded from `CONFIG_APP_CURVE25519_STATIC_SECRET_HEX` (if provided) or derived from the hardware device ID and stored in NVS so each board keeps a unique identity across reboots.
- For the curve backend, increments a session counter, draws a salt, derives AES + MAC keys from the shared secret, logs `EVT,PQC,SESSION,...`, and exposes `app_crypto_compute_sample_mac()` so telemetry and receivers share a keyed integrity check.
- Logs the derived Curve25519 public key so field engineers can confirm provisioning without dumping memory.

## Testing Hooks
`tests/unit/misra_stage1` exercises the encryption path on hardware by writing and reading back persistence records and telemetry frames.
