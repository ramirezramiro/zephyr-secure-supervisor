#include "persist_state.h"
#include "log_utils.h"
#include "app_crypto.h"
#include "safe_memory.h"
#include "persist_state_priv.h"
#if defined(CONFIG_ZTEST)
#include "persist_state_test.h"
#endif

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

LOG_MODULE_REGISTER(persist_state, LOG_LEVEL_INF);

#define PERSIST_MAGIC 0x4C454453u /* 'LEDS' */
#define PERSIST_RECORD_ID 1
#define PERSIST_CURVE_SECRET_ID 2
#define PERSIST_CURVE_SECRET_MAGIC 0x43555256u /* 'CURV' */
#define PERSIST_CURVE_PEER_ID 3
#define PERSIST_CURVE_PEER_MAGIC 0x43555250u /* 'CURP' */

#define STORAGE_PARTITION_NODE DT_NODELABEL(storage_partition)
#define PERSIST_RETRY_LIMIT 3
#define PERSIST_RETRY_DELAY_MS 10

struct persist_curve_secret {
	uint32_t magic;
	uint8_t secret[CURVE25519_KEY_SIZE];
};

struct persist_curve_peer {
	uint32_t magic;
	uint8_t peer[CURVE25519_KEY_SIZE];
};

static struct {
	struct nvs_fs fs;
	struct persist_blob blob;
	bool loaded;
} g_state;

static K_MUTEX_DEFINE(state_lock);

#if IS_ENABLED(CONFIG_APP_CRYPTO_BACKEND_CURVE25519)
static int hex_nibble(char c)
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

