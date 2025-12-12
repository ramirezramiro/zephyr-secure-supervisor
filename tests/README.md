# zephyr-secure-supervisor Test Matrix

This file mirrors the commands in `docs/testing.md` so you can run the right suites without leaving the `tests/` tree.

## Native Simulation
| Suite | Command | What it verifies |
|-------|---------|------------------|
| `tests/persist_state` | `west build -b native_sim tests/persist_state -p auto --build-dir build/tests/persist_state && west build -t run --build-dir build/tests/persist_state` | NVS mount, reset counters, watchdog overrides |
| `tests/persist_state` (Curve overlay) | `west build -b native_sim tests/persist_state -p auto -DOVERLAY_CONFIG=prj_curve.conf --build-dir build/tests/persist_state_curve && west build -t run --build-dir build/tests/persist_state_curve` | Same as above but with Curve25519 scalar/session persistence |
| `tests/supervisor` | `west build -b native_sim tests/supervisor -p auto --build-dir build/tests/supervisor && west build -t run --build-dir build/tests/supervisor` | Grace windows, heartbeat staleness, recovery escalation |

## Hardware Ztests
```
west build -p always tests/unit/misra_stage1 -b nucleo_l053r8
west flash -r openocd
```
Validates safe-memory wrappers, AES persistence round-trips, supervisor snapshots, and recovery plumbing on target hardware.

Need to exercise the Curve backend on silicon? Run the same commands with the overlay:
```
west build -p always tests/unit/misra_stage1 -b nucleo_l053r8 -DOVERLAY_CONFIG=prj_curve.conf --build-dir build/tests/unit/misra_stage1_curve
west flash -r openocd --build-dir build/tests/unit/misra_stage1_curve
```

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
Curve25519 overlay log (`-DOVERLAY_CONFIG=prj_curve.conf`):
```
START - test_persist_state_encrypt_decrypt
 PASS - test_persist_state_encrypt_decrypt in 0.005 seconds
===================================================================
START - test_persist_state_plain_copy
 PASS - test_persist_state_plain_copy in 0.001 seconds
===================================================================
START - test_recovery_event_recording
W: EVT,RECOVERY,QUEUED,reason=1(manual recovery request)
W: EVT,RECOVERY,QUEUED,reason=0(persistent health fault)
 PASS - test_recovery_event_recording in 0.011 seconds
===================================================================
START - test_safe_memory_helpers
 PASS - test_safe_memory_helpers in 0.001 seconds
===================================================================
START - test_supervisor_health_snapshots
 PASS - test_supervisor_health_snapshots in 0.001 seconds
===================================================================
TESTSUITE misra_stage1 succeeded

------ TESTSUITE SUMMARY START ------

SUITE PASS - 100.00% [misra_stage1]: pass = 5, fail = 0, skip = 0, total = 5 duration = 0.019 seconds
 - PASS - [misra_stage1.test_persist_state_encrypt_decrypt] duration = 0.005 seconds
 - PASS - [misra_stage1.test_persist_state_plain_copy] duration = 0.001 seconds
 - PASS - [misra_stage1.test_recovery_event_recording] duration = 0.011 seconds
 - PASS - [misra_stage1.test_safe_memory_helpers] duration = 0.001 seconds
 - PASS - [misra_stage1.test_supervisor_health_snapshots] duration = 0.001 seconds

------ TESTSUITE SUMMARY END ------

===================================================================
PROJECT EXECUTION SUCCESSFUL
```
Archive the full log with release notes when sharing builds.

## Full Application
```
west build -b nucleo_l053r8 -p auto .
west flash -r openocd
sudo screen /dev/ttyACM0 115200
```
Run this after code changes to confirm HTS221 telemetry, AES logs, and watchdog feeding with real sensors.

**Thread-fail demo:** (reuses the production stacks + Curve25519-backed AES path)
```
cd ~/zephyrproject
source .venv/bin/activate
CCACHE_DISABLE=1 west build -b nucleo_l053r8 ../zephyr-apps/helium_tx/tests/thread_fail -d build/thread_fail
west flash -r openocd --build-dir build/thread_fail
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```
Expect five plaintext and five encrypted HTS221 samples, followed by `Test stub reached 10 samples; simulating hang`, degraded-health logs from the supervisor, and a watchdog reset.

For deeper explanations, troubleshooting tips, and regression checklists, open `docs/testing.md`.
