#ifndef PERSIST_STATE_PRIV_H
#define PERSIST_STATE_PRIV_H

#include <stdint.h>

#include <zephyr/sys/util.h>

#include "app_crypto.h"

struct persist_blob {
	uint32_t magic;
	uint32_t consecutive_watchdog;
	uint32_t total_watchdog;
	uint32_t watchdog_override_ms;
	uint32_t session_counter;
};

#if IS_ENABLED(CONFIG_APP_USE_AES_ENCRYPTION)
struct persist_blob_encrypted {
	uint8_t iv[APP_CRYPTO_IV_LEN];
	uint8_t data[sizeof(struct persist_blob)];
};
#endif

#endif /* PERSIST_STATE_PRIV_H */
