#include "uart_commands.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "log_utils.h"
#include "persist_state.h"
#include "supervisor.h"
#include "watchdog_ctrl.h"

LOG_MODULE_REGISTER(uart_cmd, LOG_LEVEL_INF);

#if IS_ENABLED(CONFIG_APP_ENABLE_UART_COMMANDS)

#if IS_ENABLED(CONFIG_APP_PROVISION_BUILD)
#define CMD_STACK_SIZE CONFIG_APP_PROVISION_CMD_STACK
#define CMD_BUFFER_LEN CONFIG_APP_PROVISION_CMD_BUFFER
#else
#define CMD_STACK_SIZE 288
#if IS_ENABLED(CONFIG_APP_CRYPTO_BACKEND_CURVE25519)
/* prov curve <scalar> [peer] plus delimiters. */
#define CMD_BUFFER_LEN (16 + (CURVE25519_KEY_SIZE * 4))
#else
#define CMD_BUFFER_LEN 48
#endif
#endif
#define CMD_THREAD_PRIORITY 9

K_THREAD_STACK_DEFINE(cmd_stack, CMD_STACK_SIZE);
static struct k_thread cmd_thread;
static bool cmd_safe_mode_active;
static const struct device *uart_dev;

static const char *skip_spaces(const char *s);

#if IS_ENABLED(CONFIG_APP_CRYPTO_BACKEND_CURVE25519)
static char prov_accum[CMD_BUFFER_LEN];
static size_t prov_accum_len;

static void prov_accum_reset(void)
{
	prov_accum_len = 0U;
	prov_accum[0] = '\0';
}

static void prov_accum_abort(void)
{
	if (prov_accum_len > 0U) {
		LOG_EVT(WRN, "PROVISION", "INCOMPLETE_CHUNK",
			"len=%zu,head=%.*s",
			prov_accum_len,
			(int)MIN(prov_accum_len, 16U),
			prov_accum);
		prov_accum_reset();
	}
}

static void prov_accum_append(const char *chunk)
{
	if (prov_accum_len >= sizeof(prov_accum) - 1U) {
		LOG_EVT(WRN, "PROVISION", "BUFFER_OVERFLOW", "len=%zu", prov_accum_len);
		prov_accum_reset();
		return;
	}

	if (prov_accum_len != 0U) {
		prov_accum[prov_accum_len++] = ' ';
	}

	while (*chunk != '\0' && prov_accum_len < sizeof(prov_accum) - 1U) {
		prov_accum[prov_accum_len++] = *chunk++;
	}

	if (*chunk != '\0') {
		LOG_EVT(WRN, "PROVISION", "BUFFER_TRUNCATED", "");
		prov_accum_len = sizeof(prov_accum) - 1U;
	}

	prov_accum[prov_accum_len] = '\0';
	LOG_DBG("prov_accum len=%zu head=%.*s",
		prov_accum_len,
		(int)MIN(prov_accum_len, 32U),
		prov_accum);
}

static bool prov_command_complete(const char *line)
{
	const char *cursor = skip_spaces(line);

	if (strncmp(cursor, "prov", 4) != 0) {
		return true;
	}

	cursor = skip_spaces(cursor + 4);
	if (strncmp(cursor, "curve", 5) != 0) {
		return true;
	}

	cursor = skip_spaces(cursor + 5);

	size_t scalar_len = 0U;
	while (cursor[scalar_len] != '\0' &&
	       !isspace((unsigned char)cursor[scalar_len])) {
		scalar_len++;
	}

	if (scalar_len < CURVE25519_KEY_SIZE * 2U) {
		return false;
	}

	cursor = skip_spaces(cursor + scalar_len);
	if (*cursor == '\0') {
		return true;
	}

	size_t peer_len = 0U;
	while (cursor[peer_len] != '\0' &&
	       !isspace((unsigned char)cursor[peer_len])) {
		peer_len++;
	}

	if (peer_len < CURVE25519_KEY_SIZE * 2U) {
		return false;
	}

	return true;
}
#endif

static void print_status(void)
{
	uint32_t boot_timeout = CONFIG_APP_WATCHDOG_BOOT_TIMEOUT_MS;
	uint32_t steady_target = supervisor_get_watchdog_target();
	uint32_t current_hw = watchdog_ctrl_get_timeout();
	uint32_t override = persist_state_get_watchdog_override();
	uint32_t consecutive = persist_state_get_consecutive_watchdog();

	LOG_EVT(INF, "TELEMETRY", "WATCHDOG_STATUS",
		"boot_ms=%u,current_ms=%u,target_ms=%u,override_ms=%u,fallback=%s",
		boot_timeout, current_hw, steady_target, override,
		cmd_safe_mode_active ? "yes" : "no");
	if (consecutive != 0U) {
		LOG_EVT(INF, "TELEMETRY", "WATCHDOG_RESETS", "count=%u", consecutive);
	}
}

