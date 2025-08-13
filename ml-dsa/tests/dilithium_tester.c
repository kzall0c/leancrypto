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
/*
 * This code is derived in parts from the code distribution provided with
 * https://github.com/pq-crystals/dilithium
 *
 * That code is released under Public Domain
 * (https://creativecommons.org/share-your-work/public-domain/cc0/);
 * or Apache 2.0 License (https://www.apache.org/licenses/LICENSE-2.0.html).
 */

//#include "dilithium_pack.h"
//#include "dilithium_poly.h"
//#include "dilithium_polyvec.h"
#include "build_bug_on.h"
#include "dilithium_tester.h"
#include "ext_headers_internal.h"
#include "lc_hash.h"
#include "lc_sha3.h"
#include "ret_checkers.h"
#include "selftest_rng.h"
#include "small_stack_support.h"
#include "visibility.h"

#define MLEN 64
#define NVECTORS 50

/*
 * Enable to generate vectors. When enabling this, invoke the application
 * which outputs the header file of dilithium_tester_vectors.h. This
 * vector file can be included and will be applied when this option
 * not defined any more.
 *
 * The generated data could be cross-compared with test/test_vectors<level>
 * from https://github.com/pq-crystals/dilithium when printing out the
 * full keys/signatures instead of the SHAKE'd versions. NOTE: at the time of
 * writing, the CRYSTALS code does not yet have the change that enlarges their
 * ctilde to lambda * 2 (from 32 bytes). This means that the signature created
 * here is larger by (lambda * 2 - 32) starting at the 32th byte in the
 * signature. All other bits are identical.
 */
#undef GENERATE_VECTORS
#undef SHOW_SHAKEd_KEY

#ifndef GENERATE_VECTORS
#if LC_DILITHIUM_MODE == 2
#include "dilithium_tester_vectors_44.h"
#include "dilithium_internal_vectors_44.h"
#include "dilithium_prehashed_vectors_44.h"
#include "dilithium_externalmu_vectors_44.h"
#elif LC_DILITHIUM_MODE == 3
#include "dilithium_tester_vectors_65.h"
#include "dilithium_internal_vectors_65.h"
#include "dilithium_prehashed_vectors_65.h"
#include "dilithium_externalmu_vectors_65.h"
#elif LC_DILITHIUM_MODE == 5
#include "dilithium_tester_vectors_87.h"
#include "dilithium_internal_vectors_87.h"
#include "dilithium_prehashed_vectors_87.h"
#include "dilithium_externalmu_vectors_87.h"
#endif
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

int _dilithium_tester(
	unsigned int rounds, int verify_calculation, unsigned int internal_test,
	unsigned int prehashed, unsigned int external_mu,
	int (*_lc_dilithium_keypair)(struct lc_dilithium_pk *pk,
				     struct lc_dilithium_sk *sk,
				     struct lc_rng_ctx *rng_ctx),
	int (*_lc_dilithium_keypair_from_seed)(struct lc_dilithium_pk *pk,
					       struct lc_dilithium_sk *sk,
					       const uint8_t *seed,
					       size_t seedlen),
	int (*_lc_dilithium_sign)(struct lc_dilithium_sig *sig,
				  struct lc_dilithium_ctx *ctx,
				  const uint8_t *m, size_t mlen,
				  const struct lc_dilithium_sk *sk,
				  struct lc_rng_ctx *rng_ctx),
	int (*_lc_dilithium_verify)(const struct lc_dilithium_sig *sig,
				    struct lc_dilithium_ctx *ctx,
				    const uint8_t *m, size_t mlen,
				    const struct lc_dilithium_pk *pk))
{
	struct workspace {
		struct lc_dilithium_pk pk, pk2;
		struct lc_dilithium_sk sk, sk2;
		struct lc_dilithium_sig sig;
		struct lc_dilithium_sig sig_tmp;
		uint8_t m[MLEN];
		uint8_t seed[LC_DILITHIUM_CRHBYTES];
		uint8_t buf[LC_DILITHIUM_SECRETKEYBYTES];
		//uint8_t poly_uniform_gamma1_buf[POLY_UNIFORM_GAMMA1_BYTES];
		//uint8_t poly_uniform_eta_buf[POLY_UNIFORM_ETA_BYTES];
		//uint8_t poly_challenge_buf[POLY_CHALLENGE_BYTES];
		//poly c, tmp;
		//polyvecl s, y, mat[LC_DILITHIUM_K];
		//polyveck w, w1, w0, t1, t0, h;
	};
	unsigned int i, j, k, l, nvectors;
#ifndef GENERATE_VECTORS
	const struct dilithium_testvector *vec_p =
		internal_test ? dilithium_internal_testvectors :
		prehashed     ? dilithium_prehashed_testvectors :
		external_mu   ? dilithium_externalmu_testvectors :
				dilithium_testvectors;
#endif
	int ret = 0;
#if (defined(SHOW_SHAKEd_KEY) && !defined(GENERATE_VECTORS))
	uint8_t buf[32];
#endif

	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));
	LC_DILITHIUM_CTX_ON_STACK(ctx);
	LC_SELFTEST_DRNG_CTX_ON_STACK(selftest_rng);

	(void)j;
	(void)k;
	(void)l;

	ctx->ml_dsa_internal = !!internal_test;

	if (prehashed)
		ctx->dilithium_prehash_type = lc_shake256;

