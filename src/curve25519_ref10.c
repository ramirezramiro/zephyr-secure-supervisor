#include "curve25519_ref10.h"

/*
 * Derived from the public-domain curve25519-donna / TweetNaCl reference
 * implementation (D. J. Bernstein, Adam Langley, et al.). Only the small
 * subset required for X25519 scalar multiplication is kept here so the
 * firmware can run without an external crypto library.
 */

#include <string.h>

/* Field operations follow the same layout as the ref10 implementation. */
typedef int32_t fe[10];

static void fe_0(fe h)
{
	memset(h, 0, sizeof(fe));
}

static void fe_1(fe h)
{
	fe_0(h);
	h[0] = 1;
}

static void fe_copy(fe h, const fe f)
{
	for (int i = 0; i < 10; i++) {
		h[i] = f[i];
	}
}

static void fe_add(fe h, const fe f, const fe g)
{
	for (int i = 0; i < 10; i++) {
		h[i] = f[i] + g[i];
	}
}

static void fe_sub(fe h, const fe f, const fe g)
{
	for (int i = 0; i < 10; i++) {
		h[i] = f[i] - g[i];
	}
}

static void fe_mul(fe h, const fe f, const fe g);
static void fe_sq(fe h, const fe f)
{
	fe_mul(h, f, f);
}

static void fe_mul(fe h, const fe f, const fe g)
{
	int64_t f0 = f[0];
	int64_t f1 = f[1];
	int64_t f2 = f[2];
	int64_t f3 = f[3];
	int64_t f4 = f[4];
	int64_t f5 = f[5];
	int64_t f6 = f[6];
	int64_t f7 = f[7];
	int64_t f8 = f[8];
	int64_t f9 = f[9];

	int64_t g0 = g[0];
	int64_t g1 = g[1];
	int64_t g2 = g[2];
	int64_t g3 = g[3];
	int64_t g4 = g[4];
	int64_t g5 = g[5];
	int64_t g6 = g[6];
	int64_t g7 = g[7];
	int64_t g8 = g[8];
	int64_t g9 = g[9];

	int64_t g1_19 = 19 * g1;
	int64_t g2_19 = 19 * g2;
	int64_t g3_19 = 19 * g3;
	int64_t g4_19 = 19 * g4;
	int64_t g5_19 = 19 * g5;
	int64_t g6_19 = 19 * g6;
	int64_t g7_19 = 19 * g7;
	int64_t g8_19 = 19 * g8;
	int64_t g9_19 = 19 * g9;

	int64_t f1_2 = 2 * f1;
	int64_t f3_2 = 2 * f3;
	int64_t f5_2 = 2 * f5;
	int64_t f7_2 = 2 * f7;
	int64_t f9_2 = 2 * f9;

	int64_t h0 = f0 * g0 + f1_2 * g9_19 + f2 * g8_19 + f3_2 * g7_19 + f4 * g6_19 +
		     f5_2 * g5_19 + f6 * g4_19 + f7_2 * g3_19 + f8 * g2_19 + f9_2 * g1_19;
	int64_t h1 = f0 * g1 + f1 * g0 + f2 * g9_19 + f3 * g8_19 + f4 * g7_19 + f5 * g6_19 +
		     f6 * g5_19 + f7 * g4_19 + f8 * g3_19 + f9 * g2_19;
	int64_t h2 = f0 * g2 + f1_2 * g1 + f2 * g0 + f3_2 * g9_19 + f4 * g8_19 +
		     f5_2 * g7_19 + f6 * g6_19 + f7_2 * g5_19 + f8 * g4_19 + f9_2 * g3_19;
	int64_t h3 = f0 * g3 + f1 * g2 + f2 * g1 + f3 * g0 + f4 * g9_19 + f5 * g8_19 +
		     f6 * g7_19 + f7 * g6_19 + f8 * g5_19 + f9 * g4_19;
	int64_t h4 = f0 * g4 + f1_2 * g3 + f2 * g2 + f3_2 * g1 + f4 * g0 + f5_2 * g9_19 +
		     f6 * g8_19 + f7_2 * g7_19 + f8 * g6_19 + f9_2 * g5_19;
	int64_t h5 = f0 * g5 + f1 * g4 + f2 * g3 + f3 * g2 + f4 * g1 + f5 * g0 +
		     f6 * g9_19 + f7 * g8_19 + f8 * g7_19 + f9 * g6_19;
	int64_t h6 = f0 * g6 + f1_2 * g5 + f2 * g4 + f3_2 * g3 + f4 * g2 + f5_2 * g1 +
		     f6 * g0 + f7_2 * g9_19 + f8 * g8_19 + f9_2 * g7_19;
	int64_t h7 = f0 * g7 + f1 * g6 + f2 * g5 + f3 * g4 + f4 * g3 + f5 * g2 +
		     f6 * g1 + f7 * g0 + f8 * g9_19 + f9 * g8_19;
	int64_t h8 = f0 * g8 + f1_2 * g7 + f2 * g6 + f3_2 * g5 + f4 * g4 + f5_2 * g3 +
		     f6 * g2 + f7_2 * g1 + f8 * g0 + f9_2 * g9_19;
	int64_t h9 = f0 * g9 + f1 * g8 + f2 * g7 + f3 * g6 + f4 * g5 + f5 * g4 +
		     f6 * g3 + f7 * g2 + f8 * g1 + f9 * g0;

	int64_t carry0 = (h0 + (int64_t)(1 << 25)) >> 26;
	h1 += carry0;
	h0 -= carry0 << 26;
	int64_t carry4 = (h4 + (int64_t)(1 << 25)) >> 26;
	h5 += carry4;
	h4 -= carry4 << 26;
	int64_t carry1 = (h1 + (int64_t)(1 << 24)) >> 25;
	h2 += carry1;
	h1 -= carry1 << 25;
	int64_t carry5 = (h5 + (int64_t)(1 << 24)) >> 25;
	h6 += carry5;
	h5 -= carry5 << 25;
	int64_t carry2 = (h2 + (int64_t)(1 << 25)) >> 26;
	h3 += carry2;
	h2 -= carry2 << 26;
	int64_t carry6 = (h6 + (int64_t)(1 << 25)) >> 26;
	h7 += carry6;
	h6 -= carry6 << 26;
	int64_t carry3 = (h3 + (int64_t)(1 << 24)) >> 25;
	h4 += carry3;
	h3 -= carry3 << 25;
	int64_t carry7 = (h7 + (int64_t)(1 << 24)) >> 25;
	h8 += carry7;
	h7 -= carry7 << 25;
	carry4 = (h4 + (int64_t)(1 << 25)) >> 26;
	h5 += carry4;
	h4 -= carry4 << 26;
	int64_t carry8 = (h8 + (int64_t)(1 << 25)) >> 26;
	h9 += carry8;
	h8 -= carry8 << 26;
	int64_t carry9 = (h9 + (int64_t)(1 << 24)) >> 25;
	h0 += carry9 * 19;
	h9 -= carry9 << 25;
	carry0 = (h0 + (int64_t)(1 << 25)) >> 26;
	h1 += carry0;
	h0 -= carry0 << 26;

	h[0] = (int32_t)h0;
	h[1] = (int32_t)h1;
	h[2] = (int32_t)h2;
	h[3] = (int32_t)h3;
	h[4] = (int32_t)h4;
	h[5] = (int32_t)h5;
	h[6] = (int32_t)h6;
	h[7] = (int32_t)h7;
	h[8] = (int32_t)h8;
	h[9] = (int32_t)h9;
}