static int parse_hex_secret(const char *hex, uint8_t *out, size_t len)
{
	if (hex == NULL || hex[0] == '\0') {
		return -ENOENT;
	}

	size_t exp_len = len * 2U;
	size_t actual = strlen(hex);
	if (actual != exp_len) {
		return -EINVAL;
	}

	for (size_t i = 0U; i < len; i++) {
		int hi = hex_nibble(hex[i * 2U]);
		int lo = hex_nibble(hex[i * 2U + 1U]);
		if (hi < 0 || lo < 0) {
			return -EINVAL;
		}
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	return 0;
}

static void derive_scalar_from_device_id(uint8_t *buf)
{
	uint32_t seed = 0x6d5a56a1U;
	ssize_t len = -ENOSYS;

#if IS_ENABLED(CONFIG_HWINFO)
	uint8_t dev_id[16];

	len = hwinfo_get_device_id(dev_id, sizeof(dev_id));
	if (len > 0) {
		for (ssize_t i = 0; i < len; i++) {
			seed ^= ((uint32_t)dev_id[i] << ((i & 0x3) * 8));
			seed = seed * 1664525U + 1013904223U;
		}
	} else {
		len = -ENODATA;
	}
#endif

	if (len <= 0) {
		seed ^= 0xC3A5C85CU;
	}

	for (size_t i = 0U; i < CURVE25519_KEY_SIZE; i += sizeof(uint32_t)) {
		seed = seed * 1664525U + 1013904223U;
		size_t copy = MIN(sizeof(uint32_t), CURVE25519_KEY_SIZE - i);
		memcpy(&buf[i], &seed, copy);
	}
}

static int curve_secret_generate(uint8_t secret[CURVE25519_KEY_SIZE])
{
	int rc = parse_hex_secret(CONFIG_APP_CURVE25519_STATIC_SECRET_HEX,
				  secret, CURVE25519_KEY_SIZE);
	if (rc == 0) {
		LOG_INF("Curve25519 scalar seeded from CONFIG_APP_CURVE25519_STATIC_SECRET_HEX");
		curve25519_ref10_clamp_scalar(secret);
		return 0;
	}

	derive_scalar_from_device_id(secret);
	curve25519_ref10_clamp_scalar(secret);
	LOG_INF("Curve25519 scalar derived from hardware device ID");
	return 0;
}
#endif
#if IS_ENABLED(CONFIG_APP_USE_AES_ENCRYPTION)
static int persist_read_encrypted(struct persist_blob *out_blob)
{
	struct persist_blob_encrypted storage = {0};
	int rc = nvs_read(&g_state.fs, PERSIST_RECORD_ID, &storage, sizeof(storage));

	if (rc == sizeof(storage)) {
		size_t plain_len = 0U;
		rc = app_crypto_decrypt_buffer(storage.data, sizeof(storage.data),
					       storage.iv, (uint8_t *)out_blob,
					       sizeof(*out_blob), &plain_len);
		if (rc != 0) {
			LOG_ERR("Persist blob decrypt failed: %d", rc);
			return rc;
		}
		if (plain_len != sizeof(*out_blob)) {
			LOG_ERR("Unexpected persist plain length: %zu", plain_len);
			return -EIO;
		}
		return 0;
	}

	if (rc == sizeof(struct persist_blob)) {
		safe_memcpy(out_blob, sizeof(*out_blob), &storage, sizeof(*out_blob));
		return 0;
	}

	return rc;
}
#endif

static int persist_load_blob(struct persist_blob *out_blob)
{
	int rc;

#if IS_ENABLED(CONFIG_APP_USE_AES_ENCRYPTION)
	if (app_crypto_is_enabled()) {
		rc = persist_read_encrypted(out_blob);
		if (rc == 0) {
			return 0;
		}
		if (rc < 0 && rc != -ENOENT) {
			return rc;
		}
	}
#endif

	rc = nvs_read(&g_state.fs, PERSIST_RECORD_ID, out_blob, sizeof(*out_blob));
	if (rc < 0) {
		return rc;
	}

	return (rc == sizeof(*out_blob)) ? 0 : -ENOENT;
}

static int persist_store_blob(const struct persist_blob *blob)
{
#if IS_ENABLED(CONFIG_APP_USE_AES_ENCRYPTION)
	if (app_crypto_is_enabled()) {
		struct persist_blob_encrypted storage;
		size_t cipher_len = 0U;
		int rc = app_crypto_encrypt_buffer((const uint8_t *)blob, sizeof(*blob),
						   storage.data, sizeof(storage.data),
						   &cipher_len, storage.iv);
		if (rc != 0) {
			LOG_ERR("Persist blob encryption failed: %d", rc);
			return rc;
		}
		if (cipher_len != sizeof(storage.data)) {
			LOG_ERR("Unexpected cipher length %zu", cipher_len);
			return -EIO;
		}
		return nvs_write(&g_state.fs, PERSIST_RECORD_ID,
				 &storage, sizeof(storage));
	}
#endif

	return nvs_write(&g_state.fs, PERSIST_RECORD_ID,
			 blob, sizeof(*blob));
}

static int persist_commit_locked(void)
{
	int rc = persist_store_blob(&g_state.blob);

	if (rc < 0) {
		LOG_ERR("Persistent write failed: %d", rc);
		LOG_EVT(ERR, "PERSIST", "WRITE_FAIL", "rc=%d", rc);
		return rc;
	}

	return 0;
}

static int init_fs_if_needed(void)
{
	if (g_state.loaded) {
		return 0;
	}

	const struct flash_area *fa = NULL;
	int rc = 0;

	for (int attempt = 1; attempt <= PERSIST_RETRY_LIMIT; ++attempt) {
		rc = flash_area_open(DT_FIXED_PARTITION_ID(STORAGE_PARTITION_NODE), &fa);
		if (rc == 0) {
			if (attempt > 1) {
				LOG_EVT(INF, "PERSIST", "FLASH_OPEN_RECOVERED", "attempt=%d", attempt);
			}
			break;
		}

		LOG_WRN("Failed to open storage partition (attempt %d/%d): %d",
			attempt, PERSIST_RETRY_LIMIT, rc);
		LOG_EVT(WRN, "PERSIST", "FLASH_OPEN_RETRY", "attempt=%d,rc=%d", attempt, rc);

		if (attempt < PERSIST_RETRY_LIMIT) {
			k_msleep(PERSIST_RETRY_DELAY_MS);
		}
	}

	if (rc != 0) {
		LOG_EVT(ERR, "PERSIST", "FLASH_OPEN_FAIL", "rc=%d", rc);
		return rc;
	}

	g_state.fs.flash_device = fa->fa_dev;
	if (!device_is_ready(g_state.fs.flash_device)) {
		LOG_ERR("Storage flash device not ready");
		LOG_EVT(ERR, "PERSIST", "FLASH_NOT_READY", "dev_id=%u",
			DT_FIXED_PARTITION_ID(STORAGE_PARTITION_NODE));
		flash_area_close(fa);
		return -EBUSY;
	}

	g_state.fs.offset = fa->fa_off;

	struct flash_pages_info page_info;
	rc = flash_get_page_info_by_offs(g_state.fs.flash_device,
					 g_state.fs.offset, &page_info);
	if (rc != 0) {
		LOG_ERR("flash_get_page_info_by_offs failed: %d", rc);
		LOG_EVT(ERR, "PERSIST", "FLASH_PAGE_INFO_FAIL", "rc=%d", rc);
		flash_area_close(fa);
		return rc;
	}

	g_state.fs.sector_size = page_info.size;
	g_state.fs.sector_count = fa->fa_size / page_info.size;

	flash_area_close(fa);

	for (int attempt = 1; attempt <= PERSIST_RETRY_LIMIT; ++attempt) {
		rc = nvs_mount(&g_state.fs);
		if (rc == 0) {
			if (attempt > 1) {
				LOG_EVT(INF, "PERSIST", "NVS_MOUNT_RECOVERED", "attempt=%d", attempt);
			}
			break;
		}

		LOG_WRN("Failed to mount NVS (attempt %d/%d): %d",
			attempt, PERSIST_RETRY_LIMIT, rc);
		LOG_EVT(WRN, "PERSIST", "NVS_MOUNT_RETRY", "attempt=%d,rc=%d", attempt, rc);

		if (attempt < PERSIST_RETRY_LIMIT) {
			k_msleep(PERSIST_RETRY_DELAY_MS);
		}
	}

	if (rc != 0) {
		LOG_EVT(ERR, "PERSIST", "NVS_MOUNT_FAIL", "rc=%d", rc);
		return rc;
	}

	struct persist_blob tmp;
	rc = persist_load_blob(&tmp);
	if (rc == 0 && tmp.magic == PERSIST_MAGIC) {
		g_state.blob = tmp;
	} else {
		g_state.blob.magic = PERSIST_MAGIC;
		g_state.blob.consecutive_watchdog = 0U;
		g_state.blob.total_watchdog = 0U;
		g_state.blob.watchdog_override_ms = 0U;
		g_state.blob.session_counter = 0U;
		(void)persist_commit_locked();
	}

	g_state.loaded = true;
	LOG_INF("Persistent state loaded: consecutive=%u total=%u override=%u",
		g_state.blob.consecutive_watchdog,
		g_state.blob.total_watchdog,
		g_state.blob.watchdog_override_ms);
	return 0;
}

int persist_state_init(void)
{
	int rc;

	k_mutex_lock(&state_lock, K_FOREVER);
	rc = init_fs_if_needed();
	k_mutex_unlock(&state_lock);

	return rc;
}

void persist_state_record_boot(bool watchdog_reset)
{
	k_mutex_lock(&state_lock, K_FOREVER);

	if (init_fs_if_needed() != 0) {
		k_mutex_unlock(&state_lock);
		return;
	}

	if (watchdog_reset) {
		g_state.blob.consecutive_watchdog++;
		g_state.blob.total_watchdog++;
	} else if (g_state.blob.consecutive_watchdog != 0U) {
		g_state.blob.consecutive_watchdog = 0U;
	}

	(void)persist_commit_locked();
	k_mutex_unlock(&state_lock);
}

void persist_state_clear_watchdog_counter(void)
{
	k_mutex_lock(&state_lock, K_FOREVER);

	if (init_fs_if_needed() == 0 &&
	    g_state.blob.consecutive_watchdog != 0U) {
		g_state.blob.consecutive_watchdog = 0U;
		(void)persist_commit_locked();
	}

	k_mutex_unlock(&state_lock);
}

uint32_t persist_state_get_consecutive_watchdog(void)
{
	k_mutex_lock(&state_lock, K_FOREVER);
	uint32_t value = g_state.blob.consecutive_watchdog;
	k_mutex_unlock(&state_lock);
	return value;
}

uint32_t persist_state_get_total_watchdog(void)
{
	k_mutex_lock(&state_lock, K_FOREVER);
	uint32_t value = g_state.blob.total_watchdog;
	k_mutex_unlock(&state_lock);
	return value;
}

bool persist_state_is_fallback_active(void)
{
	return persist_state_get_consecutive_watchdog() >=
	       (uint32_t)CONFIG_APP_RESET_WATCHDOG_THRESHOLD;
}

uint32_t persist_state_get_watchdog_override(void)
{
	k_mutex_lock(&state_lock, K_FOREVER);
	uint32_t value = g_state.blob.watchdog_override_ms;
	k_mutex_unlock(&state_lock);
	return value;
}

int persist_state_set_watchdog_override(uint32_t timeout_ms)
{
	int rc = 0;

	k_mutex_lock(&state_lock, K_FOREVER);

	if (init_fs_if_needed() != 0) {
		k_mutex_unlock(&state_lock);
		return -EIO;
	}

	if (g_state.blob.watchdog_override_ms != timeout_ms) {
		g_state.blob.watchdog_override_ms = timeout_ms;
		rc = persist_commit_locked();
	}

	k_mutex_unlock(&state_lock);
	return rc;
}

uint32_t persist_state_next_session_counter(void)
{
	uint32_t value = 0U;

	k_mutex_lock(&state_lock, K_FOREVER);

	if (init_fs_if_needed() == 0) {
		g_state.blob.session_counter++;
		value = g_state.blob.session_counter;
		(void)persist_commit_locked();
	}

	k_mutex_unlock(&state_lock);
	return value;
}

#if IS_ENABLED(CONFIG_APP_CRYPTO_BACKEND_CURVE25519)
int persist_state_curve25519_get_secret(uint8_t out[CURVE25519_KEY_SIZE])
{
	if (out == NULL) {
		return -EINVAL;
	}

	int rc = init_fs_if_needed();
	if (rc != 0) {
		return rc;
	}

	k_mutex_lock(&state_lock, K_FOREVER);

	struct persist_curve_secret record = {0};
	rc = nvs_read(&g_state.fs, PERSIST_CURVE_SECRET_ID, &record, sizeof(record));
	if (rc == sizeof(record) && record.magic == PERSIST_CURVE_SECRET_MAGIC) {
		memcpy(out, record.secret, CURVE25519_KEY_SIZE);
		k_mutex_unlock(&state_lock);
		return 0;
	}

	rc = curve_secret_generate(record.secret);
	if (rc != 0) {
		k_mutex_unlock(&state_lock);
		return rc;
	}
	record.magic = PERSIST_CURVE_SECRET_MAGIC;

	rc = nvs_write(&g_state.fs, PERSIST_CURVE_SECRET_ID, &record, sizeof(record));
	if (rc < 0) {
		LOG_ERR("Failed to persist Curve25519 scalar: %d", rc);
		k_mutex_unlock(&state_lock);
		return rc;
	}

	memcpy(out, record.secret, CURVE25519_KEY_SIZE);
	k_mutex_unlock(&state_lock);
	return 0;
}

int persist_state_curve25519_set_secret(const uint8_t secret[CURVE25519_KEY_SIZE])
{
	if (secret == NULL) {
		return -EINVAL;
	}

	struct persist_curve_secret record = {
		.magic = PERSIST_CURVE_SECRET_MAGIC
	};

	memcpy(record.secret, secret, CURVE25519_KEY_SIZE);
	curve25519_ref10_clamp_scalar(record.secret);

	k_mutex_lock(&state_lock, K_FOREVER);

	int rc = init_fs_if_needed();
	if (rc == 0) {
		rc = nvs_write(&g_state.fs, PERSIST_CURVE_SECRET_ID,
			       &record, sizeof(record));
		if (rc < 0) {
			LOG_ERR("Failed to write Curve25519 scalar: %d", rc);
		} else {
			rc = 0;
			LOG_INF("Curve25519 scalar updated via provisioning command");
		}
	}

	k_mutex_unlock(&state_lock);
	return rc;
}

int persist_state_curve25519_get_peer(uint8_t out[CURVE25519_KEY_SIZE])
{
	if (out == NULL) {
		return -EINVAL;
	}

	int rc = init_fs_if_needed();
	if (rc != 0) {
		return rc;
	}

	k_mutex_lock(&state_lock, K_FOREVER);

	struct persist_curve_peer record = {0};
	rc = nvs_read(&g_state.fs, PERSIST_CURVE_PEER_ID, &record, sizeof(record));
	if (rc == sizeof(record) && record.magic == PERSIST_CURVE_PEER_MAGIC) {
		memcpy(out, record.peer, CURVE25519_KEY_SIZE);
		k_mutex_unlock(&state_lock);
		return 0;
	}

	k_mutex_unlock(&state_lock);
	return -ENOENT;
}

int persist_state_curve25519_set_peer(const uint8_t peer[CURVE25519_KEY_SIZE])
{
	if (peer == NULL) {
		return -EINVAL;
	}

	struct persist_curve_peer record = {
		.magic = PERSIST_CURVE_PEER_MAGIC
	};
	memcpy(record.peer, peer, CURVE25519_KEY_SIZE);

	k_mutex_lock(&state_lock, K_FOREVER);

	int rc = init_fs_if_needed();
	if (rc == 0) {
		rc = nvs_write(&g_state.fs, PERSIST_CURVE_PEER_ID,
			       &record, sizeof(record));
		if (rc < 0) {
			LOG_ERR("Failed to write Curve25519 peer key: %d", rc);
		} else {
			rc = 0;
			LOG_INF("Curve25519 peer public key updated via provisioning command");
		}
	}

	k_mutex_unlock(&state_lock);
	return rc;
}
#else
int persist_state_curve25519_get_secret(uint8_t out[CURVE25519_KEY_SIZE])
{
	ARG_UNUSED(out);
	return -ENOTSUP;
}

int persist_state_curve25519_set_secret(const uint8_t secret[CURVE25519_KEY_SIZE])
{
	ARG_UNUSED(secret);
	return -ENOTSUP;
}

int persist_state_curve25519_get_peer(uint8_t out[CURVE25519_KEY_SIZE])
{
	ARG_UNUSED(out);
	return -ENOTSUP;
}

int persist_state_curve25519_set_peer(const uint8_t peer[CURVE25519_KEY_SIZE])
{
	ARG_UNUSED(peer);
	return -ENOTSUP;
}
#endif

#if defined(CONFIG_ZTEST)
void persist_state_test_init_blob(struct persist_blob *blob,
				  uint32_t consecutive_watchdog,
				  uint32_t total_watchdog,
				  uint32_t override_ms)
{
	safe_memset(blob, sizeof(*blob), 0, sizeof(*blob));
	blob->magic = PERSIST_MAGIC;
	blob->consecutive_watchdog = consecutive_watchdog;
	blob->total_watchdog = total_watchdog;
	blob->watchdog_override_ms = override_ms;
}

#if IS_ENABLED(CONFIG_APP_USE_AES_ENCRYPTION)
int persist_state_test_encrypt_blob(const struct persist_blob *blob,
				    struct persist_blob_encrypted *storage,
				    size_t *cipher_len)
{
	return app_crypto_encrypt_buffer((const uint8_t *)blob, sizeof(*blob),
					 storage->data, sizeof(storage->data),
					 cipher_len, storage->iv);
}

int persist_state_test_decrypt_blob(const struct persist_blob_encrypted *storage,
				    struct persist_blob *blob)
{
	size_t plain_len = 0U;
	int rc = app_crypto_decrypt_buffer(storage->data, sizeof(storage->data),
					   storage->iv, (uint8_t *)blob,
					   sizeof(*blob), &plain_len);
	if (rc != 0) {
		return rc;
	}

	return (plain_len == sizeof(*blob)) ? 0 : -EIO;
}
#endif

void persist_state_test_copy_plain(struct persist_blob *dst,
				   const struct persist_blob *src)
{
	safe_memcpy(dst, sizeof(*dst), src, sizeof(*src));
}

void persist_state_test_reset(void)
{
	k_mutex_lock(&state_lock, K_FOREVER);

	if (init_fs_if_needed() == 0) {
		(void)nvs_clear(&g_state.fs);
	}

	safe_memset(&g_state.fs, sizeof(g_state.fs), 0, sizeof(g_state.fs));
	safe_memset(&g_state.blob, sizeof(g_state.blob), 0, sizeof(g_state.blob));
	g_state.loaded = false;
	k_mutex_unlock(&state_lock);
}

void persist_state_test_reload(void)
{
	k_mutex_lock(&state_lock, K_FOREVER);
	safe_memset(&g_state.fs, sizeof(g_state.fs), 0, sizeof(g_state.fs));
	safe_memset(&g_state.blob, sizeof(g_state.blob), 0, sizeof(g_state.blob));
	g_state.loaded = false;
	k_mutex_unlock(&state_lock);
}
#endif