#ifdef GENERATE_VECTORS
	if (internal_test) {
		printf("#ifndef DILITHIUM_INTERNAL_TESTVECTORS_H\n"
		       "#define DILITHIUM_INTERNAL_TESTVECTORS_H\n"
		       "#include \"dilithium_type.h\"\n"
		       "static const struct dilithium_testvector dilithium_internal_testvectors[] =\n"
		       "{\n");
	} else if (prehashed) {
		printf("#ifndef DILITHIUM_PREHASHED_TESTVECTORS_H\n"
		       "#define DILITHIUM_PREHASHED_TESTVECTORS_H\n"
		       "#include \"dilithium_type.h\"\n"
		       "static const struct dilithium_testvector dilithium_prehashed_testvectors[] =\n"
		       "{\n");
	} else if (external_mu) {
		printf("#ifndef DILITHIUM_EXTERNALMU_TESTVECTORS_H\n"
		       "#define DILITHIUM_EXTERNALMU_TESTVECTORS_H\n"
		       "#include \"dilithium_type.h\"\n"
		       "static const struct dilithium_testvector dilithium_externalmu_testvectors[] =\n"
		       "{\n");
	} else {
		printf("#ifndef DILITHIUM_TESTVECTORS_H\n"
		       "#define DILITHIUM_TESTVECTORS_H\n"
		       "#include \"dilithium_type.h\"\n"
		       "struct dilithium_testvector {\n"
		       "\tuint8_t m[64];\n"
		       "\tuint8_t pk[LC_DILITHIUM_PUBLICKEYBYTES];\n"
		       "\tuint8_t sk[LC_DILITHIUM_SECRETKEYBYTES];\n"
		       "\tuint8_t sig[LC_DILITHIUM_CRYPTO_BYTES];\n"
		       "};\n\n"
		       "static const struct dilithium_testvector dilithium_testvectors[] =\n"
		       "{\n");
	}
	nvectors = NVECTORS;

	(void)_lc_dilithium_keypair_from_seed;
#else
	nvectors = ARRAY_SIZE(dilithium_testvectors);

	if (ctx->ml_dsa_internal)
		printf("ML-DSA.Sign_internal / ML-DSA.Verify_internal test\n");
	else if (ctx->dilithium_prehash_type)
		printf("HashML-DSA test\n");
	else if (external_mu)
		printf("ML-DSA External Mu test\n");
	else
		printf("ML-DSA test\n");
#endif

#ifndef GENERATE_VECTORS
	if (_lc_dilithium_keypair_from_seed(&ws->pk, &ws->sk, ws->buf,
					    LC_DILITHIUM_SEEDBYTES)) {
		ret = 1;
		goto out;
	}
	if (_lc_dilithium_keypair_from_seed(&ws->pk2, &ws->sk2, ws->buf,
					    LC_DILITHIUM_SEEDBYTES)) {
		ret = 1;
		goto out;
	}

	if (memcmp(ws->pk.pk, ws->pk2.pk, LC_DILITHIUM_PUBLICKEYBYTES)) {
		printf("Public key mismatch for keygen from seed\n");
		ret = 1;
		goto out;
	}
	if (memcmp(ws->sk.sk, ws->sk2.sk, LC_DILITHIUM_SECRETKEYBYTES)) {
		printf("Secret key mismatch for keygen from seed\n");
		ret = 1;
		goto out;
	}
#endif

	if (!rounds)
		rounds = nvectors;

#ifdef LC_DILITHIUM_DEBUG
	rounds = 1;
