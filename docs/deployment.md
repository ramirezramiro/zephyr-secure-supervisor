# Deployment Guidance

## Production Readiness

- Treat the Curve25519-backed build as a **minimum-viable implementation**. It demonstrates per-device scalars, per-boot session keys, and MAC tagging on an 8 KB MCU, but has almost no spare SRAM and no hardware-backed secure storage.
- The AES-only path (`CONFIG_APP_CRYPTO_BACKEND_AES=y`, Curve disabled) regains ~1 KB of RAM and skips the ladder. It can be production-ready for constrained deployments **only** when static AES keys are acceptable.
- **Provisioning requirement:** Replace the bundled RFC 7748 test vectors with device-unique scalars/public keys via a provisioning jig before deployment. See `docs/crypto_backends.md` for guidance and `SECURITY_BACKLOG.md` for the roadmap.

### Where Static AES Is Acceptable

- Lab DUTs and certification fixtures where devices live inside trusted facilities.
- Factory diagnostics jigs or burn-in setups that never leave controlled benches.
- Sensor nodes installed behind a physical security perimeter (locked cabinets, sealed enclosures) where replay/impersonation risk is minimal.

## When to Upsize the Hardware

- Need diagnostic headroom, multiple transports, or future PQC candidates.
- Require hardware roots of trust (secure boot, sealed storage, tamper detection).

**Boards with secure storage / isolation features:**

- **STM32U585 / STM32L562** – TrustZone + secure SRAM regions and PSA-certified crypto accelerators.
- **NXP LPC55S6x** – Physical Unclonable Function (PUF) key storage and CASPER accelerator.
- **Nordic nRF5340** – Dual-core design with a secure Key Management Unit.
- **External secure elements** – Add ATECC608A, OPTIGA Trust M, etc., to companion boards when MCU migration isn’t possible.

## Operating Guidance on the L053R8

- Keep per-device scalars provisioned via NVS and rotate session salts on every boot.
- Capture the UART `EVT,PQC,SESSION,...` log for audit trails; this is the only way to reproduce the derived AES/MAC keys.
- Use the Misra hardware ztests (`tests/unit/misra_stage1`) after every persistence/crypto change to ensure the tiny SRAM plan still holds.
- Track outstanding hardening tasks (tamper logging, key rotation hooks) in `SECURITY_BACKLOG.md` so deployments know the current limits.
