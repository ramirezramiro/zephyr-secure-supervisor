#ifndef SUPERVISOR_TEST_H
#define SUPERVISOR_TEST_H

#include <stdbool.h>
#include <stdint.h>

#if defined(CONFIG_ZTEST)
struct supervisor_health_snapshot {
	bool led_ok;
	bool hb_ok;
	uint32_t led_age;
	uint32_t hb_age;
};

void supervisor_test_set_last_seen(uint32_t led_last, uint32_t hb_last);
struct supervisor_health_snapshot supervisor_test_sample(bool monitor_led,
							 uint32_t now32);
#endif

#endif /* SUPERVISOR_TEST_H */
