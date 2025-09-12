/*
 * Copyright (C) 2022 - 2025, Stephan Mueller <smueller@chronox.de>
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

#ifndef KYBER_KDF_H
#define KYBER_KDF_H

#include "kyber_type.h"
#include "lc_hash.h"
#include "lc_kmac.h"
#include "lc_sha3.h"
#include "ret_checkers.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kyber KDF
 *
 * @param [in] in input buffer
 * @param [in] inlen length of input buffer
 * @param [in] in2 input buffer 2
 * @param [in] inlen2 length of input buffer 2
 * @param [out] out output buffer of size
 * @param [in] outlen output buffer length
 */
static inline int kyber_kdf2(const uint8_t *in, size_t inlen,
			     const uint8_t *in2, size_t inlen2, uint8_t *out,
			     size_t outlen)
{
	LC_HASH_CTX_ON_STACK(shake256, lc_shake256);
	int ret;

	CKINT(lc_hash_init(shake256));
	lc_hash_update(shake256, in, inlen);
	lc_hash_update(shake256, in2, inlen2);
	lc_hash_set_digestsize(shake256, outlen);
	lc_hash_final(shake256, out);

	lc_hash_zero(shake256);

out:
	return ret;
}

/**
 * @brief Kyber KDF
 *
 * SS <- KMAC256(K = Kyber-SS from ephemeral key 1,
 *		 X = Kyber-SS from static key ||
 * 		     Nonce,
 *		 L = requested SS length,
 *		 S = "Kyber KEM 3-way SS")
 *
 * @param [in] in input buffer
 * @param [in] inlen length of input buffer
 * @param [in] in2 input buffer 2
 * @param [in] inlen2 length of input buffer 2
 * @param [in] in3 input buffer 3
 * @param [in] inlen3 length of input buffer 3
 * @param [out] out output buffer of size
 * @param [in] outlen output buffer length
 */
static inline int kyber_kdf3(const uint8_t *in, size_t inlen,
			     const uint8_t *in2, size_t inlen2,
			     const uint8_t *in3, size_t inlen3, uint8_t *out,
			     size_t outlen)
{
	LC_FIPS_RODATA_SECTION
	static const uint8_t kyber_ss_label[] = "Kyber KEM 3-way SS";
	LC_KMAC_CTX_ON_STACK(kmac_ctx, lc_cshake256);
	int ret;

	CKINT(lc_kmac_init(kmac_ctx, in, inlen, kyber_ss_label,
			   sizeof(kyber_ss_label) - 1));
	lc_kmac_update(kmac_ctx, in2, inlen2);
	lc_kmac_update(kmac_ctx, in3, inlen3);
	lc_kmac_final(kmac_ctx, out, outlen);

	lc_kmac_zero(kmac_ctx);

out:
	return ret;
}

/**
 * @brief Kyber KDF
 *
 * SS <- KMAC256(K = Kyber-SS from ephemeral key 1,
 *		 X = Kyber-SS from ephemeral key 2 ||
 *		     Kyber-SS from static key ||
 * 		     Nonce,
 *		 L = requested SS length,
 *		 S = "Kyber KEM 4-way SS")
 *
 * @param [in] in input buffer
 * @param [in] inlen length of input buffer
 * @param [in] in2 input buffer 2
 * @param [in] inlen2 length of input buffer 2
 * @param [in] in3 input buffer 3
 * @param [in] inlen3 length of input buffer 3
 * @param [in] in4 input buffer 4
 * @param [in] inlen4 length of input buffer 4
 * @param [out] out output buffer of size
 * @param [in] outlen output buffer length
 */
static inline int kyber_kdf4(const uint8_t *in, size_t inlen,
			     const uint8_t *in2, size_t inlen2,
			     const uint8_t *in3, size_t inlen3,
			     const uint8_t *in4, size_t inlen4, uint8_t *out,
			     size_t outlen)
{
	LC_FIPS_RODATA_SECTION
	static const uint8_t kyber_ss_label[] = "Kyber KEM 4-way SS";
	LC_KMAC_CTX_ON_STACK(kmac_ctx, lc_cshake256);
	int ret;

	CKINT(lc_kmac_init(kmac_ctx, in, inlen, kyber_ss_label,
			   sizeof(kyber_ss_label) - 1));
	lc_kmac_update(kmac_ctx, in2, inlen2);
	lc_kmac_update(kmac_ctx, in3, inlen3);
	lc_kmac_update(kmac_ctx, in4, inlen4);
	lc_kmac_final(kmac_ctx, out, outlen);

	lc_kmac_zero(kmac_ctx);

out:
	return ret;
}

/**
 * @brief kyber_shake256_prf - Usage of SHAKE256 as a PRF, concatenates secret
 *			       and public input and then generates outlen bytes
 *			       of SHAKE256 output
 *
 * @param [out] out pointer to output
 * @param [in] outlen number of requested output bytes
 * @param [in] key pointer to the key
 * @param [in] nonce single-byte nonce (public PRF input)
 */
static inline int kyber_shake256_prf(uint8_t *out, size_t outlen,
				     const uint8_t key[LC_KYBER_SYMBYTES],
				     uint8_t nonce)
{
	return kyber_kdf2(key, LC_KYBER_SYMBYTES, &nonce, 1, out, outlen);
}

/**
 * @brief kyber_shake256_rkprf - Usage of SHAKE256 as a PRF, concatenates secret
 *			   	 and public input and then generates outlen
 *				 bytes of SHAKE256 output
 *
 * @param [out] out pointer to output
 * @param [in] key pointer to the key
 * @param [in] nonce single-byte nonce (public PRF input)
 */
static inline int
kyber_shake256_rkprf(uint8_t out[LC_KYBER_SSBYTES],
		     const uint8_t key[LC_KYBER_SYMBYTES],
		     const uint8_t nonce[LC_KYBER_CIPHERTEXTBYTES])
{
	return kyber_kdf2(key, LC_KYBER_SYMBYTES, nonce,
			  LC_KYBER_CIPHERTEXTBYTES, out, LC_KYBER_SSBYTES);
}

/**
 * @brief kyber_ss_kdf - KDF to derive arbitrary sized SS from Kyber SS
 *
 *	SS <- KMAC256(K = Kyber-SS, X = Kyber-CT, L = requested SS length,
 *		      S = "Kyber KEM SS")
 *
 * This KDF is is consistent with SP800-108 rev 1.
 */
static inline int kyber_ss_kdf(uint8_t *ss, size_t ss_len,
			       const struct lc_kyber_ct *ct,
			       const uint8_t kyber_ss[LC_KYBER_SSBYTES])
{
	LC_FIPS_RODATA_SECTION
	static const uint8_t kyber_ss_label[] = "Kyber KEM SS";

	return lc_kmac(lc_cshake256, kyber_ss, LC_KYBER_SSBYTES, kyber_ss_label,
		       sizeof(kyber_ss_label) - 1, ct->ct,
		       LC_KYBER_CIPHERTEXTBYTES, ss, ss_len);
}

#ifdef __cplusplus
}
#endif

#endif /* KYBER_KDF_H */
