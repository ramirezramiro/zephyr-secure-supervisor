#ifndef PERSIST_STATE_TEST_H
#define PERSIST_STATE_TEST_H

#include <zephyr/kernel.h>

#include "persist_state_priv.h"

#if defined(CONFIG_ZTEST)
void persist_state_test_init_blob(struct persist_blob *blob,
				  uint32_t consecutive_watchdog,
				  uint32_t total_watchdog,
				  uint32_t override_ms);

#if IS_ENABLED(CONFIG_APP_USE_AES_ENCRYPTION)
int persist_state_test_encrypt_blob(const struct persist_blob *blob,
				    struct persist_blob_encrypted *storage,
				    size_t *cipher_len);
int persist_state_test_decrypt_blob(const struct persist_blob_encrypted *storage,
				    struct persist_blob *blob);
#endif

void persist_state_test_copy_plain(struct persist_blob *dst,
				   const struct persist_blob *src);
#endif /* CONFIG_ZTEST */

#endif /* PERSIST_STATE_TEST_H */
