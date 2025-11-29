# SRAM & Flash Budget

The NUCLEO-L053R8 only exposes 8 KB of SRAM and 64 KB of flash, so fitting Curve25519 alongside watchdog, persistence, and sensor threads required aggressive tuning. Use this doc when adjusting stacks or deciding whether a new feature fits.

## Current Stack Plan

| Stack / feature | Value | Rationale |
|-----------------|-------|-----------|
| `CONFIG_MAIN_STACK_SIZE` | 1536 B | Minimum that keeps the Curve25519 ladder + HTS221 worker from overflowing. |
| `CONFIG_ISR_STACK_SIZE` | 768 B | Smallest value that survives sensor/crypto interrupts without violating guard regions. |
| System workqueue | 1280 B | Needed for NVS writes and delayed sensor work (lower values tripped stack guards). |
| Supervisor / Recovery / Sensor threads | 768 B / 384 B / 256 B | Restored to the smallest sizes that still pass hardware regression tests. |
| Optional features | Thread analyzer, UART CLI, extra crypto backends | Disabled to avoid their additional stacks/heap usage. Enable only when running on a bigger MCU. |

## Footprint Snapshot

```
RAM: 7400 B / 8 KB = 90.33%
FLASH: 49744 B / 64 KB = 75.90%
```

That leaves ~600 B of SRAM headroom for telemetry buffers and logging. When experimenting:

- Reduce log verbosity or keep the UART CLI disabled to reclaim ~300 B.
- Only enable the thread analyzer if you simultaneously grow `CONFIG_MAIN_STACK_SIZE` or switch back to AES-only mode.
- Expect to trade away an existing stack (or move to a >16 KB SRAM MCU) before adding new features such as additional sensors or crypto primitives.
