#ifndef APP_CRYPTO_H
#define APP_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_CRYPTO_CTR_LEN_BITS 32U
#define APP_CRYPTO_AES_BLOCK_BYTES 16U
#define APP_CRYPTO_IV_LEN (APP_CRYPTO_AES_BLOCK_BYTES - (APP_CRYPTO_CTR_LEN_BITS / 8U))

int app_crypto_init(void);
bool app_crypto_is_enabled(void);

int app_crypto_encrypt_buffer(const uint8_t *input, size_t input_len,
			      uint8_t *cipher_out, size_t cipher_capacity,
			      size_t *cipher_len, uint8_t iv_out[APP_CRYPTO_IV_LEN]);

int app_crypto_decrypt_buffer(const uint8_t *cipher, size_t cipher_len,
			      const uint8_t iv[APP_CRYPTO_IV_LEN],
			      uint8_t *plain_out, size_t plain_capacity,
			      size_t *plain_len);

int app_crypto_bytes_to_hex(const uint8_t *src, size_t src_len,
			    char *dst, size_t dst_len);

#endif /* APP_CRYPTO_H */