static void apply_timeout(uint32_t timeout_ms)
{
	if (timeout_ms < 100U || timeout_ms > 60000U) {
		LOG_WRN("Timeout %u ms out of range (100-60000)", timeout_ms);
		return;
	}

	int rc = persist_state_set_watchdog_override(timeout_ms);
	if (rc < 0) {
		LOG_ERR("Failed to persist watchdog override: %d", rc);
		return;
	}

	supervisor_request_watchdog_target(timeout_ms, true);
	LOG_EVT(INF, "WATCHDOG", "OVERRIDE_SET", "timeout_ms=%u", timeout_ms);
}

static void clear_override(void)
{
	int rc = persist_state_set_watchdog_override(0U);
	if (rc < 0) {
		LOG_ERR("Failed to clear watchdog override: %d", rc);
		return;
	}

	supervisor_request_watchdog_target(CONFIG_APP_WATCHDOG_STEADY_TIMEOUT_MS, true);
	LOG_EVT(INF, "WATCHDOG", "OVERRIDE_CLEARED",
		"steady_ms=%u", CONFIG_APP_WATCHDOG_STEADY_TIMEOUT_MS);
}

#if IS_ENABLED(CONFIG_APP_CRYPTO_BACKEND_CURVE25519)
static int hex_value_cli(char c)
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

static int decode_hex_token(const char *token, size_t len,
			    uint8_t *out, size_t out_len)
{
	if (token == NULL || len != (out_len * 2U)) {
		return -EINVAL;
	}

	for (size_t i = 0U; i < out_len; i++) {
		int hi = hex_value_cli(token[i * 2U]);
		int lo = hex_value_cli(token[i * 2U + 1U]);

		if (hi < 0 || lo < 0) {
			return -EINVAL;
		}

		out[i] = (uint8_t)((hi << 4) | lo);
	}

	return 0;
}

static const char *skip_spaces(const char *s)
{
	while (*s != '\0' && isspace((unsigned char)*s)) {
		s++;
	}
	return s;
}

static void handle_provision_command(const char *args)
{
	args = skip_spaces(args);
	if (strncmp(args, "curve", 5) != 0) {
		LOG_EVT(WRN, "PROVISION", "UNKNOWN_TARGET", "body=%s", args);
		return;
	}

	args = skip_spaces(args + 5);
	if (*args == '\0') {
		LOG_EVT(WRN, "PROVISION", "MISSING_SCALAR", "");
		return;
	}

	const char *scalar_tok = args;
	size_t scalar_len = 0U;
	while (args[scalar_len] != '\0' && !isspace((unsigned char)args[scalar_len])) {
		scalar_len++;
	}

	args = skip_spaces(args + scalar_len);
	const char *peer_tok = NULL;
	size_t peer_len = 0U;
	if (*args != '\0') {
		peer_tok = args;
		while (args[peer_len] != '\0' && !isspace((unsigned char)args[peer_len])) {
			peer_len++;
		}
	}

	LOG_EVT(INF, "PROVISION", "CURVE_CMD_RX",
		"scalar_len=%zu,peer_len=%zu,raw=%.*s",
		scalar_len, peer_len, (int)(scalar_len + (peer_tok ? peer_len + 1U : 0U)), scalar_tok);

	uint8_t buffer[CURVE25519_KEY_SIZE];
	if (decode_hex_token(scalar_tok, scalar_len, buffer, CURVE25519_KEY_SIZE) != 0) {
		LOG_EVT(WRN, "PROVISION", "SCALAR_PARSE_FAIL", "len=%zu", scalar_len);
		return;
	}

	int rc = persist_state_curve25519_set_secret(buffer);
	if (rc != 0) {
		LOG_ERR("Failed to persist Curve25519 scalar: %d", rc);
		return;
	}

	if (peer_tok != NULL) {
		if (peer_len != CURVE25519_KEY_SIZE * 2U) {
			LOG_EVT(WRN, "PROVISION", "PEER_LEN_BAD", "len=%zu", peer_len);
			return;
		}
		if (decode_hex_token(peer_tok, peer_len, buffer, CURVE25519_KEY_SIZE) != 0) {
			LOG_EVT(WRN, "PROVISION", "PEER_PARSE_FAIL", "");
			return;
		}
		rc = persist_state_curve25519_set_peer(buffer);
		if (rc != 0) {
			LOG_ERR("Failed to persist Curve25519 peer key: %d", rc);
			return;
		}
	}

	LOG_EVT(INF, "PROVISION", "CURVE25519_UPDATED",
		"peer_updated=%s", peer_tok ? "yes" : "no");
	LOG_INF("Reboot the board to load the new Curve25519 material");
}
#endif

