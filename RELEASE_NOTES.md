# Release Notes

## Curve25519 PoC – STM32 NUCLEO-L053R8

This snapshot captures the Curve25519-backed AES helper running on the 8 KB SRAM NUCLEO-L053R8. It should be treated as a PoC: Curve handles key derivation/MAC tagging, while AES-CTR still encrypts telemetry and persistence.

### Build Metadata

| Item | Value |
|------|-------|
| Commit | `35ca086216a861619b992cca1294fe67f295e4df` |
| Build command | `CCACHE_DISABLE=1 west build -b nucleo_l053r8 -p auto ../zephyr-apps/helium_tx` |
| West version | `v1.5.0` |
| SDK toolchain | `/home/mbbdev/zephyr-sdk-0.17.4/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc --version` → `Zephyr SDK 0.17.4 (gcc 12.2.0)` |
| Footprint | `FLASH 49 744 B (75.90 %)`, `RAM 7 400 B (90.33 %)` |

### Configuration Highlights

- Curve25519 backend enabled (`CONFIG_APP_CRYPTO_BACKEND_CURVE25519=y`, `CONFIG_APP_USE_CURVE25519=y`).
- AES helper retained to encrypt telemetry with the derived session key (`CONFIG_APP_USE_AES_ENCRYPTION=y`).
- Stack sizes tuned per the SRAM budget table in `README.md`.

### Provisioning Workflow Updates

- Added `tools/provision_curve.py` for generating/transmitting per-device scalars + peer keys, and `tools/update_provision_overlay.py` for copying the latest pair into `prj_provision.conf`.
- Provisioning builds now auto-persist those values to NVS on first boot (`app: Provision auto-persist secret=ok peer=ok`). See the sample UART capture in `docs/release_logs/v1.0/provision_auto_persist_uart.txt`.
- README.md includes quick decision charts plus the full command sequence; release artifacts now link directly to the UART log for easier auditing.

### Thread-fail Demonstration Overlay

- New overlay under `tests/thread_fail/` mirrors the production Curve25519 + AES setup but forces the HTS221 sensor stub to freeze after ten samples (five plaintext + five encrypted). The watchdog path, supervisor stacks, and crypto footprint remain unchanged, making it ideal for demos.
- Commands: `CCACHE_DISABLE=1 west build -b nucleo_l053r8 ../zephyr-apps/helium_tx/tests/thread_fail -d build/thread_fail`, `west flash -r openocd --build-dir build/thread_fail`, then `python3 -m serial.tools.miniterm /dev/ttyACM0 115200`.
- UART excerpt (full log in `docs/release_logs/v1.0/thread_fail_uart.txt`):
  ```
  [00:00:11.623,000] <inf> sensor_stub: Test stub: switching to encrypted telemetry
  [00:00:19.700,000] <wrn> sensor_stub: Test stub reached 10 samples; simulating hang
  [00:00:24.709,000] <wrn> supervisor: EVT,HEALTH,DEGRADED,fail=1,led=stale,led_age_ms=7023,hb=stale,hb_age_ms=7023
  [00:00:26.742,000] <err> supervisor: EVT,HEALTH,RECOVERY_REQUEST
  ... watchdog reset ...
  [00:00:01.605,000] <wrn> app: Reset cause: WATCHDOG
  ```

Build footprint (Curve + AES, no provisioning): `FLASH 49 036 B (74.82%)`, `RAM 7 400 B (90.33%)`.

### Logs & Tests

- Native simulation + hardware MISRA suites run with the Curve overlay (command recipes in `docs/testing.md` / `tests/README.md`).
- Hardware MISRA (Curve overlay) UART log:
  ```
  *** Booting Zephyr OS build v4.2.0-6484-g196a1da504bd ***
  Running TESTSUITE misra_stage1
  ===================================================================
  I: 32 Sectors of 128 bytes
  I: alloc wra: 21, 50
  I: data wra: 21, 3c
  I: Persistent state loaded: consecutive=0 total=0 override=0
  I: EVT,PQC,SESSION,counter=12,salt=0x47CB2DF5
  ===================================================================
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
- Application UART log (Curve backend enabling telemetry) to be appended when captured.
- Provisioning auto-persist UART log: `docs/release_logs/v1.0/provision_auto_persist_uart.txt` (attached to the GitHub release for download).

### Known Constraints

- Per-device Curve25519 scalars currently default to RFC 7748 vectors; provision real values before shipping hardware.
- No secure storage on STM32L053R8; treat this firmware as a lab/demo baseline. AES-only mode remains available for production-lite deployments with static keys.
