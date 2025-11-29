#ifndef PERSIST_STATE_H
#define PERSIST_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "curve25519_ref10.h"

int persist_state_init(void);
void persist_state_record_boot(bool watchdog_reset);
void persist_state_clear_watchdog_counter(void);

uint32_t persist_state_get_consecutive_watchdog(void);
uint32_t persist_state_get_total_watchdog(void);
bool persist_state_is_fallback_active(void);

uint32_t persist_state_get_watchdog_override(void);
int persist_state_set_watchdog_override(uint32_t timeout_ms);

int persist_state_curve25519_get_secret(uint8_t out[CURVE25519_KEY_SIZE]);
uint32_t persist_state_next_session_counter(void);

#if defined(CONFIG_ZTEST)
void persist_state_test_reset(void);
void persist_state_test_reload(void);
#endif

#endif /* PERSIST_STATE_H */
