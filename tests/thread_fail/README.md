# Thread Fail Demo

This overlay mirrors the production build (same Curve25519 + AES configuration and stack sizes) but modifies the HTS221 sensor stub so it stops rescheduling itself after ten samples. Use it to demonstrate how the watchdog/supervisor respond when a real sensor thread wedges.

## Build & Flash
```
cd ~/zephyrproject
source .venv/bin/activate
CCACHE_DISABLE=1 west build -b nucleo_l053r8 ../zephyr-apps/helium_tx/tests/thread_fail -d build/thread_fail
west flash -r openocd --build-dir build/thread_fail
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

## Expected UART sequence (excerpt)
```
[00:00:11.623,000] <inf> sensor_stub: Test stub: switching to encrypted telemetry
[00:00:11.632,000] <inf> sensor_stub: EVT,SENSOR,HTS221_SAMPLE,enc=1,iv=CBAD87F13A1B00794A905C41,data=8979...
[00:00:19.700,000] <wrn> sensor_stub: Test stub reached 10 samples; simulating hang
[00:00:24.709,000] <wrn> supervisor: EVT,HEALTH,DEGRADED,fail=1,led=stale,led_age_ms=7023,hb=stale,hb_age_ms=7023
[00:00:26.742,000] <err> supervisor: EVT,HEALTH,RECOVERY_REQUEST
[00:00:26.807,000] <err> os: Current thread: 0x20000100 (supervisor)

*** Booting Zephyr OS build v4.2.0-6484-g196a1da504bd ***
[00:00:01.605,000] <wrn> app: Reset cause: WATCHDOG
[00:00:01.647,000] <wrn> app: EVT,WATCHDOG,RESET_HISTORY,consecutive=1,total=1
```
See `docs/release_logs/v1.0/thread_fail_uart.txt` for the full log with all samples.
