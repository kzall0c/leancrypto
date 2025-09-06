/*
 * Copyright (C) 2023 - 2025, Stephan Mueller <smueller@chronox.de>
 *
 * License: see LICENSE file in root directory
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
/*
 * This code is derived in parts from the code distribution provided with
 * https://github.com/psanal2018/kyber-arm64
 *
 * That code is released under MIT license.
 */

#ifndef KYBER_POLY_ARMV8_H
#define KYBER_POLY_ARMV8_H

#include "kyber_type.h"
#include "kyber_cbd_armv8.h"
#include "kyber_kdf.h"
#include "kyber_ntt.h"
#include "kyber_ntt_armv8.h"
#include "kyber_reduce_armv8.h"
#include "null_buffer.h"
#include "ret_checkers.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Elements of R_q = Z_q[X]/(X^n + 1). Represents polynomial
 * coeffs[0] + X*coeffs[1] + X^2*coeffs[2] + ... + X^{n-1}*coeffs[n-1]
 */
typedef struct {
	int16_t coeffs[LC_KYBER_N];
} poly;

void kyber_add_armv8(int16_t *r, const int16_t *a, const int16_t *b);

/*
 * TODO: remove this code once poly_reduce has been fixed
 */
#include "kyber_reduce.h"
static inline void poly_reduce_c_bugfix(poly *r)
{
	unsigned int i;

	for (i = 0; i < LC_KYBER_N; i++)
		r->coeffs[i] = barrett_reduce(r->coeffs[i]);
}

/**
 * @brief poly_reduce - Applies Barrett reduction to all coefficients of a
 *			polynomial for details of the Barrett reduction see
 *			comments in kyber_reduce.c
 *
 * @param [in,out] r pointer to input/output polynomial
 */
static inline void poly_reduce(poly *r)
{
	kyber_barret_red_armv8(r->coeffs);
	kyber_barret_red_armv8(r->coeffs + 128);
}

/**
 * @brief poly_add - Add two polynomials; no modular reduction is performed
 *
 * @param [out] r pointer to output polynomial
 * @param [in] a pointer to first input polynomial
 * @param [in] b pointer to second input polynomial
 */
static inline void poly_add(poly *r, const poly *a, const poly *b)
{
	kyber_add_armv8(r->coeffs, a->coeffs, b->coeffs);
}

#include "common/kyber_poly_sub.h"

/**
 * @brief poly_sub_reduce - Combination of
 *				poly_sub(r, a, b);
 *				poly_reduce(r);
 *
 * @param [out] r pointer to output polynomial
 * @param [in] a pointer to first input polynomial
 * @param [in] b pointer to second input polynomial
 */
static inline void poly_sub_reduce(poly *r, const poly *a, const poly *b)
{
	kyber_sub_reduce_armv8(r->coeffs, a->coeffs, b->coeffs);
	kyber_sub_reduce_armv8(r->coeffs + 128, a->coeffs + 128,
			       b->coeffs + 128);
}

/**
 * @brief poly_add_reduce - Combination of
 *				poly_add(r, a, b);
 *				poly_reduce(r);
 *
 * @param [out] r pointer to output polynomial
 * @param [in] a pointer to first input polynomial
 * @param [in] b pointer to second input polynomial
 */
static inline void poly_add_reduce(poly *r, const poly *a, const poly *b)
{
	kyber_add_reduce_armv8(r->coeffs, a->coeffs, b->coeffs);
	kyber_add_reduce_armv8(r->coeffs + 128, a->coeffs + 128,
			       b->coeffs + 128);
}

/**
 * @brief poly_add_add_reduce - Combination of
 *				poly_add(r, a, b);
 *				poly_add(r, r, c);
 *				poly_reduce(r);
 *
 * @param [out] r pointer to output polynomial
 * @param [in] a pointer to first input polynomial
 * @param [in] b pointer to second input polynomial
 * @param [in] c pointer to third input polynomial
 */
static inline void poly_add_add_reduce(poly *r, const poly *a, const poly *b,
				       const poly *c)
{
	kyber_add_add_reduce_armv8(r->coeffs, a->coeffs, b->coeffs, c->coeffs);
	kyber_add_add_reduce_armv8(r->coeffs + 128, a->coeffs + 128,
				   b->coeffs + 128, c->coeffs + 128);
}

/**
 * @brief poly_compress_armv8 - Compression and subsequent serialization of a
 *			 	polynomial
 *
 * @param [out] r pointer to output byte array
 * @param [in] a pointer to input polynomial
 */
void poly_compress(uint8_t r[LC_KYBER_POLYCOMPRESSEDBYTES], const poly *a);

/**
 * @brief poly_decompress_armv8 - De-serialization and subsequent decompression
 *				  of a polynomial;
 *			    	  approximate inverse of poly_compress
 *
 * @param [out] r pointer to output polynomial
 * @param [in] a pointer to input byte array
 */
void poly_decompress(poly *r, const uint8_t a[LC_KYBER_POLYCOMPRESSEDBYTES]);

void kyber_poly_tobytes_armv8(uint8_t r[LC_KYBER_POLYBYTES], const poly *a);
void kyber_poly_frombytes_armv8(poly *r, const uint8_t a[LC_KYBER_POLYBYTES]);

/**
 * @brief poly_tobytes - Serialization of a polynomial
 *
 * @param [out] r pointer to output byte array
 * @param [in] a pointer to input polynomial
 */
static inline void poly_tobytes(uint8_t r[LC_KYBER_POLYBYTES], const poly *a)
{
	kyber_poly_tobytes_armv8(r, a);
}

/**
 * @brief poly_frombytes - De-serialization of a polynomial;
 *			   inverse of poly_tobytes
 *
 * @param [out] r pointer to output polynomial
 * @param [in] a pointer to input byte array
 */