static void fe_mul121666(fe h, const fe f)
{
	for (int i = 0; i < 10; i++) {
		h[i] = f[i] * 121666;
	}
}

static void fe_invert(fe out, const fe z)
{
	fe t0;
	fe t1;
	fe t2;
	fe t3;

	fe_sq(t0, z);
	fe_sq(t1, t0);
	fe_sq(t1, t1);
	fe_mul(t1, z, t1);
	fe_mul(t0, t0, t1);
	fe_sq(t2, t0);
	fe_mul(t1, t1, t2);
	fe_sq(t2, t1);
	for (int i = 1; i < 5; i++) {
		fe_sq(t2, t2);
	}
	fe_mul(t1, t2, t1);
	fe_sq(t2, t1);
	for (int i = 1; i < 10; i++) {
		fe_sq(t2, t2);
	}
	fe_mul(t2, t2, t1);
	fe_sq(t3, t2);
	for (int i = 1; i < 20; i++) {
		fe_sq(t3, t3);
	}
	fe_mul(t2, t3, t2);
	fe_sq(t2, t2);
	for (int i = 1; i < 10; i++) {
		fe_sq(t2, t2);
	}
	fe_mul(t1, t2, t1);
	fe_sq(t2, t1);
	for (int i = 1; i < 50; i++) {
		fe_sq(t2, t2);
	}
	fe_mul(t2, t2, t1);
	fe_sq(t3, t2);
	for (int i = 1; i < 100; i++) {
		fe_sq(t3, t3);
	}
	fe_mul(t2, t3, t2);
	fe_sq(t2, t2);
	for (int i = 1; i < 50; i++) {
		fe_sq(t2, t2);
	}
	fe_mul(t1, t2, t1);
	fe_sq(t1, t1);
	for (int i = 1; i < 5; i++) {
		fe_sq(t1, t1);
	}
	fe_mul(out, t1, t0);
}

