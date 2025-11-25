# zephyr-secure-supervisor Test Matrix

This file mirrors the commands in `docs/testing.md` so you can run the right suites without leaving the `tests/` tree.

## Native Simulation
| Suite | Command | What it verifies |
|-------|---------|------------------|
| `tests/persist_state` | `west build -b native_sim tests/persist_state -p auto --build-dir build/tests/persist_state && west build -t run --build-dir build/tests/persist_state` | NVS mount, reset counters, watchdog overrides |
| `tests/supervisor` | `west build -b native_sim tests/supervisor -p auto --build-dir build/tests/supervisor && west build -t run --build-dir build/tests/supervisor` | Grace windows, heartbeat staleness, recovery escalation |

## Hardware Ztests
```
west build -p always tests/unit/misra_stage1 -b nucleo_l053r8
west flash -r openocd
```
Validates safe-memory wrappers, AES persistence round-trips, supervisor snapshots, and recovery plumbing on target hardware.

Latest footprint from that build: `FLASH 34,360 B (52.43%)`, `RAM 3,976 B (48.54%)`, `SRAM0/IDT unused`, so you know the ztest image easily fits the NUCLEO-L053R8 allocation.

UART pass signature (screen `/dev/ttyACM0 115200`):
```
PASS misra_stage1.test_persist_state_encrypt_decrypt
PASS misra_stage1.test_persist_state_plain_copy
PASS misra_stage1.test_recovery_event_recording
PASS misra_stage1.test_safe_memory_helpers
PASS misra_stage1.test_supervisor_health_snapshots
TESTSUITE misra_stage1 succeeded (5/5)
```
Archive the full log with release notes when sharing builds.

## Full Application
```
west build -b nucleo_l053r8 -p auto .
west flash -r openocd
sudo screen /dev/ttyACM0 115200
```
Run this after code changes to confirm HTS221 telemetry, AES logs, and watchdog feeding with real sensors.

For deeper explanations, troubleshooting tips, and regression checklists, open `docs/testing.md`.