static inline void poly_frombytes(poly *r, const uint8_t a[LC_KYBER_POLYBYTES])
{
	kyber_poly_frombytes_armv8(r, a);

	/* Reduce to ensure loaded data is within interval [0, q - 1] */
	poly_reduce(r);
}

#include "common/kyber_poly_frommsg.h"
#include "common/kyber_poly_tomsg.h"

#define POLY_GETNOISE_ETA1_BUFSIZE (LC_KYBER_ETA1 * LC_KYBER_N / 4)
static inline void
kyber_poly_cbd_eta1_armv8(poly *r,
			  const uint8_t buf[POLY_GETNOISE_ETA1_BUFSIZE])
{
#if LC_KYBER_ETA1 == 2
	kyber_cbd2_armv8(r->coeffs, buf);
#elif LC_KYBER_ETA1 == 3
	kyber_cbd3_armv8(r->coeffs, buf);
#else
#error "This implementation requires eta1 in {2,3}"
#endif
}

/**
 * @brief poly_getnoise_eta1 - Sample a polynomial deterministically from a seed
 *			       and a nonce, with output polynomial close to
 *			       centered binomial distribution with parameter
 *			       LC_KYBER_ETA1
 *
 * @param [out] r pointer to output polynomial
 * @param [in] seed pointer to input seed
 * @param [in] nonce one-byte input nonce
 */
static inline int
poly_getnoise_eta1_armv8(poly *r, const uint8_t seed[LC_KYBER_SYMBYTES],
			 uint8_t nonce, void *ws_buf)
{
	uint8_t *buf = ws_buf;
	int ret;

	CKINT(kyber_shake256_prf(buf, POLY_GETNOISE_ETA1_BUFSIZE, seed, nonce));
	kyber_poly_cbd_eta1_armv8(r, buf);

out:
	return ret;
}

static inline void
kyber_poly_cbd_eta2_armv8(poly *r,
			  const uint8_t buf[LC_KYBER_ETA2 * LC_KYBER_N / 4])
{
#if LC_KYBER_ETA2 == 2
	kyber_cbd2_armv8(r->coeffs, buf);
#else
#error "This implementation requires eta2 = 2"
#endif
}

/**
 * @brief poly_getnoise_eta2 - Sample a polynomial deterministically from a seed
 *			       and a nonce, with output polynomial close to
 *			       centered binomial distribution with parameter
 *			       LC_KYBER_ETA2
 *
 * @param [out] r pointer to output polynomial
 * @param [in] seed pointer to input seed
 * @param [in] nonce one-byte input nonce
 */
#define POLY_GETNOISE_ETA2_BUFSIZE (LC_KYBER_ETA2 * LC_KYBER_N / 4)
static inline int
poly_getnoise_eta2_armv8(poly *r, const uint8_t seed[LC_KYBER_SYMBYTES],
			 uint8_t nonce, void *ws_buf)
{
	uint8_t *buf = ws_buf;
	int ret;

	CKINT(kyber_shake256_prf(buf, POLY_GETNOISE_ETA2_BUFSIZE, seed, nonce));
	kyber_poly_cbd_eta2_armv8(r, buf);

out:
	return ret;
}

/**
 * @brief poly_ntt - Computes negacyclic number-theoretic transform (NTT) of
 *		     a polynomial in place; inputs assumed to be in normal
 *		     order, output in bitreversed order
 *
 * @param [in,out] r pointer to in/output polynomial
 */
static inline void poly_ntt(poly *r)
{
	kyber_ntt_armv8(r->coeffs, kyber_zetas_armv8);

	/*
	 * TODO: the poly_reduce() here somehow calculates a different
	 * result compared to the poly_reduce() from kyber_poly.h (i.e. the
	 * C implementation). This leads sometimes to a different SK (e.g.
	 * test vector 21 in tests/kyber_kem_tester_armv8.c shows that the
	 * 1087th and 1088th byte is different compared to the expected result).
	 * Yet, the calculated ciphertext or shared secret are identical to
	 * the expected values, even with the different SK. Strange.
	 *
	 * Yet, we cannot use the ARMv8 poly_reduce() until this has been
	 * corrected. Instead we simply use the C implementation from
	 * kyber_poly.h for the time being.
	 */
	//poly_reduce(r);
	poly_reduce_c_bugfix(r);
}

/**
 * @brief poly_invntt_tomont - Computes inverse of negacyclic number-theoretic
 *			       transform (NTT) of a polynomial in place;
 *			       inputs assumed to be in bitreversed order, output
 *			       in normal order
 *
 * @param [in,out] r pointer to in/output polynomial
 */
static inline void poly_invntt_tomont(poly *r)
{
	kyber_inv_ntt_armv8(r->coeffs, kyber_zetas_inv_armv8);
}

/**
 * @brief poly_basemul_montgomery - Multiplication of two polynomials in NTT
 *				    domain
 *
 * @param [out] r pointer to output polynomial
 * @param [in] a pointer to first input polynomial
 * @param [in] b pointer to second input polynomial
 */
static inline void poly_basemul_montgomery(poly *r, const poly *a,
					   const poly *b)
{
	kyber_basemul_armv8(r->coeffs, a->coeffs, b->coeffs, kyber_zetas);
}

/**
 * @brief poly_tomont - Inplace conversion of all coefficients of a polynomial
 *			from normal domain to Montgomery domain
 *
 * @param [in,out] r pointer to input/output polynomial
 */
static inline void poly_tomont(poly *r)
{
	kyber_tomont_armv8(r->coeffs);
	kyber_tomont_armv8(r->coeffs + 128);
}

#ifdef __cplusplus
}
#endif

#endif /* KYBER_POLY_ARMV8_H */
