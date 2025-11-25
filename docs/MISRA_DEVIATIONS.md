# MISRA Deviation Log

## Directive 4.9 / Rule 20.10 – Function-like macros
The telemetry helpers in `src/log_utils.h` (`LOG_EVT`/`LOG_EVT_SIMPLE`) remain
function-like macros. They encapsulate Zephyr's logging backend so that each
module can emit compact `EVT,<tag>,<status>` strings without repeating format
code. Converting the helpers into functions would introduce dynamic formatting
overhead and break existing log filtering rules that inspect the macro name at
compile time. The macros are kept, but their usage is confined to this header
and documented here.

## Rule 14.4 – Infinite loops
The worker threads in `src/supervisor.c`, `src/recovery.c`, and
`src/uart_commands.c` still contain `while (1)` control loops. Each loop is the
core of a long-lived Zephyr thread that monitors system health, recovery
requests, or UART commands. These tasks are intentionally non-terminating so
that hardware watchdogs continue to be serviced. The threads include bounded
sleep calls (`k_msleep`/`k_event_wait`) to prevent starvation, and higher-level
APIs (e.g., `recovery_request`) control their behavior. They therefore retain
their infinite loops with this documented deviation.
