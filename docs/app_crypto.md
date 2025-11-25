# app_crypto.c

`src/app_crypto.c` sits between zephyr-secure-supervisor and the AES primitive (`simple_aes.c`). It provides a protocol-agnostic CTR helper so persistence and telemetry can enable encryption without knowing about block sizes or IV math.

## Responsibilities
- Initialize the cipher with a static key stored in Kconfig or board-specific data.
- Manage IVs/counters per payload, ensuring every telemetry frame includes both IV and ciphertext once encryption is enabled.
- Expose `app_crypto_encrypt()` that accepts plaintext buffers and returns sealed blobs suitable for persistence or logging.
- Guard all memory operations with `safe_memory` helpers to satisfy MISRA rules.

## Integrations
- Called by `sensor_hts221.c` once the plaintext sample threshold is met.
- Used by `persist_state.c` when storing reset counters or overrides if `CONFIG_APP_USE_AES_ENCRYPTION=y`.
- Plugs directly into the PQC experiments described in the README because the rest of the stack never touches AES internals.

## Testing Hooks
`tests/unit/misra_stage1` exercises the encryption path on hardware by writing and reading back persistence records and telemetry frames.
