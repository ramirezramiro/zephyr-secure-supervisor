#include <zephyr/sys/util.h>

#include "watchdog_ctrl.h"

int watchdog_ctrl_init(uint32_t timeout_ms)
{
	ARG_UNUSED(timeout_ms);
	return 0;
}

int watchdog_ctrl_feed(void)
{
	return 0;
}

void watchdog_ctrl_set_enabled(bool enable)
{
	ARG_UNUSED(enable);
}

bool watchdog_ctrl_is_enabled(void)
{
	return true;
}

int watchdog_ctrl_retune(uint32_t timeout_ms)
{
	ARG_UNUSED(timeout_ms);
	return 0;
}

uint32_t watchdog_ctrl_get_timeout(void)
{
	return 0U;
}
