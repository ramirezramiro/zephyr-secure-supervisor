# log_utils.h

`src/log_utils.h` defines the `LOG_EVT_SIMPLE` and `LOG_EVT` macros that give zephyr-secure-supervisor its `EVT,<tag>,<status>,key=value...` log lines.

## Why Macros?
Zephyr’s logging backend expects compile-time levels and module IDs. Wrapping the helpers as macros lets the code emit compact CSV-style strings without re-implementing `LOG_INF`/`LOG_ERR` everywhere, even though it triggers a MISRA deviation (documented in `MISRA_DEVIATIONS.md`).

## Usage Pattern
```
LOG_EVT_SIMPLE("WATCHDOG", "HEALTHY");
LOG_EVT("SENSOR", "ENC", "iv=%s,cipher=%s", iv_hex, cipher_hex);
```
Each subsystem sticks to a consistent tag (`WATCHDOG`, `RECOVERY`, `SENSOR`, `PERSIST`, etc.) so downstream tooling can parse logs deterministically.

## Guidance
- Keep payloads short—these logs are consumed over UART at 115200 bps.
- Add new tags sparingly and document them in module-specific docs so integrations stay informed.
