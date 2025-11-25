# Module Reference

This document summarizes zephyr-secure-supervisor source files that are not described in the dedicated supervisor, recovery, or architecture notes.

| File | Role | Notes |
|------|------|-------|
| `src/simple_aes.c` | Minimal AES block implementation used by CTR helpers. | Called only by `app_crypto.c`; safe-memory wrappers validate buffers before encrypt/decrypt. See `docs/simple_aes.md`. |
| `src/app_crypto.c` | CTR encryption orchestration for persistence and telemetry payloads. | Provides `app_crypto_init`, `app_crypto_encrypt`, and IV bookkeeping that higher-level code consumes without touching AES internals. See `docs/app_crypto.md`. |
| `src/persist_state.c` | NVS mount/retry logic, watchdog overrides, and reset counter management. | Interfaces with Zephyr NVS; exposes APIs consumed by main, supervisor, and UART CLI. See `docs/persist_state.md`. |
| `src/safe_memory.h` | Inline wrappers replacing raw `memcpy`/`memset`. | Ensures bounds checking for MISRA-inspired guardrails (used throughout persistence/crypto code). |
| `src/sensor_hts221.c` | Delayed work fetching HTS221 readings. | Produces plaintext samples before enabling encryption, toggles LED, and notifies supervisor heartbeats. See `docs/sensor_hts221.md`. |
| `src/uart_commands.c` | Optional UART CLI for watchdog overrides. | Implements `wdg?`, `wdg <ms>`, `wdg clear` commands and calls supervisor/persistence APIs. See `docs/uart_commands.md`. |
| `src/log_utils.h` | Structured logging macros. | Keeps the `EVT,<tag>,<status>` format while deferring to Zephyr `LOG_*` macros under the hood. See `docs/log_utils.md`. |
| `src/watchdog_ctrl.c` | STM32 IWDG ownership, timeout retune helpers, and initial feed. | Supervisor is the only client; provides boot vs steady window setters that honor persistent overrides. See `docs/watchdog_ctrl.md`. |

Use this table as a quick pointer when navigating the rest of the codebase or writing new docs.