static void fe_tobytes(uint8_t s[32], const fe h)
{
	fe t;
	fe_copy(t, h);

	int32_t q = (19 * t[9] + (1 << 24)) >> 25;
	q = (t[0] + q) >> 26;
	q = (t[1] + q) >> 25;
	q = (t[2] + q) >> 26;
	q = (t[3] + q) >> 25;
	q = (t[4] + q) >> 26;
	q = (t[5] + q) >> 25;
	q = (t[6] + q) >> 26;
	q = (t[7] + q) >> 25;
	q = (t[8] + q) >> 26;
	t[9] += q << 25;

	int32_t carry0 = (t[0] + (1 << 25)) >> 26;
	t[1] += carry0;
	t[0] -= carry0 << 26;
	int32_t carry1 = (t[1] + (1 << 24)) >> 25;
	t[2] += carry1;
	t[1] -= carry1 << 25;
	int32_t carry2 = (t[2] + (1 << 25)) >> 26;
	t[3] += carry2;
	t[2] -= carry2 << 26;
	int32_t carry3 = (t[3] + (1 << 24)) >> 25;
	t[4] += carry3;
	t[3] -= carry3 << 25;
	int32_t carry4 = (t[4] + (1 << 25)) >> 26;
	t[5] += carry4;
	t[4] -= carry4 << 26;
	int32_t carry5 = (t[5] + (1 << 24)) >> 25;
	t[6] += carry5;
	t[5] -= carry5 << 25;
	int32_t carry6 = (t[6] + (1 << 25)) >> 26;
	t[7] += carry6;
	t[6] -= carry6 << 26;
	int32_t carry7 = (t[7] + (1 << 24)) >> 25;
	t[8] += carry7;
	t[7] -= carry7 << 25;
	int32_t carry8 = (t[8] + (1 << 25)) >> 26;
	t[9] += carry8;
	t[8] -= carry8 << 26;
	int32_t carry9 = (t[9] + (1 << 24)) >> 25;
	t[0] += carry9 * 19;
	t[9] -= carry9 << 25;

	carry0 = (t[0] + (1 << 25)) >> 26;
	t[1] += carry0;
	t[0] -= carry0 << 26;
	carry1 = (t[1] + (1 << 24)) >> 25;
	t[2] += carry1;
	t[1] -= carry1 << 25;

	s[0] = t[0] >> 0;
	s[1] = t[0] >> 8;
	s[2] = t[0] >> 16;
	s[3] = (t[0] >> 24) | (t[1] << 2);
	s[4] = t[1] >> 6;
	s[5] = t[1] >> 14;
	s[6] = (t[1] >> 22) | (t[2] << 3);
	s[7] = t[2] >> 5;
	s[8] = t[2] >> 13;
	s[9] = (t[2] >> 21) | (t[3] << 5);
	s[10] = t[3] >> 3;
	s[11] = t[3] >> 11;
	s[12] = (t[3] >> 19) | (t[4] << 6);
	s[13] = t[4] >> 2;
	s[14] = t[4] >> 10;
	s[15] = t[4] >> 18;
	s[16] = t[5] >> 0;
	s[17] = t[5] >> 8;
	s[18] = t[5] >> 16;
	s[19] = (t[5] >> 24) | (t[6] << 1);
	s[20] = t[6] >> 7;
	s[21] = t[6] >> 15;
	s[22] = (t[6] >> 23) | (t[7] << 3);
	s[23] = t[7] >> 5;
	s[24] = t[7] >> 13;
	s[25] = (t[7] >> 21) | (t[8] << 4);
	s[26] = t[8] >> 4;
	s[27] = t[8] >> 12;
	s[28] = (t[8] >> 20) | (t[9] << 6);
	s[29] = t[9] >> 2;
	s[30] = t[9] >> 10;
	s[31] = t[9] >> 18;
}

