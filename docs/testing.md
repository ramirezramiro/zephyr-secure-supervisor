# zephyr-secure-supervisor Testing Guide

zephyr-secure-supervisor leans on Zephyr's `west build` flow and reuses production sources so watchdog, persistence, and crypto changes can be validated quickly. A condensed version of these instructions also lives in `tests/README.md` for reviewers who live inside the `tests/` tree.

## Native Simulation Suites
All native builds share the same pattern: configure into a scratch build directory, then run the resulting binary via `west build -t run`.

### Persistence State Machine
```
west build -b native_sim tests/persist_state -p auto --build-dir build/tests/persist_state
west build -t run --build-dir build/tests/persist_state
```
Covers NVS mount retries, reset counter bookkeeping, and watchdog override setters/clearers without touching hardware.

### Supervisor Logic
```
west build -b native_sim tests/supervisor -p auto --build-dir build/tests/supervisor
west build -t run --build-dir build/tests/supervisor
```
Exercises grace windows, LED/system heartbeat staleness math, and failure escalation thresholds.

## Hardware Ztests (nucleo_l053r8)
`tests/unit/misra_stage1` pulls in the production sources (safe memory wrappers, persistence, crypto, supervisor snapshot helpers, and recovery plumbing) and runs them as a Zephyr ztest app on the MCU.
```
west build -p always tests/unit/misra_stage1 -b nucleo_l053r8
west flash -r openocd
```
Use the same UART console (`/dev/ttyACM0 @ 115200`) to monitor EVT logs and confirm pass/fail output.

> Latest build footprint (`west build -p always tests/unit/misra_stage1 -b nucleo_l053r8 --build-dir build/tests/unit/misra_stage1`): `FLASH 34,360 B (52.43%)`, `RAM 3,976 B (48.54%)`, `SRAM0/IDT unused`. Handy for sizing MISRA-only drops when negotiating test firmware headroom.

> Sample UART summary (nucleo_l053r8, OpenOCD + `/dev/ttyACM0 @ 115200`):
> ```
> PASS misra_stage1.test_persist_state_encrypt_decrypt
> PASS misra_stage1.test_persist_state_plain_copy
> PASS misra_stage1.test_recovery_event_recording (logs EVT,RECOVERY,QUEUED for manual + health faults)
> PASS misra_stage1.test_safe_memory_helpers
> PASS misra_stage1.test_supervisor_health_snapshots
> TESTSUITE misra_stage1 succeeded (5/5, 0.017 s)
> ```
> Keep a full UART capture with your release notes for certification trails.

## Application Build + Flash
For end-to-end validation with the HTS221 sensor and watchdog feeding loop:
```
west build -b nucleo_l053r8 -p auto .
west flash -r openocd
sudo screen /dev/ttyACM0 115200
```

## Regression Checklist
- Run `tests/persist_state` after touching persistence or safe-memory helpers.
- Run `tests/supervisor` for changes to `supervisor.c`, `watchdog_ctrl.c`, or recovery signaling.
- Run `tests/unit/misra_stage1` whenever persistence, crypto, supervisor, or recovery files change (hardware guardrail, sub-minute turnaround).
- Capture UART logs for every hardware flash to ensure `EVT,<tag>,<status>` strings remain parsable.