#endif

	for (i = 0; i < rounds; ++i) {
		lc_rng_generate(selftest_rng, NULL, 0, ws->m, MLEN);
		_lc_dilithium_keypair(&ws->pk, &ws->sk, selftest_rng);

		if (external_mu) {
			BUILD_BUG_ON(MLEN != LC_DILITHIUM_CRHBYTES);
			ctx->external_mu = ws->m;
			ctx->external_mu_len = MLEN;
		}

		CKINT(_lc_dilithium_sign(&ws->sig, ctx, ws->m, MLEN, &ws->sk,
					 NULL /*selftest_rng*/));

		if (_lc_dilithium_verify(&ws->sig, ctx, ws->m, MLEN, &ws->pk))
			printf("Signature verification failed!\n");

		/* One more generation to match up with the CRYSTALS impl */
		lc_rng_generate(selftest_rng, NULL, 0, ws->seed,
				sizeof(ws->seed));
		if (rounds > nvectors)
			continue;

#ifdef GENERATE_VECTORS

		printf("\t{\n\t\t.m = {\n\t\t\t");
		for (j = 0; j < MLEN; ++j) {
			printf("0x%02x, ", ws->m[j]);
			if (j && !((j + 1) % 8))
				printf("\n\t\t\t");
		}
		printf("},\n");

		printf("\t\t.pk = {\n\t\t\t");
		for (j = 0; j < LC_DILITHIUM_PUBLICKEYBYTES; ++j) {
			printf("0x%02x, ", ws->pk.pk[j]);
			if (j && !((j + 1) % 8))
				printf("\n\t\t\t");
		}
		printf("},\n");
		printf("\t\t.sk = {\n\t\t\t");
		for (j = 0; j < LC_DILITHIUM_SECRETKEYBYTES; ++j) {
			printf("0x%02x, ", ws->sk.sk[j]);
			if (j && !((j + 1) % 8))
				printf("\n\t\t\t");
		}
		printf("},\n");
		printf("\t\t.sig = {\n\t\t\t");
		for (j = 0; j < LC_DILITHIUM_CRYPTO_BYTES; ++j) {
			printf("0x%02x, ", ws->sig.sig[j]);
			if (j && !((j + 1) % 8))
				printf("\n\t\t\t");
		}
		printf("},\n\t}, ");
#else
		if (memcmp(ws->m, vec_p[i].m, MLEN)) {
			printf("Message mismatch at test vector %u\n", i);
			ret = 1;
			goto out;
		}

		if (memcmp(ws->pk.pk, vec_p[i].pk,
			   LC_DILITHIUM_PUBLICKEYBYTES)) {
			printf("Public key mismatch at test vector %u\n", i);
			ret = 1;
			goto out;
		}
		if (memcmp(ws->sk.sk, vec_p[i].sk,
			   LC_DILITHIUM_SECRETKEYBYTES)) {
			printf("Secret key mismatch at test vector %u\n", i);
			ret = 1;
			goto out;
		}
		if (memcmp(ws->sig.sig, vec_p[i].sig,
			   LC_DILITHIUM_CRYPTO_BYTES)) {
			printf("Signature (%u) mismatch at test vector %u\n",
			       internal_test, i);
			ret = 1;
			goto out;
		}

		//printf("Sucessful validation of test vector %u\n", i);

#ifdef SHOW_SHAKEd_KEY
		printf("count = %u\n", i);
		lc_shake(lc_shake256, ws->pk.pk, LC_DILITHIUM_PUBLICKEYBYTES,
			 buf, 32);
		printf("pk = ");
		for (j = 0; j < 32; ++j)
			printf("%02x", buf[j]);
		printf("\n");
		lc_shake(lc_shake256, ws->sk.sk, LC_DILITHIUM_SECRETKEYBYTES,
			 buf, 32);
		printf("sk = ");
		for (j = 0; j < 32; ++j)
			printf("%02x", buf[j]);
		printf("\n");
		lc_shake(lc_shake256, ws->sig.sig, LC_DILITHIUM_CRYPTO_BYTES,
			 buf, 32);
		printf("sig = ");
		for (j = 0; j < 32; ++j)
			printf("%02x", buf[j]);
		printf("\n");
#endif
#endif

		if (!verify_calculation)
			continue;
	}

#ifdef GENERATE_VECTORS
	printf("\n};\n");
	printf("#endif\n");
#endif
out:
	LC_RELEASE_MEM(ws);
	return ret;
}

