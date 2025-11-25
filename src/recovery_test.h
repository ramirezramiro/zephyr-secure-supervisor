#ifndef RECOVERY_TEST_H
#define RECOVERY_TEST_H

#include <stdint.h>

#if defined(CONFIG_ZTEST)
void recovery_test_init_event(void);
uint32_t recovery_test_get_pending_events(void);
void recovery_test_clear_pending_events(void);
#endif

#endif /* RECOVERY_TEST_H */
