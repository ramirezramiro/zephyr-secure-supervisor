#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

#include "app_crypto.h"
#include "safe_memory.h"
#include "persist_state_priv.h"
#include "persist_state_test.h"
#include "supervisor_test.h"
#include "recovery_test.h"
#include "recovery.h"

static void *misra_stage1_setup(void)
{
	int rc = app_crypto_init();
	zassert_equal(rc, 0, "app_crypto_init failed (%d)", rc);
#if defined(CONFIG_ZTEST)
	recovery_test_init_event();
#endif
	return NULL;
}

ZTEST_SUITE(misra_stage1, NULL, misra_stage1_setup, NULL, NULL, NULL);

ZTEST(misra_stage1, test_safe_memory_helpers)
{
	uint8_t src[8] = {0, 1, 2, 3, 4, 5, 6, 7};
	uint8_t dst[8] = {0};

	safe_memcpy(dst, sizeof(dst), src, sizeof(src));
	zassert_mem_equal(dst, src, sizeof(src), "memcpy mismatch");

	safe_memset(dst, sizeof(dst), 0xAA, sizeof(dst));
	for (size_t i = 0U; i < ARRAY_SIZE(dst); i++) {
		zassert_equal(dst[i], 0xAA, "memset mismatch at %zu", i);
	}

	const char *msg = "hello";
	size_t len = safe_strlen(msg, 16U);
	zassert_equal(len, 5U, "unexpected strlen result");
}

ZTEST(misra_stage1, test_persist_state_encrypt_decrypt)
{
	struct persist_blob blob;
	struct persist_blob decoded;
	struct persist_blob_encrypted storage;
	size_t cipher_len = 0U;

	persist_state_test_init_blob(&blob, 1U, 10U, 2500U);

	int rc = persist_state_test_encrypt_blob(&blob, &storage, &cipher_len);
	zassert_equal(rc, 0, "encrypt blob failed (%d)", rc);
	zassert_equal(cipher_len, sizeof(blob), "cipher len mismatch");

	rc = persist_state_test_decrypt_blob(&storage, &decoded);
	zassert_equal(rc, 0, "decrypt blob failed (%d)", rc);
	zassert_mem_equal(&decoded, &blob, sizeof(blob), "blob mismatch");
}

ZTEST(misra_stage1, test_persist_state_plain_copy)
{
	struct persist_blob original;
	struct persist_blob copy;

	persist_state_test_init_blob(&original, 2U, 5U, 0U);
	persist_state_test_copy_plain(&copy, &original);

	zassert_mem_equal(&copy, &original, sizeof(original), "plain copy mismatch");
}

ZTEST(misra_stage1, test_supervisor_health_snapshots)
{
	const uint32_t now = 1000U;

	supervisor_test_set_last_seen(now - 100U, now - 200U);
	struct supervisor_health_snapshot healthy =
		supervisor_test_sample(true, now);

	zassert_true(healthy.led_ok, "led should be ok");
	zassert_true(healthy.hb_ok, "hb should be ok");

	supervisor_test_set_last_seen(now - (CONFIG_APP_HEALTH_LED_STALE_MS + 10U),
				      now - (CONFIG_APP_HEALTH_SYS_STALE_MS + 10U));
	struct supervisor_health_snapshot stale =
		supervisor_test_sample(true, now);

	zassert_false(stale.led_ok, "led should be stale");
	zassert_false(stale.hb_ok, "hb should be stale");
}

ZTEST(misra_stage1, test_recovery_event_recording)
{
	recovery_test_clear_pending_events();

	recovery_request(RECOVERY_REASON_MANUAL_TRIGGER);
	uint32_t events = recovery_test_get_pending_events();
	zassert_not_equal(events & BIT(RECOVERY_REASON_MANUAL_TRIGGER), 0U,
			  "manual trigger bit missing");

	recovery_test_clear_pending_events();
	recovery_request(RECOVERY_REASON_HEALTH_FAULT);
	events = recovery_test_get_pending_events();
	zassert_not_equal(events & BIT(RECOVERY_REASON_HEALTH_FAULT), 0U,
			  "health fault bit missing");
}
