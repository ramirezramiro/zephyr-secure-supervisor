#ifndef APP_CRYPTO_H
#define APP_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_CRYPTO_CTR_LEN_BITS 32U
#define APP_CRYPTO_AES_BLOCK_BYTES 16U
#define APP_CRYPTO_IV_LEN (APP_CRYPTO_AES_BLOCK_BYTES - (APP_CRYPTO_CTR_LEN_BITS / 8U))

enum app_crypto_backend_type {
	APP_CRYPTO_BACKEND_TYPE_NONE = 0,
	APP_CRYPTO_BACKEND_TYPE_AES,
	APP_CRYPTO_BACKEND_TYPE_CURVE25519,
};

int app_crypto_init(void);
bool app_crypto_is_enabled(void);
enum app_crypto_backend_type app_crypto_get_backend(void);
uint32_t app_crypto_get_session_counter(void);
uint32_t app_crypto_get_session_salt(void);

int app_crypto_encrypt_buffer(const uint8_t *input, size_t input_len,
			      uint8_t *cipher_out, size_t cipher_capacity,
			      size_t *cipher_len, uint8_t iv_out[APP_CRYPTO_IV_LEN]);

int app_crypto_decrypt_buffer(const uint8_t *cipher, size_t cipher_len,
			      const uint8_t iv[APP_CRYPTO_IV_LEN],
			      uint8_t *plain_out, size_t plain_capacity,
			      size_t *plain_len);

int app_crypto_bytes_to_hex(const uint8_t *src, size_t src_len,
			    char *dst, size_t dst_len);

uint32_t app_crypto_compute_sample_mac(const uint8_t iv[APP_CRYPTO_IV_LEN],
				       const uint8_t *cipher, size_t cipher_len);

#endif /* APP_CRYPTO_H */
