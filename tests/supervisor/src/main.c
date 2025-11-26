#include <zephyr/ztest.h>

#include "supervisor_test.h"

static void supervisor_fixture_reset(void)
{
	supervisor_test_set_last_seen(0U, 0U);
}

ZTEST(supervisor_suite, test_led_and_hb_fresh)
{
	uint32_t now = 2000U;
	supervisor_test_set_last_seen(now - 100U, now - 150U);

	struct supervisor_health_snapshot snapshot = supervisor_test_sample(true, now);
	zassert_true(snapshot.led_ok, "LED should be considered fresh");
	zassert_true(snapshot.hb_ok, "Heartbeat should be considered fresh");
}

ZTEST(supervisor_suite, test_led_stale_when_monitored)
{
	uint32_t now = 10000U;
	supervisor_test_set_last_seen(0U, now - 100U);

	struct supervisor_health_snapshot snapshot = supervisor_test_sample(true, now);
	zassert_false(snapshot.led_ok, "LED should be stale when monitoring is enabled");
	zassert_true(snapshot.hb_ok, "Heartbeat remains fresh");
}

ZTEST(supervisor_suite, test_led_ignored_when_not_monitored)
{
	uint32_t now = 5000U;
	supervisor_test_set_last_seen(0U, now - 200U);

	struct supervisor_health_snapshot snapshot = supervisor_test_sample(false, now);
	zassert_true(snapshot.led_ok, "LED should be ignored when not monitored");
	zassert_true(snapshot.hb_ok, "Heartbeat still fresh");
}

ZTEST(supervisor_suite, test_heartbeat_stale)
{
	uint32_t now = 9000U;
	supervisor_test_set_last_seen(now - 100U, 0U);

	struct supervisor_health_snapshot snapshot = supervisor_test_sample(true, now);
	zassert_false(snapshot.hb_ok, "Heartbeat should be stale");
	zassert_true(snapshot.led_ok, "LED is still healthy");
}

ZTEST_SUITE(supervisor_suite, NULL, NULL, supervisor_fixture_reset, supervisor_fixture_reset, NULL);
