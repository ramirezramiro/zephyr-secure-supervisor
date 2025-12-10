/* main.c - NUCLEO-L053R8: Watchdog + Supervisors */
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "app_crypto.h"
#include "log_utils.h"
#include "persist_state.h"
#include "recovery.h"
#include "sensor_hts221.h"
#include "supervisor.h"
#include "uart_commands.h"
#include "watchdog_ctrl.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#if IS_ENABLED(CONFIG_APP_PROVISION_AUTO_PERSIST)
static int hex_digit_value(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return 10 + (c - 'a');
	}
	if (c >= 'A' && c <= 'F') {
		return 10 + (c - 'A');
	}
	return -1;
}

static int decode_hex_key(const char *hex, uint8_t out[CURVE25519_KEY_SIZE])
{
	if (hex == NULL) {
		return -EINVAL;
	}

	size_t len = strlen(hex);
	if (len != CURVE25519_KEY_SIZE * 2U) {
		return -EINVAL;
	}

	for (size_t i = 0U; i < CURVE25519_KEY_SIZE; i++) {
		char hi = hex[i * 2U];
		char lo = hex[i * 2U + 1U];
		int hi_val = hex_digit_value(hi);
		int lo_val = hex_digit_value(lo);
		if (hi_val < 0 || lo_val < 0) {
			return -EINVAL;
		}
		out[i] = (uint8_t)((hi_val << 4) | lo_val);
	}

	return 0;
}

static void autoload_curve_keys(void)
{
	uint8_t buf[CURVE25519_KEY_SIZE];
	bool secret_written = false;
	bool peer_written = false;

	if (CONFIG_APP_CURVE25519_STATIC_SECRET_HEX[0] != '\0' &&
	    decode_hex_key(CONFIG_APP_CURVE25519_STATIC_SECRET_HEX, buf) == 0) {
		if (persist_state_curve25519_set_secret(buf) == 0) {
			secret_written = true;
		}
	}

	if (CONFIG_APP_CURVE25519_STATIC_PEER_PUB_HEX[0] != '\0' &&
	    decode_hex_key(CONFIG_APP_CURVE25519_STATIC_PEER_PUB_HEX, buf) == 0) {
		if (persist_state_curve25519_set_peer(buf) == 0) {
			peer_written = true;
		}
	}

	LOG_INF("Provision auto-persist secret=%s peer=%s",
		secret_written ? "ok" : "skipped",
		peer_written ? "ok" : "skipped");
}
#endif

/* Optional reset cause printout */
static uint32_t log_reset_cause(void)
{
	uint32_t cause = 0;
	if (hwinfo_get_reset_cause(&cause) == 0) {
		if (cause & RESET_WATCHDOG) LOG_WRN("Reset cause: WATCHDOG");
		if (cause & RESET_SOFTWARE) LOG_WRN("Reset cause: SOFTWARE");
		if (cause & RESET_POR)      LOG_WRN("Reset cause: POWER-ON");
		hwinfo_clear_reset_cause();
	}
	return cause;
}

void main(void)
{
	k_thread_name_set(k_current_get(), "main");
	LOG_EVT_SIMPLE(INF, "APP", "START");

	int ret = app_crypto_init();
	if (ret) {
		LOG_ERR("AES helper init failed: %d", ret);
	}

	ret = persist_state_init();
	if (ret) {
		LOG_ERR("Persistent state init failed: %d", ret);
	}

#if IS_ENABLED(CONFIG_APP_PROVISION_AUTO_PERSIST)
	autoload_curve_keys();
#endif

	uint32_t reset_cause = log_reset_cause();
	bool watchdog_reset = (reset_cause & RESET_WATCHDOG) != 0U;
	persist_state_record_boot(watchdog_reset);

	uint32_t consecutive = persist_state_get_consecutive_watchdog();
	if (consecutive != 0U) {
		LOG_EVT(WRN, "WATCHDOG", "RESET_HISTORY",
			"consecutive=%u,total=%u", consecutive, persist_state_get_total_watchdog());
	}

	bool safe_mode_active = persist_state_is_fallback_active();
	if (safe_mode_active) {
		LOG_EVT_SIMPLE(ERR, "SAFE_MODE", "ENTERED");
		persist_state_clear_watchdog_counter();
		LOG_EVT_SIMPLE(INF, "WATCHDOG", "COUNTER_CLEARED");
	}

    if (!IS_ENABLED(CONFIG_APP_PROVISION_BUILD)) {
        recovery_start();
        recovery_schedule_safe_mode_reboot(
            safe_mode_active ? CONFIG_APP_SAFE_MODE_REBOOT_DELAY_MS : 0U);

        uint32_t boot_timeout_ms = CONFIG_APP_WATCHDOG_BOOT_TIMEOUT_MS;
        uint32_t steady_timeout_ms = persist_state_get_watchdog_override();
        if (steady_timeout_ms == 0U) {
            steady_timeout_ms = CONFIG_APP_WATCHDOG_STEADY_TIMEOUT_MS;
        }
        uint32_t retune_delay_ms = CONFIG_APP_WATCHDOG_RETUNE_DELAY_MS;

        if (safe_mode_active) {
            steady_timeout_ms = MAX(steady_timeout_ms, boot_timeout_ms);
            retune_delay_ms = 0U;
        }

        ret = watchdog_ctrl_init(boot_timeout_ms);
        if (ret) {
            LOG_EVT(ERR, "WATCHDOG", "INIT_FAIL", "rc=%d", ret);
            LOG_EVT_SIMPLE(ERR, "RECOVERY", "WATCHDOG_INIT_FAIL");
            recovery_request(RECOVERY_REASON_WATCHDOG_INIT_FAIL);
            return;
        }

        LOG_EVT(INF, "WATCHDOG", "CONFIGURED",
            "boot_ms=%u,steady_ms=%u,retune_delay_ms=%u", boot_timeout_ms,
            steady_timeout_ms, retune_delay_ms);
        if (safe_mode_active) {
            LOG_EVT_SIMPLE(WRN, "WATCHDOG", "RETUNE_DISABLED_SAFE_MODE");
        }

        supervisor_start(steady_timeout_ms, retune_delay_ms, true);
    } else {
        LOG_INF("Provisioning build: watchdog/supervisor disabled");
    }

#if !IS_ENABLED(CONFIG_APP_PROVISION_BUILD)
	ret = sensor_hts221_start(safe_mode_active);
	if (ret) {
		LOG_EVT(ERR, "SENSOR", "HTS221_INIT_FAIL", "rc=%d", ret);
	}
#else
	LOG_INF("Skipping HTS221 sensor thread while provisioning build is enabled");
#endif

	if (IS_ENABLED(CONFIG_APP_ENABLE_UART_COMMANDS)) {
		uart_commands_start(safe_mode_active);
	}

	/* small delay to let logging flush before threads settle */
	k_msleep(120);

	LOG_EVT_SIMPLE(INF, "APP", "READY");

}
