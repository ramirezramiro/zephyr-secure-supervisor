#include "app_crypto.h"

#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include "simple_aes.h"
#include "safe_memory.h"

LOG_MODULE_REGISTER(app_crypto, LOG_LEVEL_INF);

#define APP_CRYPTO_MAX_KEY_BYTES 32U
#define APP_CRYPTO_MAX_HEX_CHARS (APP_CRYPTO_MAX_KEY_BYTES * 2U)

static uint8_t key_buf[APP_CRYPTO_MAX_KEY_BYTES];
static size_t key_len;
static uint8_t iv_seed[APP_CRYPTO_IV_LEN];
static bool crypto_ready;
static atomic_t iv_counter = ATOMIC_INIT(0);
static atomic_t prng_state = ATOMIC_INIT(0x6d5a56a1);
static struct simple_aes_ctx aes_ctx;

static int hex_value(char c)
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

static int parse_hex_string(const char *hex, uint8_t *out, size_t max_len)
{
	size_t len = safe_strlen(hex, APP_CRYPTO_MAX_HEX_CHARS + 1U);

	if ((len % 2U) != 0U) {
		return -EINVAL;
	}

	size_t bytes = len / 2U;

	if (bytes == 0U || bytes > max_len) {
		return -EOVERFLOW;
	}

	for (size_t i = 0U; i < bytes; i++) {
		int hi = hex_value(hex[i * 2U]);
		int lo = hex_value(hex[i * 2U + 1U]);

		if (hi < 0 || lo < 0) {
			return -EINVAL;
		}

		out[i] = (uint8_t)((hi << 4) | lo);
	}

	return (int)bytes;
}

static uint32_t next_pseudo_entropy(void)
{
	uint32_t current;
	uint32_t next;

	do {
		current = (uint32_t)atomic_get(&prng_state);
		next = current * 1664525U + 1013904223U;
	} while (atomic_cas(&prng_state, current, next) == false);

	/* xorshift mix */
	next ^= next << 13;
	next ^= next >> 17;
	next ^= next << 5;

	return next;
}

static void generate_iv(uint8_t *iv_out)
{
	for (size_t offset = 0U; offset < APP_CRYPTO_IV_LEN; offset += sizeof(uint32_t)) {
		uint32_t rnd = next_pseudo_entropy();
		size_t chunk = sizeof(uint32_t);

		if (chunk > (APP_CRYPTO_IV_LEN - offset)) {
			chunk = APP_CRYPTO_IV_LEN - offset;
		}

			safe_memcpy(&iv_out[offset], APP_CRYPTO_IV_LEN - offset, &rnd, chunk);
	}

	for (size_t i = 0U; i < APP_CRYPTO_IV_LEN; i++) {
		iv_out[i] ^= iv_seed[i];
	}

	uint32_t ctr = (uint32_t)atomic_inc(&iv_counter);
	for (size_t i = 0U; i < sizeof(ctr); i++) {
		size_t idx = APP_CRYPTO_IV_LEN - 1U - i;
		iv_out[idx] ^= (uint8_t)((ctr >> (i * 8U)) & 0xFF);
	}
}

static void increment_counter(uint8_t *counter)
{
	for (int i = APP_CRYPTO_AES_BLOCK_BYTES - 1; i >= (int)APP_CRYPTO_IV_LEN; i--) {
		counter[i]++;
		if (counter[i] != 0U) {
			break;
		}
	}
}

static void ctr_process(const uint8_t *input, uint8_t *output, size_t len,
			const uint8_t iv[APP_CRYPTO_IV_LEN])
{
	uint8_t counter[APP_CRYPTO_AES_BLOCK_BYTES] = {0};
	uint8_t stream[APP_CRYPTO_AES_BLOCK_BYTES];

	safe_memcpy(counter, sizeof(counter), iv, APP_CRYPTO_IV_LEN);

	while (len > 0U) {
		simple_aes_encrypt_block(&aes_ctx, counter, stream);

		size_t chunk = MIN(len, (size_t)APP_CRYPTO_AES_BLOCK_BYTES);
		for (size_t i = 0U; i < chunk; i++) {
			output[i] = input[i] ^ stream[i];
		}

		input += chunk;
		output += chunk;
		len -= chunk;
		increment_counter(counter);
	}
}

