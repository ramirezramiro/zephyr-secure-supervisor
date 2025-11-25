#ifndef LOG_UTILS_H
#define LOG_UTILS_H

#include <zephyr/logging/log.h>

/* Compact structured logging helpers.
 * Format: EVT,<tag>,<status>[,<kv pairs...>]
 */
#define LOG_EVT_SIMPLE(level, tag, status) \
	LOG_##level("EVT,%s,%s", tag, status)

#define LOG_EVT(level, tag, status, fmt, ...) \
	LOG_##level("EVT,%s,%s," fmt, tag, status, ##__VA_ARGS__)

#endif /* LOG_UTILS_H */
