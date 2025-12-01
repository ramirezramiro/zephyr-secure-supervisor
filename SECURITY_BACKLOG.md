# Security Backlog & Roadmap

This file captures the remaining security hardening tasks for zephyr-secure-supervisor. Use it to prioritize future work and explain current limitations to stakeholders.

## Provisioning & Key Management

- **Device-unique scalars/public keys** – Replace the RFC 7748 test vectors in `prj.conf` with per-device secrets. Implement a host CLI or factory jig that:
  - Generates/clamps a scalar.
  - Writes it to the board via a Zephyr shell command or bespoke provisioning binary that calls `persist_state_curve25519_get_secret()` with a “write” path.
  - Stores the server-side peer key + session counter baseline for decryption.
- **Audit trail** – Record which scalar was burned into each serial number / hardware ID, and capture the initial session counter so replay protection can be enforced.

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
