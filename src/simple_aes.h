#ifndef SIMPLE_AES_H
#define SIMPLE_AES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIMPLE_AES_BLOCK_BYTES 16
#define SIMPLE_AES_MAX_KEY_BYTES 32
#define SIMPLE_AES_MAX_ROUNDS 14

struct simple_aes_ctx {
    uint8_t round_keys[SIMPLE_AES_BLOCK_BYTES * (SIMPLE_AES_MAX_ROUNDS + 1)];
    uint8_t rounds;
};

int simple_aes_setkey_enc(struct simple_aes_ctx *ctx, const uint8_t *key,
                          size_t key_len);
void simple_aes_encrypt_block(const struct simple_aes_ctx *ctx,
                              const uint8_t input[SIMPLE_AES_BLOCK_BYTES],
                              uint8_t output[SIMPLE_AES_BLOCK_BYTES]);

#ifdef __cplusplus
}
#endif

#endif
