# watchdog_ctrl.c

`src/watchdog_ctrl.c` centralizes all STM32 Independent Watchdog (IWDG) operations. Keeping the hardware calls in a single file lets the rest of the firmware treat the watchdog as a policy object rather than a peripheral.

## Responsibilities
- Initialize the IWDG channel during boot via `watchdog_ctrl_init`, applying the “boot timeout” from Kconfig or overrides.
- Provide `watchdog_ctrl_feed` and `watchdog_ctrl_set_timeout` so the supervisor can safely service the watchdog without touching STM32 HAL symbols.
- Surface error codes (init failure, invalid timeouts) to the recovery thread so the system reboots if the IWDG cannot be owned.

## Key Behaviors
- Honors persistent overrides by accepting millisecond values supplied after `persist_state_read_watchdog_override`.
- Uses Zephyr’s watchdog API (`wdt_install_timeout`, `wdt_setup`) and keeps the single feed handle private.
- Issues an initial feed before starting the supervisor thread so the device has a grace window while other services come online.

When porting zephyr-secure-supervisor to a new MCU, this is the only file that should need hardware-specific updates; the supervisor, persistence, and telemetry stacks stay untouched.
