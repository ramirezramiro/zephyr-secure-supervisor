#include <string.h>

#include <zephyr/ztest.h>

#include "app_crypto.h"
#include "persist_state_priv.h"
#include "persist_state_test.h"

static void *persist_state_suite_setup(void)
{
	int rc = app_crypto_init();
	zassert_ok(rc, "AES helper init failed (%d)", rc);
	return NULL;
}

ZTEST(persist_state_suite, test_encrypt_decrypt_round_trip)
{
	struct persist_blob original;
	struct persist_blob decoded;
	struct persist_blob_encrypted storage = {0};
	size_t cipher_len = 0U;

	persist_state_test_init_blob(&original, 3U, 12U, 2500U);

	int rc = persist_state_test_encrypt_blob(&original, &storage, &cipher_len);
	zassert_ok(rc, "encrypt failed (%d)", rc);
	zassert_equal(cipher_len, sizeof(storage.data), "cipher length mismatch");

	rc = persist_state_test_decrypt_blob(&storage, &decoded);
	zassert_ok(rc, "decrypt failed (%d)", rc);
	zassert_equal(decoded.consecutive_watchdog, original.consecutive_watchdog, NULL);
	zassert_equal(decoded.total_watchdog, original.total_watchdog, NULL);
	zassert_equal(decoded.watchdog_override_ms, original.watchdog_override_ms, NULL);
}

ZTEST(persist_state_suite, test_plain_copy_helper)
{
	struct persist_blob baseline;
	struct persist_blob copied;

	persist_state_test_init_blob(&baseline, 9U, 42U, 0U);
	persist_state_test_copy_plain(&copied, &baseline);

	zassert_equal(memcmp(&copied, &baseline, sizeof(baseline)), 0, "blob copy mismatch");
}

ZTEST_SUITE(persist_state_suite, NULL, persist_state_suite_setup, NULL, NULL, NULL);