static void handle_line(const char *line)
{
	size_t raw_len = strlen(line);
	LOG_HEXDUMP_INF(line, raw_len, "UART_CMD raw line");
	size_t preview_len = raw_len < 64U ? raw_len : 64U;
	char preview[65];
	memcpy(preview, line, preview_len);
	preview[preview_len] = '\0';
	LOG_INF("UART_CMD line_str=%s%s", preview,
		raw_len > preview_len ? "…" : "");

	while (isspace((unsigned char)*line)) {
		line++;
	}

	if (*line == '\0') {
		return;
	}

#if IS_ENABLED(CONFIG_APP_CRYPTO_BACKEND_CURVE25519)
	if (prov_accum_len > 0U && strncmp(line, "prov", 4) != 0) {
		prov_accum_abort();
	}
#endif

    if (strncmp(line, "wdg", 3) == 0) {
        line += 3;
		while (isspace((unsigned char)*line)) {
			line++;
		}

		if (*line == '?') {
			print_status();
			return;
		}

		if (strncmp(line, "clear", 5) == 0) {
			clear_override();
			return;
		}

		errno = 0;
		char *end = NULL;
		unsigned long val = strtoul(line, &end, 10);
		if (errno || end == line) {
			LOG_EVT(WRN, "UART_CMD", "PARSE_FAIL", "arg=%s", line);
			return;
		}

		if (*end != '\0' && !isspace((unsigned char)*end)) {
			LOG_EVT(WRN, "UART_CMD", "GARBAGE_TRAILING", "suffix=%s", end);
			return;
		}

		apply_timeout((uint32_t)val);
		return;
	}

#if IS_ENABLED(CONFIG_APP_CRYPTO_BACKEND_CURVE25519)
	if (strncmp(line, "prov", 4) == 0) {
		prov_accum_reset();
		prov_accum_append(line);
		if (prov_command_complete(prov_accum)) {
			handle_provision_command(prov_accum + 4);
			prov_accum_reset();
		}
		return;
	}

	if (prov_accum_len > 0U) {
		prov_accum_append(line);
		if (prov_command_complete(prov_accum)) {
			handle_provision_command(prov_accum + 4);
			prov_accum_reset();
		}
		return;
	}
#endif

	LOG_EVT(WRN, "UART_CMD", "UNKNOWN", "cmd=%s", line);
}

static void command_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_thread_name_set(k_current_get(), "uart_cmd");

	char buffer[CMD_BUFFER_LEN];
	size_t len = 0U;

	LOG_EVT(INF, "UART_CMD", "READY",
		"fallback=%s", cmd_safe_mode_active ? "yes" : "no");

	while (1) {
		uint8_t ch;
		int rc = uart_poll_in(uart_dev, &ch);
		if (rc != 0) {
			k_sleep(K_MSEC(20));
			continue;
		}

		LOG_DBG("uart_cmd ch=0x%02x (%c)",
			ch, isprint(ch) ? ch : '.');
		if (ch == '\r' || ch == '\n') {
			if (len > 0U) {
				buffer[len] = '\0';
				handle_line(buffer);
				len = 0U;
			}
			continue;
		}

		if (len < sizeof(buffer) - 1U) {
			buffer[len++] = (char)ch;
		}
	}
}

void uart_commands_start(bool safe_mode_active)
{
	cmd_safe_mode_active = safe_mode_active;

	uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("Console UART not ready; disabling command handler");
		return;
	}

	LOG_INF("uart_cmd thread starting (safe_mode=%s)",
		cmd_safe_mode_active ? "yes" : "no");

	k_thread_create(&cmd_thread, cmd_stack, K_THREAD_STACK_SIZEOF(cmd_stack),
			command_thread, NULL, NULL, NULL, CMD_THREAD_PRIORITY, 0,
			K_NO_WAIT);
}

#else /* CONFIG_APP_ENABLE_UART_COMMANDS */

void uart_commands_start(bool safe_mode_active)
{
	ARG_UNUSED(safe_mode_active);
}

#endif
