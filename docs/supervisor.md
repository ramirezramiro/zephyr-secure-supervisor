# Supervisor Thread

`src/supervisor.c` encapsulates all watchdog feeding policy. It runs as a Zephyr thread with a configurable period (boot grace vs steady cadence) and owns the heartbeat atomics that other subsystems poke.

## Responsibilities
- Track boot grace windows so the MCU can settle peripherals before enforcing strict heartbeat checks.
- Maintain LED and system heartbeat timestamps (`atomic32_t` ages). Sensor work calls `supervisor_notify_led/system` whenever telemetry is real.
- Feed the watchdog only when **both** heartbeats are fresh (age < `CONFIG_APP_HEALTH_*_STALE_MS`).
- Increment failure counters whenever a heartbeat goes stale. After the configured threshold, signal recovery via `k_event_post` with `RECOVERY_EVT_HEALTH_FAULT`.
- Clear persistent watchdog counters once the system is healthy again, ensuring safe mode only engages after consecutive failures.

## Loop Outline
1. Sleep for the current window (boot grace uses the boot timeout, steady state uses the healthy timeout or persistent override).
2. Sample both heartbeat atomics and compute age.
3. If within boot grace, feed the watchdog unconditionally.
4. After boot grace:
   - If both heartbeats are fresh, feed the watchdog and reset failure counters.
   - Otherwise, increment the failure counter, log a degraded `EVT,WATCHDOG,...` line, and potentially request recovery.
5. If safe mode was active and the system proves healthy, clear the persistent watchdog counters via `persist_state_clear_watchdog_history`.

## Interfaces
- `supervisor_notify_led/system()`: called by sensor work or other producers to mark heartbeats fresh.
- `supervisor_request_manual_recovery()`: used by UART CLI when commands should immediately reboot.
- `supervisor_configure_from_overrides()`: reads persistent overrides (if any) and retunes the watchdog windows accordingly.

## Testing Hooks
The supervisor logic is covered by:
- `tests/supervisor` (native_sim) for grace windows, failure thresholds, and recovery escalation.
- `tests/unit/misra_stage1` (hardware) to ensure interactions with persistence and watchdog control remain deterministic under MISRA guardrails.
