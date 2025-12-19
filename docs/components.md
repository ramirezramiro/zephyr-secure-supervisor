# Module Reference

This document summarizes zephyr-secure-supervisor source files that are not described in the dedicated supervisor, recovery, or architecture notes.

| File | Role | Notes |
|------|------|-------|
| `src/simple_aes.c` | Minimal AES block implementation used by CTR helpers. | Called only by `app_crypto.c`; safe-memory wrappers validate buffers before encrypt/decrypt. See `docs/simple_aes.md`. |
| `src/app_crypto.c` | CTR encryption + Curve25519 session orchestration. | Derives per-device scalars, mixes shared secrets into AES/MAC keys, logs PQC session info, and exposes encryption/MAC helpers. See `docs/app_crypto.md`. |
| `src/persist_state.c` | NVS mount/retry logic, watchdog overrides, reset counters, and Curve25519 secrets. | Stores boot stats plus the device scalar + session counter so crypto can survive reboots. See `docs/persist_state.md`. |
| `src/safe_memory.h` | Inline wrappers replacing raw `memcpy`/`memset`. | Ensures bounds checking for MISRA-inspired guardrails (used throughout persistence/crypto code). |
| `src/sensor_hts221.c` | Delayed work fetching HTS221 readings. | Talks to the HTS221 on the X-NUCLEO-IKS01A2 shield via `i2c1` @ `0x5F`, produces plaintext samples before enabling encryption, emits MAC-tagged frames in Curve25519 mode, toggles LED, and notifies supervisor heartbeats. See `docs/sensor_hts221.md`. |
| `src/uart_commands.c` | Optional UART CLI for watchdog overrides. | Implements `wdg?`, `wdg <ms>`, `wdg clear` commands and calls supervisor/persistence APIs. See `docs/uart_commands.md`. |
| `src/log_utils.h` | Structured logging macros. | Keeps the `EVT,<tag>,<status>` format while deferring to Zephyr `LOG_*` macros under the hood. See `docs/log_utils.md`. |
| `src/watchdog_ctrl.c` | STM32 IWDG ownership, timeout retune helpers, and initial feed. | Supervisor is the only client; provides boot vs steady window setters that honor persistent overrides. See `docs/watchdog_ctrl.md`. |

Use this table as a quick pointer when navigating the rest of the codebase or writing new docs.
