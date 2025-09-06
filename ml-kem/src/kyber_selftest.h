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

#ifndef KYBER_SELFTEST_H
#define KYBER_SELFTEST_H

#include "kyber_type.h"

#ifdef __cplusplus
extern "C" {
#endif

void kyber_kem_keygen_selftest(
	int (*_lc_kyber_keypair)(struct lc_kyber_pk *pk, struct lc_kyber_sk *sk,
				 struct lc_rng_ctx *rng_ctx));

void kyber_kem_enc_selftest(int (*_lc_kyber_enc)(struct lc_kyber_ct *ct,
						 struct lc_kyber_ss *ss,
						 const struct lc_kyber_pk *pk,
						 struct lc_rng_ctx *rng_ctx));
void kyber_kem_enc_kdf_selftest(int (*_lc_kyber_kdf_enc)(
	struct lc_kyber_ct *ct, uint8_t *ss, size_t ss_len,
	const struct lc_kyber_pk *pk, struct lc_rng_ctx *rng_ctx));

void kyber_kem_dec_selftest(int (*_lc_kyber_dec)(struct lc_kyber_ss *ss,
						 const struct lc_kyber_ct *ct,
						 const struct lc_kyber_sk *sk));
void kyber_kem_dec_kdf_selftest(int (*_lc_kyber_kdf_dec)(
	uint8_t *ss, size_t ss_len, const struct lc_kyber_ct *ct,
	const struct lc_kyber_sk *sk));

#ifdef __cplusplus
}
#endif

#endif /* KYBER_SELFTEST_H */
