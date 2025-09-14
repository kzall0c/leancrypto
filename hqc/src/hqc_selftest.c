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

#include "hqc_type.h"

#include "compare.h"
#include "hqc_selftest.h"
#include "lc_sha3.h"
#include "ret_checkers.h"
#include "small_stack_support.h"
#include "timecop.h"
#include "selftest_shake256_rng.h"

#if (LC_HQC_TYPE == 128)
#include "../tests/hqc_tester_vectors_128.h"
#elif (LC_HQC_TYPE == 192)
#include "../tests/hqc_tester_vectors_192.h"
#elif (LC_HQC_TYPE == 256)
#include "../tests/hqc_tester_vectors_256.h"
#else
#error "Bad level, choose one of 128/192/256"
#endif

static int _hqc_kem_keygen_selftest(int (*_lc_hqc_keypair)(struct lc_hqc_pk *pk,
						    struct lc_hqc_sk *sk,
						    struct lc_rng_ctx *rng_ctx))
{
	struct workspace {
		struct lc_hqc_pk pk;
		struct lc_hqc_sk sk;
	};
	int ret;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));
	LC_SELFTEST_SHAKE256_DRNG_CTX_ON_STACK(selftest_rng);

	lc_rng_seed(selftest_rng, hqc_test[0].seed, sizeof(hqc_test[0].seed),
		    NULL, 0);

	_lc_hqc_keypair(&ws->pk, &ws->sk, selftest_rng);
	if (lc_compare_selftest(LC_ALG_STATUS_HQC_KEYGEN, ws->pk.pk,
				hqc_test[0].pk, LC_HQC_PUBLIC_KEY_BYTES,
				"HQC PK"))
		goto out2;

out:
	/* Timecop: Selftest does not contain secrets */
	unpoison(&ws->sk.sk, LC_HQC_SECRET_KEY_BYTES);
	lc_compare_selftest(LC_ALG_STATUS_HQC_KEYGEN, ws->sk.sk, hqc_test[0].sk,
			    LC_HQC_SECRET_KEY_BYTES, "HQC SK");

out2:
	LC_RELEASE_MEM(ws);
	lc_rng_zero(selftest_rng);
	return ret;
}

void hqc_kem_keygen_selftest(int (*_lc_hqc_keypair)(struct lc_hqc_pk *pk,
						    struct lc_hqc_sk *sk,
						    struct lc_rng_ctx *rng_ctx))
{
	LC_SELFTEST_RUN(LC_ALG_STATUS_HQC_KEYGEN);
	_hqc_kem_keygen_selftest(_lc_hqc_keypair);
}

static int _hqc_kem_enc_selftest(int (*_lc_hqc_enc)(struct lc_hqc_ct *ct,
					     struct lc_hqc_ss *ss,
					     const struct lc_hqc_pk *pk,
					     struct lc_rng_ctx *rng_ctx))
{
	struct workspace {
		struct lc_hqc_ct ct;
		struct lc_hqc_ss key_b;
	};
	uint8_t discard[LC_HQC_SEED_BYTES + LC_HQC_VEC_K_SIZE_BYTES +
			LC_HQC_SEED_BYTES];
	int ret;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));
	LC_SELFTEST_SHAKE256_DRNG_CTX_ON_STACK(selftest_rng);

	lc_rng_seed(selftest_rng, hqc_test[0].seed, sizeof(hqc_test[0].seed),
		    NULL, 0);
	/* The test vector RNG state served a keygen before enc */
	lc_rng_generate(selftest_rng, NULL, 0, discard, sizeof(discard));

	// Encapsulation
	_lc_hqc_enc(&ws->ct, &ws->key_b, (struct lc_hqc_pk *)&hqc_test[0].pk,
		    selftest_rng);

	/* Timecop: Selftest does not contain secrets */
	unpoison(&ws->ct.ct, LC_HQC_CIPHERTEXT_BYTES);
	if (lc_compare_selftest(LC_ALG_STATUS_HQC_ENC, ws->ct.ct, hqc_test[0].ct, LC_HQC_CIPHERTEXT_BYTES,
			    "HQC CT"))
		goto out2;

out:
	/* Timecop: Selftest does not contain secrets */
	unpoison(&ws->key_b.ss, LC_HQC_SHARED_SECRET_BYTES);
	lc_compare_selftest(LC_ALG_STATUS_HQC_ENC, ws->key_b.ss, hqc_test[0].ss,
			    LC_HQC_SHARED_SECRET_BYTES, "HQC SS");

out2:
	LC_RELEASE_MEM(ws);
	lc_rng_zero(selftest_rng);
	return 0;
}

void hqc_kem_enc_selftest(int (*_lc_hqc_enc)(struct lc_hqc_ct *ct,
					     struct lc_hqc_ss *ss,
					     const struct lc_hqc_pk *pk,
					     struct lc_rng_ctx *rng_ctx))
{
	LC_SELFTEST_RUN(LC_ALG_STATUS_HQC_ENC);
	_hqc_kem_enc_selftest(_lc_hqc_enc);
}

void hqc_kem_dec_selftest(int (*_lc_hqc_dec)(struct lc_hqc_ss *ss,
					     const struct lc_hqc_ct *ct,
					     const struct lc_hqc_sk *sk))
{
	struct lc_hqc_ss key_a;

	LC_SELFTEST_RUN(LC_ALG_STATUS_HQC_DEC);

	// Decapsulation
	_lc_hqc_dec(&key_a, (struct lc_hqc_ct *)&hqc_test[0].ct,
		    (struct lc_hqc_sk *)&hqc_test[0].sk);

	/* Timecop: Selftest does not contain secrets */
	unpoison(key_a.ss, LC_HQC_SHARED_SECRET_BYTES);
	lc_compare_selftest(LC_ALG_STATUS_HQC_DEC, key_a.ss, hqc_test[0].ss,
			    LC_HQC_SHARED_SECRET_BYTES, "HQC SS");
}