int _dilithium_init_update_final_tester(
	unsigned int rounds, unsigned int internal_test, unsigned int prehashed,
	int (*_lc_dilithium_keypair)(struct lc_dilithium_pk *pk,
				     struct lc_dilithium_sk *sk,
				     struct lc_rng_ctx *rng_ctx),

	int (*_lc_dilithium_sign_init)(struct lc_dilithium_ctx *ctx,
				       const struct lc_dilithium_sk *sk),
	int (*_lc_dilithium_sign_update)(struct lc_dilithium_ctx *ctx,
					 const uint8_t *m, size_t mlen),
	int (*_lc_dilithium_sign_final)(struct lc_dilithium_sig *sig,
					struct lc_dilithium_ctx *ctx,
					const struct lc_dilithium_sk *sk,
					struct lc_rng_ctx *rng_ctx),

	int (*_lc_dilithium_verify_init)(struct lc_dilithium_ctx *ctx,
					 const struct lc_dilithium_pk *pk),
	int (*_lc_dilithium_verify_update)(struct lc_dilithium_ctx *ctx,
					   const uint8_t *m, size_t mlen),
	int (*_lc_dilithium_verify_final)(const struct lc_dilithium_sig *sig,
					  struct lc_dilithium_ctx *ctx,
					  const struct lc_dilithium_pk *pk))
{
#ifdef GENERATE_VECTORS
	(void)rounds;
	(void)internal_test;
	(void)prehashed;
	(void)_lc_dilithium_keypair;
	(void)_lc_dilithium_sign_init;
	(void)_lc_dilithium_sign_update;
	(void)_lc_dilithium_sign_final;
	(void)_lc_dilithium_verify_init;
	(void)_lc_dilithium_verify_update;
	(void)_lc_dilithium_verify_final;

	return 0;
#else /* GENERATE_VECTORS */
	struct workspace {
		struct lc_dilithium_pk pk;
		struct lc_dilithium_sk sk;
		struct lc_dilithium_sig sig;
		struct lc_dilithium_sig sig_tmp;
		uint8_t m[MLEN];
		uint8_t seed[LC_DILITHIUM_CRHBYTES];
		uint8_t buf[LC_DILITHIUM_SECRETKEYBYTES];
		//uint8_t poly_uniform_gamma1_buf[POLY_UNIFORM_GAMMA1_BYTES];
		//uint8_t poly_uniform_eta_buf[POLY_UNIFORM_ETA_BYTES];
		//uint8_t poly_challenge_buf[POLY_CHALLENGE_BYTES];
		//poly c, tmp;
		//polyvecl s, y, mat[LC_DILITHIUM_K];
		//polyveck w, w1, w0, t1, t0, h;
	};
	unsigned int i, nvectors;
	int ret = 0;
	const struct dilithium_testvector *vec_p =
		internal_test ? dilithium_internal_testvectors :
		prehashed     ? dilithium_prehashed_testvectors :
				dilithium_testvectors;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));
	LC_DILITHIUM_CTX_ON_STACK(ctx);
	LC_SELFTEST_DRNG_CTX_ON_STACK(selftest_rng);

	/* we assume that all arrays have same size */
	nvectors = ARRAY_SIZE(dilithium_testvectors);

	ctx->ml_dsa_internal = !!internal_test;

	if (prehashed)
		ctx->dilithium_prehash_type = lc_shake256;

	if (ctx->ml_dsa_internal)
		printf("ML-DSA.Sign_internal / ML-DSA.Verify_internal test\n");
	else if (ctx->dilithium_prehash_type)
		printf("HashML-DSA test\n");
	else
		printf("ML-DSA test\n");

	if (!rounds)
		rounds = nvectors;

	for (i = 0; i < rounds; ++i) {
		lc_rng_generate(selftest_rng, NULL, 0, ws->m, MLEN);
		_lc_dilithium_keypair(&ws->pk, &ws->sk, selftest_rng);
		CKINT(_lc_dilithium_sign_init(ctx, &ws->sk));
		_lc_dilithium_sign_update(ctx, ws->m, 1);
		_lc_dilithium_sign_update(ctx, ws->m + 1, MLEN - 1);
		_lc_dilithium_sign_final(&ws->sig, ctx, &ws->sk,
					 NULL /*selftest_rng*/);

		_lc_dilithium_verify_init(ctx, &ws->pk);
		_lc_dilithium_verify_update(ctx, ws->m, 3);
		_lc_dilithium_verify_update(ctx, ws->m + 3, MLEN - 3);
		if (_lc_dilithium_verify_final(&ws->sig, ctx, &ws->pk))
			printf("Signature verification failed!\n");

		lc_rng_generate(selftest_rng, NULL, 0, ws->seed,
				sizeof(ws->seed));

		if (rounds > nvectors)
			continue;

		if (memcmp(ws->m, vec_p[i].m, MLEN)) {
			printf("Message mismatch at test vector %u\n", i);
			ret = 1;
			goto out;
		}

		if (memcmp(ws->pk.pk, vec_p[i].pk,
			   LC_DILITHIUM_PUBLICKEYBYTES)) {
			printf("Public key mismatch at test vector %u\n", i);
			ret = 1;
			goto out;
		}
		if (memcmp(ws->sk.sk, vec_p[i].sk,
			   LC_DILITHIUM_SECRETKEYBYTES)) {
			printf("Secret key mismatch at test vector %u\n", i);
			ret = 1;
			goto out;
		}
		if (memcmp(ws->sig.sig, vec_p[i].sig,
			   LC_DILITHIUM_CRYPTO_BYTES)) {
			printf("Signature (%u) mismatch at test vector %u\n",
			       internal_test, i);
			ret = 1;
			goto out;
		}
	}

out:
	lc_dilithium_ctx_zero(ctx);
	LC_RELEASE_MEM(ws);
	return ret;
#endif
}