bool app_crypto_is_enabled(void)
{
	return IS_ENABLED(CONFIG_APP_USE_AES_ENCRYPTION) && crypto_ready;
}

int app_crypto_init(void)
{
	if (!IS_ENABLED(CONFIG_APP_USE_AES_ENCRYPTION)) {
		crypto_ready = false;
		return 0;
	}

	int rc = parse_hex_string(CONFIG_APP_AES_STATIC_KEY_HEX,
				      key_buf, sizeof(key_buf));
	if (rc < 0) {
		LOG_ERR("Invalid AES key hex string: %d", rc);
		return rc;
	}

	key_len = (size_t)rc;
	if (!(key_len == 16U || key_len == 24U || key_len == 32U)) {
		LOG_ERR("Unsupported AES key length: %zu", key_len);
		return -EINVAL;
	}

	rc = parse_hex_string(CONFIG_APP_AES_STATIC_IV_HEX,
			      iv_seed, sizeof(iv_seed));
	if (rc < 0 || (size_t)rc != APP_CRYPTO_IV_LEN) {
		LOG_ERR("Invalid IV seed (expected %u bytes)", APP_CRYPTO_IV_LEN);
		return rc < 0 ? rc : -EINVAL;
	}

	rc = simple_aes_setkey_enc(&aes_ctx, key_buf, key_len);
	if (rc != 0) {
		LOG_ERR("AES key setup failed");
		return -EINVAL;
	}

	crypto_ready = true;
	LOG_INF("AES helper initialized (key_len=%zu)", key_len);
	return 0;
}

int app_crypto_encrypt_buffer(const uint8_t *input, size_t input_len,
			      uint8_t *cipher_out, size_t cipher_capacity,
			      size_t *cipher_len, uint8_t iv_out[APP_CRYPTO_IV_LEN])
{
	if (!app_crypto_is_enabled()) {
		return -EACCES;
	}

	if (input_len == 0U) {
		return -EINVAL;
	}

	if (cipher_capacity < input_len) {
		return -ENOSPC;
	}

	uint8_t iv_tmp[APP_CRYPTO_IV_LEN];

	generate_iv(iv_tmp);

	ctr_process(input, cipher_out, input_len, iv_tmp);
	safe_memcpy(iv_out, APP_CRYPTO_IV_LEN, iv_tmp, APP_CRYPTO_IV_LEN);
	if (cipher_len != NULL) {
		*cipher_len = input_len;
	}

	return 0;
}

int app_crypto_decrypt_buffer(const uint8_t *cipher, size_t cipher_len,
			      const uint8_t iv[APP_CRYPTO_IV_LEN],
			      uint8_t *plain_out, size_t plain_capacity,
			      size_t *plain_len)
{
	if (!app_crypto_is_enabled()) {
		return -EACCES;
	}

	if (cipher_len == 0U) {
		return -EINVAL;
	}

	if (plain_capacity < cipher_len) {
		return -ENOSPC;
	}

	ctr_process(cipher, plain_out, cipher_len, iv);

	if (plain_len != NULL) {
		*plain_len = cipher_len;
	}

	return 0;
}

int app_crypto_bytes_to_hex(const uint8_t *src, size_t src_len,
			    char *dst, size_t dst_len)
{
	if (dst_len < (src_len * 2U + 1U)) {
		return -ENOSPC;
	}

	static const char hex_chars[] = "0123456789ABCDEF";

	for (size_t i = 0U; i < src_len; i++) {
		dst[i * 2U] = hex_chars[(src[i] >> 4) & 0xF];
		dst[i * 2U + 1U] = hex_chars[src[i] & 0xF];
	}

	dst[src_len * 2U] = '\0';
	return 0;
}
