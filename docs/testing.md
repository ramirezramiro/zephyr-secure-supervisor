# zephyr-secure-supervisor Testing Guide

zephyr-secure-supervisor leans on Zephyr's `west build` flow and reuses production sources so watchdog, persistence, and crypto changes can be validated quickly. A condensed version of these instructions also lives in `tests/README.md` for reviewers who live inside the `tests/` tree.

## Native Simulation Suites
All native builds share the same pattern: configure into a scratch build directory, then run the resulting binary via `west build -t run`.

### Persistence State Machine
```
west build -b native_sim tests/persist_state -p auto --build-dir build/tests/persist_state
west build -t run --build-dir build/tests/persist_state
```
Covers NVS mount retries, reset counter bookkeeping, and watchdog override setters/clearers without touching hardware. Need to exercise the Curve25519 scalar/session flow? Add the overlay:
```
west build -b native_sim tests/persist_state -p auto -DOVERLAY_CONFIG=prj_curve.conf --build-dir build/tests/persist_state_curve
west build -t run --build-dir build/tests/persist_state_curve
```

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
To cover the Curve25519 backend on hardware, use the overlay:
```
west build -p always tests/unit/misra_stage1 -b nucleo_l053r8 -DOVERLAY_CONFIG=prj_curve.conf --build-dir build/tests/unit/misra_stage1_curve
west flash -r openocd --build-dir build/tests/unit/misra_stage1_curve
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

> Curve25519 overlay UART capture (`tests/unit/misra_stage1`, `OVERLAY_CONFIG=prj_curve.conf`):
> ```
> START - test_persist_state_encrypt_decrypt
>  PASS - test_persist_state_encrypt_decrypt in 0.005 seconds
> ===================================================================
> START - test_persist_state_plain_copy
>  PASS - test_persist_state_plain_copy in 0.001 seconds
> ===================================================================
> START - test_recovery_event_recording
> W: EVT,RECOVERY,QUEUED,reason=1(manual recovery request)
> W: EVT,RECOVERY,QUEUED,reason=0(persistent health fault)
>  PASS - test_recovery_event_recording in 0.011 seconds
> ===================================================================
> START - test_safe_memory_helpers
>  PASS - test_safe_memory_helpers in 0.001 seconds
> ===================================================================
> START - test_supervisor_health_snapshots
>  PASS - test_supervisor_health_snapshots in 0.001 seconds
> ===================================================================
> TESTSUITE misra_stage1 succeeded
> 
> ------ TESTSUITE SUMMARY START ------
> 
> SUITE PASS - 100.00% [misra_stage1]: pass = 5, fail = 0, skip = 0, total = 5 duration = 0.019 seconds
>  - PASS - [misra_stage1.test_persist_state_encrypt_decrypt] duration = 0.005 seconds
>  - PASS - [misra_stage1.test_persist_state_plain_copy] duration = 0.001 seconds
>  - PASS - [misra_stage1.test_recovery_event_recording] duration = 0.011 seconds
>  - PASS - [misra_stage1.test_safe_memory_helpers] duration = 0.001 seconds
>  - PASS - [misra_stage1.test_supervisor_health_snapshots] duration = 0.001 seconds
> 
> ------ TESTSUITE SUMMARY END ------
> 
> ===================================================================
> PROJECT EXECUTION SUCCESSFUL
> ```

## Application Build + Flash
For end-to-end validation with the HTS221 sensor and watchdog feeding loop:
```
west build -b nucleo_l053r8 -p auto .
west flash -r openocd
sudo screen /dev/ttyACM0 115200
```

## Thread-fail demo (nucleo_l053r8)

When you need to demo watchdog recovery caused by a wedged sensor thread (without forking production code), build the overlay in `tests/thread_fail/`. It shares the Curve25519-backed AES path and stack sizes with the shipping image; the only change is that the HTS221 stub stops posting work after ten samples.

```
cd ~/zephyrproject
source .venv/bin/activate
CCACHE_DISABLE=1 west build -b nucleo_l053r8 ../zephyr-apps/helium_tx/tests/thread_fail -d build/thread_fail
west flash -r openocd --build-dir build/thread_fail
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

Expected UART cues:

- Five plaintext HTS221 samples followed by `Test stub: switching to encrypted telemetry`.
- Five AES-tagged samples (`enc=1` logs) then `Test stub reached 10 samples; simulating hang`.
- Supervisor logs degraded health (`EVT,HEALTH,DEGRADED`) and requests recovery; the watchdog reboots the MCU.
- Next boot shows `Reset cause: WATCHDOG` and the history counter increments.

Build footprint for reference: `FLASH 49 036 B (74.82%)`, `RAM 7 400 B (90.33%)`.

## Regression Checklist
- Run `tests/persist_state` after touching persistence or safe-memory helpers.
- Run `tests/supervisor` for changes to `supervisor.c`, `watchdog_ctrl.c`, or recovery signaling.
- Run `tests/unit/misra_stage1` whenever persistence, crypto, supervisor, or recovery files change (hardware guardrail, sub-minute turnaround).
- Capture UART logs for every hardware flash to ensure `EVT,<tag>,<status>` strings remain parsable.
