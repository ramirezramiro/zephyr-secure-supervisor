# Security Backlog & Roadmap

This file captures the remaining security hardening tasks for zephyr-secure-supervisor. Use it to prioritize future work and explain current limitations to stakeholders.

## Provisioning & Key Management

- **Device-unique scalars/public keys** – ✅ Automated via `tools/provision_curve.py` + `tools/update_provision_overlay.py` (see README). Future work: hook the scripts into factory infrastructure and ensure the generated values are copied to release artifacts.
- **Audit trail (TODO)** – Record which scalar was burned into each serial number / hardware ID, and capture the initial session counter so replay protection can be enforced.

## Tamper Logging & Secure Storage

- **Tamper events** – Add hooks in `src/app_crypto.c` / `persist_state.c` to log when scalar reads fail integrity checks or when NVS records flip unexpectedly. Persist those events so field logs show potential tampering.
- **Secure storage integration** – Evaluate moving scalars/keys into TrustZone or an external secure element (e.g., ATECC608A) on boards with more SRAM/flash. Document the migration plan once target hardware is picked.

## Key Rotation & Authentication Hooks

- **Scheduled rotation** – Provide an API that can bump the Curve25519 scalar (or AES key) on a controlled interval. Requires a provisioning channel plus receiver-side support to derive new shared secrets.
- **MAC authentication** – Today the MAC is CRC-based; plan for upgrading to a cryptographically strong MAC (e.g., HMAC or AEAD) once SRAM/flash budgets allow. List the code paths that must change (`sensor_hts221.c`, `app_crypto.c`).
- **Session counter persistence** – Harden `persist_state_next_session_counter()` to detect rollbacks (e.g., via monotonic counters or secure elements) once the hardware supports it.

## Documentation & Tooling

- Keep `README.md`, `docs/crypto_backends.md`, and `docs/deployment.md` in sync with the requirements above.
- When the provisioning jig/CLI exists, document its usage under `docs/provisioning.md` (placeholder) and link from the README.

## Future Work Snapshot

*Note:* The provisioning overlay already consumes ~96 % of the STM32L053R8’s 8 KB SRAM, so the items below should primarily target the production firmware (or larger MCUs) rather than trying to shoehorn more logic into the provisioning build.

1. **Secure storage** – Design how scalars/peers move from plain NVS to OTP/secure elements/encrypted blobs. Capture hardware requirements and migration steps.
2. **Tamper logging** – Persist events when NVS integrity checks fail or provisioning occurs unexpectedly. Expose the log via UART so field teams can detect tampering.
3. **Key rotation triggers** – Define the APIs and provisioning flow for rotating scalars on demand (factory reset, remote command, counter threshold). Ensure receivers can derive new shared secrets safely.
4. **Audit trail automation** – Tie the existing provisioning scripts into manufacturing records so each scalar/peer pair is traceable to a device ID.

Track progress on these items before declaring the security story complete.