static void fe_frombytes(fe h, const uint8_t s[32])
{
	int64_t t0 = s[0] | ((int64_t)s[1] << 8) | ((int64_t)s[2] << 16) | ((int64_t)s[3] << 24);
	int64_t t1 = s[4] | ((int64_t)s[5] << 8) | ((int64_t)s[6] << 16) | ((int64_t)s[7] << 24);
	int64_t t2 = s[8] | ((int64_t)s[9] << 8) | ((int64_t)s[10] << 16) | ((int64_t)s[11] << 24);
	int64_t t3 = s[12] | ((int64_t)s[13] << 8) | ((int64_t)s[14] << 16) | ((int64_t)s[15] << 24);
	int64_t t4 = s[16] | ((int64_t)s[17] << 8) | ((int64_t)s[18] << 16) | ((int64_t)s[19] << 24);
	int64_t t5 = s[20] | ((int64_t)s[21] << 8) | ((int64_t)s[22] << 16) | ((int64_t)s[23] << 24);
	int64_t t6 = s[24] | ((int64_t)s[25] << 8) | ((int64_t)s[26] << 16) | ((int64_t)s[27] << 24);
	int64_t t7 = s[28] | ((int64_t)s[29] << 8) | ((int64_t)s[30] << 16) | ((int64_t)s[31] << 24);

	h[0] = (int32_t)(t0 & 0x3ffffff);
	h[1] = (int32_t)((t0 >> 26) | ((t1 & 0x1fff) << 6));
	h[2] = (int32_t)((t1 >> 19) | ((t2 & 0x3ff) << 13));
	h[3] = (int32_t)((t2 >> 10) | ((t3 & 0x7f) << 22));
	h[4] = (int32_t)((t3 >> 7) & 0x3ffffff);
	h[5] = (int32_t)((t3 >> 33) | ((t4 & 0x1fff) << 6));
	h[6] = (int32_t)((t4 >> 20) | ((t5 & 0x3ff) << 13));
	h[7] = (int32_t)((t5 >> 11) | ((t6 & 0x7f) << 22));
	h[8] = (int32_t)((t6 >> 8) & 0x3ffffff);
	h[9] = (int32_t)((t6 >> 34) | ((t7 & 0x1fff) << 6));
}

static void montgomery_ladder(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32])
{
	uint8_t e[32];
	memcpy(e, scalar, 32);
	curve25519_ref10_clamp_scalar(e);

	fe x1;
	fe_frombytes(x1, point);
	fe x2;
	fe z2;
	fe x3;
	fe z3;
	fe tmp0;
	fe tmp1;

	fe_1(x2);
	fe_0(z2);
	fe_copy(x3, x1);
	fe_1(z3);

	int swap = 0;
	for (int pos = 254; pos >= 0; pos--) {
		int bit = (e[pos >> 3] >> (pos & 7)) & 1;
		swap ^= bit;
		if (swap) {
			for (int i = 0; i < 10; i++) {
				int32_t t = x2[i];
				x2[i] = x3[i];
				x3[i] = t;
				t = z2[i];
				z2[i] = z3[i];
				z3[i] = t;
			}
		}
		swap = bit;

		fe_add(tmp0, x2, z2);
		fe_sub(tmp1, x2, z2);
		fe_add(z2, x3, z3);
		fe_sub(x3, x3, z3);
		fe_mul(z3, tmp0, x3);
		fe_mul(x3, tmp1, z2);
		fe_sq(tmp0, tmp0);
		fe_sq(tmp1, tmp1);
		fe_add(z2, z3, x3);
		fe_sub(x3, z3, x3);
		fe_sq(z3, x3);
		fe_sub(x2, tmp0, tmp1);
		fe_mul121666(z2, x2);
		fe_add(z2, z2, tmp0);
		fe_mul(x2, tmp0, tmp1);
		fe_mul(z2, z2, x2);
		fe_sq(x3, tmp1);
		fe_mul(z3, z3, x1);
	}

	for (int i = 0; i < 10; i++) {
		int32_t t = x2[i];
		x2[i] = x3[i];
		x3[i] = t;
		t = z2[i];
		z2[i] = z3[i];
		z3[i] = t;
	}

	fe_invert(z2, z2);
	fe_mul(x2, x2, z2);
	fe_tobytes(out, x2);
}

void curve25519_ref10_clamp_scalar(uint8_t scalar[CURVE25519_KEY_SIZE])
{
	scalar[0] &= 248;
	scalar[31] &= 127;
	scalar[31] |= 64;
}

void curve25519_ref10_scalarmult_base(uint8_t out[CURVE25519_KEY_SIZE],
				      const uint8_t scalar[CURVE25519_KEY_SIZE])
{
	static const uint8_t basepoint[32] = {9};
	montgomery_ladder(out, scalar, basepoint);
}

int curve25519_ref10_scalarmult(uint8_t out[CURVE25519_KEY_SIZE],
				const uint8_t scalar[CURVE25519_KEY_SIZE],
				const uint8_t point[CURVE25519_KEY_SIZE])
{
	uint8_t tmp[32];
	memcpy(tmp, point, 32);
	montgomery_ladder(out, scalar, tmp);
	return 0;
}
