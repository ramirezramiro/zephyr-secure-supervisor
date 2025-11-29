#ifndef CURVE25519_REF10_H
#define CURVE25519_REF10_H

#include <stdint.h>

#define CURVE25519_KEY_SIZE 32

void curve25519_ref10_clamp_scalar(uint8_t scalar[CURVE25519_KEY_SIZE]);
void curve25519_ref10_scalarmult_base(uint8_t out[CURVE25519_KEY_SIZE],
				      const uint8_t scalar[CURVE25519_KEY_SIZE]);
int curve25519_ref10_scalarmult(uint8_t out[CURVE25519_KEY_SIZE],
				const uint8_t scalar[CURVE25519_KEY_SIZE],
				const uint8_t point[CURVE25519_KEY_SIZE]);

#endif /* CURVE25519_REF10_H */
