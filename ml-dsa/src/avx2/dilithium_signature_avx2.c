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

#include "alignment_x86.h"
#include "build_bug_on.h"
#include "dilithium_type.h"
#include "dilithium_pack_avx2.h"
#include "dilithium_poly_avx2.h"
#include "dilithium_poly_common.h"
#include "dilithium_polyvec_avx2.h"
#include "dilithium_debug.h"
#include "dilithium_pct.h"
#include "dilithium_signature_avx2.h"
#include "lc_rng.h"
#include "lc_sha3.h"
#include "lc_memcmp_secure.h"
#include "signature_domain_separation.h"
#include "static_rng.h"
#include "ret_checkers.h"
#include "shake_4x_avx2.h"
#include "small_stack_support.h"
#include "static_rng.h"
#include "timecop.h"
#include "visibility.h"

static inline void
polyvec_matrix_expand_row(polyvecl **row, polyvecl buf[2],
			  const uint8_t rho[LC_DILITHIUM_SEEDBYTES],
			  unsigned int i, void *ws_buf, void *ws_keccak)
{
	switch (i) {
	case 0:
		polyvec_matrix_expand_row0(buf, buf + 1, rho, ws_buf,
					   ws_keccak);
		*row = buf;
		break;
	case 1:
		polyvec_matrix_expand_row1(buf + 1, buf, rho, ws_buf,
					   ws_keccak);
		*row = buf + 1;
		break;
	case 2:
		polyvec_matrix_expand_row2(buf, buf + 1, rho, ws_buf,
					   ws_keccak);
		*row = buf;
		break;
	case 3:
		polyvec_matrix_expand_row3(buf + 1, buf, rho, ws_buf,
					   ws_keccak);
		*row = buf + 1;
		break;
	case 4:
		polyvec_matrix_expand_row4(buf, buf + 1, rho, ws_buf,
					   ws_keccak);
		*row = buf;
		break;
	case 5:
		polyvec_matrix_expand_row5(buf + 1, buf, rho, ws_buf,
					   ws_keccak);
		*row = buf + 1;
		break;
#if LC_DILITHIUM_K > 6
	case 6:
		polyvec_matrix_expand_row6(buf, buf + 1, rho, ws_buf,
					   ws_keccak);
		*row = buf;
		break;
	case 7:
		polyvec_matrix_expand_row7(buf + 1, buf, rho, ws_buf,
					   ws_keccak);
		*row = buf + 1;
		break;
#endif
	}
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_keypair_avx2,
		      struct lc_dilithium_pk *pk, struct lc_dilithium_sk *sk,
		      struct lc_rng_ctx *rng_ctx)
{
	struct workspace {
		union {
			BUF_ALIGNED_UINT8_M256I(REJ_UNIFORM_BUFLEN + 8)
			poly_uniform_4x_buf[4];
			BUF_ALIGNED_UINT8_M256I(REJ_UNIFORM_ETA_BUFLEN)
			poly_uniform_eta_4x_buf[4];
		} tmp;
		uint8_t seedbuf[2 * LC_DILITHIUM_SEEDBYTES +
				LC_DILITHIUM_CRHBYTES];
		polyvecl rowbuf[2], s1;
		polyveck s2;
		poly t1, t0;
		keccakx4_state keccak_state;
	};
	LC_FIPS_RODATA_SECTION
	static const uint8_t dimension[2] = { LC_DILITHIUM_K, LC_DILITHIUM_L };
	unsigned int i;
	const uint8_t *rho, *rhoprime, *key;
	polyvecl *row;
	int ret;
	LC_HASH_CTX_ON_STACK(shake256_ctx, lc_shake256);
	LC_DECLARE_MEM(ws, struct workspace, 32);

	if (!pk || !sk || !rng_ctx) {
		ret = -EINVAL;
		goto out;
	}

	row = ws->rowbuf;

	/* Get randomness for rho, rhoprime and key */
	CKINT(lc_rng_generate(rng_ctx, NULL, 0, ws->seedbuf,
			      LC_DILITHIUM_SEEDBYTES));
	CKINT(lc_hash_init(shake256_ctx));
	lc_hash_update(shake256_ctx, ws->seedbuf, LC_DILITHIUM_SEEDBYTES);
	lc_hash_update(shake256_ctx, dimension, sizeof(dimension));
	lc_hash_set_digestsize(shake256_ctx, sizeof(ws->seedbuf));
	lc_hash_final(shake256_ctx, ws->seedbuf);
	lc_hash_zero(shake256_ctx);

	rho = ws->seedbuf;
	rhoprime = rho + LC_DILITHIUM_SEEDBYTES;
	key = rhoprime + LC_DILITHIUM_CRHBYTES;

	/* Timecop: key goes into the secret key */
	poison(key, LC_DILITHIUM_SEEDBYTES);

	/* Store rho, key */
	memcpy(pk->pk, rho, LC_DILITHIUM_SEEDBYTES);
	memcpy(sk->sk, rho, LC_DILITHIUM_SEEDBYTES);
	memcpy(sk->sk + LC_DILITHIUM_SEEDBYTES, key, LC_DILITHIUM_SEEDBYTES);

#if LC_DILITHIUM_K == 8 && LC_DILITHIUM_L == 7
	poly_uniform_eta_4x_avx(&ws->s1.vec[0], &ws->s1.vec[1], &ws->s1.vec[2],
				&ws->s1.vec[3], rhoprime, 0, 1, 2, 3,
				ws->tmp.poly_uniform_eta_4x_buf,
				&ws->keccak_state);
	poly_uniform_eta_4x_avx(&ws->s1.vec[4], &ws->s1.vec[5], &ws->s1.vec[6],
				&ws->s2.vec[0], rhoprime, 4, 5, 6, 7,
				ws->tmp.poly_uniform_eta_4x_buf,
				&ws->keccak_state);
	poly_uniform_eta_4x_avx(&ws->s2.vec[1], &ws->s2.vec[2], &ws->s2.vec[3],
				&ws->s2.vec[4], rhoprime, 8, 9, 10, 11,
				ws->tmp.poly_uniform_eta_4x_buf,
				&ws->keccak_state);
	poly_uniform_eta_4x_avx(&ws->s2.vec[5], &ws->s2.vec[6], &ws->s2.vec[7],
				&ws->t0, rhoprime, 12, 13, 14, 15,
				ws->tmp.poly_uniform_eta_4x_buf,
				&ws->keccak_state);
#elif LC_DILITHIUM_K == 6 && LC_DILITHIUM_L == 5
	poly_uniform_eta_4x_avx(&ws->s1.vec[0], &ws->s1.vec[1], &ws->s1.vec[2],
				&ws->s1.vec[3], rhoprime, 0, 1, 2, 3,
				ws->tmp.poly_uniform_eta_4x_buf,
				&ws->keccak_state);
	poly_uniform_eta_4x_avx(&ws->s1.vec[4], &ws->s2.vec[0], &ws->s2.vec[1],
				&ws->s2.vec[2], rhoprime, 4, 5, 6, 7,
				ws->tmp.poly_uniform_eta_4x_buf,
				&ws->keccak_state);
	poly_uniform_eta_4x_avx(&ws->s2.vec[3], &ws->s2.vec[4], &ws->s2.vec[5],
				&ws->t0, rhoprime, 8, 9, 10, 11,
				ws->tmp.poly_uniform_eta_4x_buf,
				&ws->keccak_state);
#else
#error "Undefined LC_DILITHIUM_K"
#endif

	/* Timecop: s1 and s2 are secret */
	poison(&ws->s1, sizeof(polyvecl));
	poison(&ws->s2, sizeof(polyveck));

	/* Pack secret vectors */
	for (i = 0; i < LC_DILITHIUM_L; i++)
		polyeta_pack_avx(sk->sk + 2 * LC_DILITHIUM_SEEDBYTES +
					 LC_DILITHIUM_TRBYTES +
					 i * LC_DILITHIUM_POLYETA_PACKEDBYTES,
				 &ws->s1.vec[i]);
	for (i = 0; i < LC_DILITHIUM_K; i++)
		polyeta_pack_avx(sk->sk + 2 * LC_DILITHIUM_SEEDBYTES +
					 LC_DILITHIUM_TRBYTES +
					 (LC_DILITHIUM_L +
					  i) * LC_DILITHIUM_POLYETA_PACKEDBYTES,
				 &ws->s2.vec[i]);

	/* Transform s1 */
	polyvecl_ntt_avx(&ws->s1);
	dilithium_print_polyvecl(&ws->s1,
				 "Keygen - S1 L x N matrix after NTT:");

	for (i = 0; i < LC_DILITHIUM_K; i++) {
		polyvec_matrix_expand_row(&row, ws->rowbuf, rho, i,
					  ws->tmp.poly_uniform_4x_buf,
					  &ws->keccak_state);

		/* Compute inner-product */
		polyvecl_pointwise_acc_montgomery_avx(&ws->t1, row, &ws->s1);
		dilithium_print_poly(&ws->t1,
				     "Keygen - T N vector after A*NTT(s1):");

		poly_invntt_tomont_avx(&ws->t1);

		/* Add error polynomial */
		poly_add_avx(&ws->t1, &ws->t1, &ws->s2.vec[i]);

		/* Round t and pack t1, t0 */
		poly_caddq_avx(&ws->t1);
		poly_power2round_avx(&ws->t1, &ws->t0, &ws->t1);

		polyt1_pack_avx(pk->pk + LC_DILITHIUM_SEEDBYTES +
					i * LC_DILITHIUM_POLYT1_PACKEDBYTES,
				&ws->t1);
		polyt0_pack_avx(
			sk->sk + 2 * LC_DILITHIUM_SEEDBYTES +
				LC_DILITHIUM_TRBYTES +
				(LC_DILITHIUM_L + LC_DILITHIUM_K) *
					LC_DILITHIUM_POLYETA_PACKEDBYTES +
				i * LC_DILITHIUM_POLYT0_PACKEDBYTES,
			&ws->t0);
	}

	dilithium_print_buffer(pk->pk, LC_DILITHIUM_PUBLICKEYBYTES,
			       "Keygen - PK after pkEncode:");

	/* Compute H(rho, t1) and store in secret key */
	CKINT(lc_xof(lc_shake256, pk->pk, LC_DILITHIUM_PUBLICKEYBYTES,
		     sk->sk + 2 * LC_DILITHIUM_SEEDBYTES,
		     LC_DILITHIUM_TRBYTES));

	/* Timecop: pk and sk are not relevant for side-channels any more. */
	unpoison(pk->pk, sizeof(pk->pk));
	unpoison(sk->sk, sizeof(sk->sk));

	CKINT(lc_dilithium_pct_fips(pk, sk));

out:
	LC_RELEASE_MEM(ws);
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_keypair_from_seed_avx2,
		      struct lc_dilithium_pk *pk, struct lc_dilithium_sk *sk,
		      const uint8_t *seed, size_t seedlen)
{
	struct lc_static_rng_data s_rng_state;
	LC_STATIC_DRNG_ON_STACK(s_drng, &s_rng_state);
	int ret;

	if (seedlen != LC_DILITHIUM_SEEDBYTES)
		return -EINVAL;

	/* Set the seed that the key generation can pull via the RNG. */
	s_rng_state.seed = seed;
	s_rng_state.seedlen = seedlen;

	/* Generate the key pair from the seed. */
	CKINT(lc_dilithium_keypair_avx2(pk, sk, &s_drng));

out:
	return ret;
}

static int lc_dilithium_sign_avx2_internal(struct lc_dilithium_sig *sig,
					   struct lc_dilithium_ctx *ctx,
					   const struct lc_dilithium_sk *sk,
					   struct lc_rng_ctx *rng_ctx)
{
	struct workspace_sign {
		union {
			BUF_ALIGNED_UINT8_M256I(REJ_UNIFORM_BUFLEN + 8)
			poly_uniform_4x_buf[4];
			BUF_ALIGNED_UINT8_M256I(POLY_UNIFORM_GAMMA1_NBLOCKS *
							LC_SHAKE_256_SIZE_BLOCK +
						14)
			poly_uniform_gamma1[4];
		} buf;
		uint8_t seedbuf[LC_DILITHIUM_SEEDBYTES + LC_DILITHIUM_RNDBYTES +
				2 * LC_DILITHIUM_CRHBYTES];
		uint8_t hintbuf[LC_DILITHIUM_N];
		polyvecl mat[LC_DILITHIUM_K], s1, z;
		polyveck t0, s2, w1;
		poly c, tmp;
		union {
			polyvecl y;
			polyveck w0;
		} tmpv;
		keccakx4_state keccak_state;
	};
	unsigned int i, n, pos;
	uint8_t *rho, *key, *mu, *rhoprime, *rnd;
	uint8_t *hint = sig->sig + LC_DILITHIUM_CTILDE_BYTES +
			LC_DILITHIUM_L * LC_DILITHIUM_POLYZ_PACKEDBYTES;
	uint64_t nonce = 0;
	int ret = 0;
	struct lc_hash_ctx *hash_ctx = &ctx->dilithium_hash_ctx;
	LC_DECLARE_MEM(ws, struct workspace_sign, 32);

	/* Skip tr which is in rho + LC_DILITHIUM_SEEDBYTES; */
	key = ws->seedbuf;
	rnd = key + LC_DILITHIUM_SEEDBYTES;
	mu = rnd + LC_DILITHIUM_RNDBYTES;
	rhoprime = mu + LC_DILITHIUM_CRHBYTES;

	/*
	 * If the external mu is provided, use this verbatim, otherwise
	 * calculate the mu value.
	 */
	if (ctx->external_mu) {
		if (ctx->external_mu_len != LC_DILITHIUM_CRHBYTES)
			return -EINVAL;
		memcpy(mu, ctx->external_mu, LC_DILITHIUM_CRHBYTES);
	} else {
		/*
		 * Set the digestsize - for SHA512 this is a noop, for SHAKE256,
		 * it sets the value. The BUILD_BUG_ON is to check that the
		 * SHA-512 output size is identical to the expected length.
		 */
		BUILD_BUG_ON(LC_DILITHIUM_CRHBYTES != LC_SHA3_512_SIZE_DIGEST);

		/* Compute CRH(tr, msg) */
		lc_hash_set_digestsize(hash_ctx, LC_DILITHIUM_CRHBYTES);
		lc_hash_final(hash_ctx, mu);
	}

	if (rng_ctx) {
		CKINT(lc_rng_generate(rng_ctx, NULL, 0, rnd,
				      LC_DILITHIUM_RNDBYTES));
	} else {
		memset(rnd, 0, LC_DILITHIUM_RNDBYTES);
	}

	unpack_sk_key_avx2(key, sk);

	/* Timecop: key is secret */
	poison(key, LC_DILITHIUM_SEEDBYTES);

	CKINT(lc_xof(lc_shake256, key,
		     LC_DILITHIUM_SEEDBYTES + LC_DILITHIUM_RNDBYTES +
			     LC_DILITHIUM_CRHBYTES,
		     rhoprime, LC_DILITHIUM_CRHBYTES));

	/*
	 * Timecop: RHO' is the hash of the secret value of key which is
	 * enlarged to sample the intermediate vector y from. Due to the hashing
	 * any side channel on RHO' cannot allow the deduction of the original
	 * key.
	 */
	unpoison(rhoprime, LC_DILITHIUM_CRHBYTES);

	/* Expand matrix and transform vectors */
	rho = ws->seedbuf;
	unpack_sk_rho_avx2(rho, sk);
	polyvec_matrix_expand(ws->mat, rho, ws->buf.poly_uniform_4x_buf,
			      &ws->keccak_state, &ws->z);
	unpack_sk_s1_avx2(&ws->s1, sk);

	/* Timecop: s1 is secret */
	poison(&ws->s1, sizeof(polyvecl));

	unpack_sk_s2_avx2(&ws->s2, sk);

	/* Timecop: s2 is secret */
	poison(&ws->s2, sizeof(polyveck));

	unpack_sk_t0_avx2(&ws->t0, sk);
	polyvecl_ntt_avx(&ws->s1);
	polyveck_ntt_avx(&ws->s2);
	polyveck_ntt_avx(&ws->t0);

rej:
	/* Sample intermediate vector y */
#if LC_DILITHIUM_L == 7
	poly_uniform_gamma1_4x_avx(&ws->z.vec[0], &ws->z.vec[1], &ws->z.vec[2],
				   &ws->z.vec[3], rhoprime, (uint16_t)nonce,
				   (uint16_t)(nonce + 1), (uint16_t)(nonce + 2),
				   (uint16_t)(nonce + 3),
				   ws->buf.poly_uniform_gamma1,
				   &ws->keccak_state);
	poly_uniform_gamma1_4x_avx(&ws->z.vec[4], &ws->z.vec[5], &ws->z.vec[6],
				   &ws->tmp, rhoprime, (uint16_t)(nonce + 4),
				   (uint16_t)(nonce + 5), (uint16_t)(nonce + 6),
				   0, ws->buf.poly_uniform_gamma1,
				   &ws->keccak_state);
	nonce += 7;
#elif LC_DILITHIUM_L == 5
	poly_uniform_gamma1_4x_avx(&ws->z.vec[0], &ws->z.vec[1], &ws->z.vec[2],
				   &ws->z.vec[3], rhoprime, (uint16_t)nonce,
				   (uint16_t)(nonce + 1), (uint16_t)(nonce + 2),
				   (uint16_t)(nonce + 3),
				   ws->buf.poly_uniform_gamma1,
				   &ws->keccak_state);
	poly_uniform_gamma1(&ws->z.vec[4], rhoprime, (uint16_t)(nonce + 4),
			    ws->buf.poly_uniform_gamma1);
	nonce += 5;
#else
#error "Undefined LC_DILITHIUM_K"
#endif

	/* Timecop: s2 is secret */
	poison(&ws->tmpv.y, sizeof(polyvecl));

	/* Matrix-vector product */
	ws->tmpv.y = ws->z;

	polyvecl_ntt_avx(&ws->tmpv.y);
	polyvec_matrix_pointwise_montgomery_avx(&ws->w1, ws->mat, &ws->tmpv.y);
	polyveck_invntt_tomont_avx(&ws->w1);

	/* Decompose w and call the random oracle */
	polyveck_caddq_avx(&ws->w1);
	polyveck_decompose_avx(&ws->w1, &ws->tmpv.w0, &ws->w1);

	polyveck_pack_w1_avx(sig->sig, &ws->w1);

	CKINT(lc_hash_init(hash_ctx));
	lc_hash_update(hash_ctx, mu, LC_DILITHIUM_CRHBYTES);
	lc_hash_update(hash_ctx, sig->sig,
		       LC_DILITHIUM_K * LC_DILITHIUM_POLYW1_PACKEDBYTES);
	lc_hash_set_digestsize(hash_ctx, LC_DILITHIUM_CTILDE_BYTES);
	lc_hash_final(hash_ctx, sig->sig);
	lc_hash_zero(hash_ctx);

	poly_challenge_avx(&ws->c, sig->sig);
	poly_ntt_avx(&ws->c);

	/* Compute z, reject if it reveals secret */
	for (i = 0; i < LC_DILITHIUM_L; i++) {
		poly_pointwise_montgomery_avx(&ws->tmp, &ws->c, &ws->s1.vec[i]);
		poly_invntt_tomont_avx(&ws->tmp);
		poly_add_avx(&ws->z.vec[i], &ws->z.vec[i], &ws->tmp);
		poly_reduce_avx(&ws->z.vec[i]);

		/*
		 * Timecop: the signature component z is not sensitive any
		 * more.
		 */
		unpoison(&ws->z.vec[i], sizeof(poly));

		if (poly_chknorm_avx(&ws->z.vec[i],
				     LC_DILITHIUM_GAMMA1 - LC_DILITHIUM_BETA)) {
			dilithium_print_polyvecl(&ws->z,
						 "Siggen - z rejection");
			goto rej;
		}
	}

	/* Zero hint vector in signature */
	pos = 0;
	memset(hint, 0, LC_DILITHIUM_OMEGA);

	for (i = 0; i < LC_DILITHIUM_K; i++) {
		/*
		 * Check that subtracting cs2 does not change high bits of
		 * w and low bits do not reveal secret information
		 */
		poly_pointwise_montgomery_avx(&ws->tmp, &ws->c, &ws->s2.vec[i]);
		poly_invntt_tomont_avx(&ws->tmp);
		poly_sub_avx(&ws->tmpv.w0.vec[i], &ws->tmpv.w0.vec[i],
			     &ws->tmp);
		poly_reduce_avx(&ws->tmpv.w0.vec[i]);

		/* Timecop: verification data w0 is not sensitive any more. */
		unpoison(&ws->tmpv.w0.vec[i], sizeof(poly));

		if (poly_chknorm_avx(&ws->tmpv.w0.vec[i],
				     LC_DILITHIUM_GAMMA2 - LC_DILITHIUM_BETA)) {
			dilithium_print_polyveck(&ws->tmpv.w0,
						 "Siggen - r0 rejection");
			goto rej;
		}

		/* Compute hints */
		poly_pointwise_montgomery_avx(&ws->tmp, &ws->c, &ws->t0.vec[i]);
		poly_invntt_tomont_avx(&ws->tmp);
		poly_reduce_avx(&ws->tmp);

		/* Timecop: the hint information is not sensitive any more. */
		unpoison(&ws->tmp, sizeof(poly));

		if (poly_chknorm_avx(&ws->tmp, LC_DILITHIUM_GAMMA2)) {
			dilithium_print_poly(&ws->tmp,
					     "Siggen - ct0 rejection");
			goto rej;
		}

		poly_add_avx(&ws->tmpv.w0.vec[i], &ws->tmpv.w0.vec[i],
			     &ws->tmp);
		n = poly_make_hint_avx(ws->hintbuf, &ws->tmpv.w0.vec[i],
				       &ws->w1.vec[i]);
		if (pos + n > LC_DILITHIUM_OMEGA) {
			dilithium_print_polyveck(&ws->tmpv.w0,
						 "Siggen - h rejection");
			goto rej;
		}

		/* Store hints in signature */
		memcpy(&hint[pos], ws->hintbuf, n);
		pos = pos + n;
		hint[LC_DILITHIUM_OMEGA + i] = (uint8_t)pos;
	}

	/* Pack z into signature */
	for (i = 0; i < LC_DILITHIUM_L; i++)
		polyz_pack_avx(sig->sig + LC_DILITHIUM_CTILDE_BYTES +
				       i * LC_DILITHIUM_POLYZ_PACKEDBYTES,
			       &ws->z.vec[i]);

out:
	LC_RELEASE_MEM(ws);
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_sign_ctx_avx2,
		      struct lc_dilithium_sig *sig,
		      struct lc_dilithium_ctx *ctx, const uint8_t *m,
		      size_t mlen, const struct lc_dilithium_sk *sk,
		      struct lc_rng_ctx *rng_ctx)
{
	uint8_t tr[LC_DILITHIUM_TRBYTES];
	int ret = 0;

	/* rng_ctx is allowed to be NULL as handled below */
	if (!sig || !sk || !ctx)
		return -EINVAL;
	/* Either the message or the external mu must be provided */
	if (!m && !ctx->external_mu)
		return -EINVAL;

	unpack_sk_tr_avx2(tr, sk);

	if (m) {
		/* Compute mu = CRH(tr, msg) */
		struct lc_hash_ctx *hash_ctx = &ctx->dilithium_hash_ctx;

		CKINT(lc_hash_init(hash_ctx));
		lc_hash_update(hash_ctx, tr, LC_DILITHIUM_TRBYTES);
		CKINT(signature_domain_separation(
			&ctx->dilithium_hash_ctx, ctx->ml_dsa_internal,
			ctx->dilithium_prehash_type, ctx->userctx,
			ctx->userctxlen, m, mlen, ctx->randomizer,
			ctx->randomizerlen, LC_DILITHIUM_NIST_CATEGORY));
	}

	ret = lc_dilithium_sign_avx2_internal(sig, ctx, sk, rng_ctx);

out:
	lc_memset_secure(tr, 0, sizeof(tr));
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_sign_avx2, struct lc_dilithium_sig *sig,
		      const uint8_t *m, size_t mlen,
		      const struct lc_dilithium_sk *sk,
		      struct lc_rng_ctx *rng_ctx)
{
	LC_DILITHIUM_CTX_ON_STACK(ctx);
	int ret = lc_dilithium_sign_ctx_avx2(sig, ctx, m, mlen, sk, rng_ctx);

	lc_dilithium_ctx_zero(ctx);
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_sign_init_avx2,
		      struct lc_dilithium_ctx *ctx,
		      const struct lc_dilithium_sk *sk)
{
	uint8_t tr[LC_DILITHIUM_TRBYTES];
	struct lc_hash_ctx *hash_ctx;
	int ret;

	/* rng_ctx is allowed to be NULL as handled below */
	if (!ctx || !sk)
		return -EINVAL;

	hash_ctx = &ctx->dilithium_hash_ctx;

	/* Require the use of SHAKE256 */
	if (hash_ctx->hash != lc_shake256)
		return -EOPNOTSUPP;

	unpack_sk_tr_avx2(tr, sk);

	/* Compute mu = CRH(tr, msg) */
	CKINT(lc_hash_init(hash_ctx));
	lc_hash_update(hash_ctx, tr, LC_DILITHIUM_TRBYTES);
	lc_memset_secure(tr, 0, sizeof(tr));

	CKINT(signature_domain_separation(
		&ctx->dilithium_hash_ctx, ctx->ml_dsa_internal,
		ctx->dilithium_prehash_type, ctx->userctx, ctx->userctxlen,
		NULL, 0, ctx->randomizer, ctx->randomizerlen,
		LC_DILITHIUM_NIST_CATEGORY));

out:
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_sign_update_avx2,
		      struct lc_dilithium_ctx *ctx, const uint8_t *m,
		      size_t mlen)
{
	struct lc_hash_ctx *hash_ctx;

	if (!ctx || !m)
		return -EINVAL;

	hash_ctx = &ctx->dilithium_hash_ctx;

	/* Compute CRH(tr, msg) */
	lc_hash_update(hash_ctx, m, mlen);

	return 0;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_sign_final_avx2,
		      struct lc_dilithium_sig *sig,
		      struct lc_dilithium_ctx *ctx,
		      const struct lc_dilithium_sk *sk,
		      struct lc_rng_ctx *rng_ctx)
{
	int ret = 0;

	/* rng_ctx is allowed to be NULL as handled below */
	if (!sig || !ctx || !sk) {
		ret = -EINVAL;
		goto out;
	}

	ret = lc_dilithium_sign_avx2_internal(sig, ctx, sk, rng_ctx);

out:
	lc_dilithium_ctx_zero(ctx);
	return ret;
}

static int lc_dilithium_verify_avx2_internal(const struct lc_dilithium_sig *sig,
					     const struct lc_dilithium_pk *pk,
					     struct lc_dilithium_ctx *ctx)
{
	struct workspace_verify {
		/* polyw1_pack writes additional 14 bytes */
		BUF_ALIGNED_UINT8_M256I(
			LC_DILITHIUM_K *LC_DILITHIUM_POLYW1_PACKEDBYTES + 14)
		buf;
		BUF_ALIGNED_UINT8_M256I(REJ_UNIFORM_BUFLEN + 8)
		poly_uniform_4x_buf[4];
		uint8_t mu[LC_DILITHIUM_CRHBYTES];
		polyvecl rowbuf[2];
		polyvecl z;
		poly c, w1, h;
		keccakx4_state keccak_state;
	};
	unsigned int i, j, pos = 0;
	const uint8_t *hint = sig->sig + LC_DILITHIUM_CTILDE_BYTES +
			      LC_DILITHIUM_L * LC_DILITHIUM_POLYZ_PACKEDBYTES;
	polyvecl *row;
	int ret = 0;
	struct lc_hash_ctx *hash_ctx = &ctx->dilithium_hash_ctx;
	LC_DECLARE_MEM(ws, struct workspace_verify, 32);

	row = ws->rowbuf;

	/* Expand challenge */
	poly_challenge_avx(&ws->c, sig->sig);
	poly_ntt_avx(&ws->c);

	/* Unpack z; shortness follows from unpacking */
	for (i = 0; i < LC_DILITHIUM_L; i++) {
		polyz_unpack_avx(&ws->z.vec[i],
				 sig->sig + LC_DILITHIUM_CTILDE_BYTES +
					 i * LC_DILITHIUM_POLYZ_PACKEDBYTES);
		poly_ntt_avx(&ws->z.vec[i]);
	}

	for (i = 0; i < LC_DILITHIUM_K; i++) {
		polyvec_matrix_expand_row(&row, ws->rowbuf, pk->pk, i,
					  ws->poly_uniform_4x_buf,
					  &ws->keccak_state);

		/* Compute i-th row of Az - c2^Dt1 */
		polyvecl_pointwise_acc_montgomery_avx(&ws->w1, row, &ws->z);

		polyt1_unpack_avx(&ws->h,
				  pk->pk + LC_DILITHIUM_SEEDBYTES +
					  i * LC_DILITHIUM_POLYT1_PACKEDBYTES);
		poly_shiftl_avx(&ws->h);
		poly_ntt_avx(&ws->h);
		poly_pointwise_montgomery_avx(&ws->h, &ws->c, &ws->h);

		poly_sub_avx(&ws->w1, &ws->w1, &ws->h);
		poly_reduce_avx(&ws->w1);
		poly_invntt_tomont_avx(&ws->w1);

		/* Get hint polynomial and reconstruct w1 */
		memset(ws->h.coeffs, 0, sizeof(poly));
		if (hint[LC_DILITHIUM_OMEGA + i] < pos ||
		    hint[LC_DILITHIUM_OMEGA + i] > LC_DILITHIUM_OMEGA) {
			ret = -1;
			goto out;
		}

		for (j = pos; j < hint[LC_DILITHIUM_OMEGA + i]; ++j) {
			/* Coefficients are ordered for strong unforgeability */
			if (j > pos && hint[j] <= hint[j - 1]) {
				ret = -1;
				goto out;
			}
			ws->h.coeffs[hint[j]] = 1;
		}
		pos = hint[LC_DILITHIUM_OMEGA + i];

		poly_caddq_avx(&ws->w1);
		poly_use_hint_avx(&ws->w1, &ws->w1, &ws->h);
		polyw1_pack_avx(ws->buf.coeffs +
					i * LC_DILITHIUM_POLYW1_PACKEDBYTES,
				&ws->w1);
	}

	/* Extra indices are zero for strong unforgeability */
	for (j = pos; j < LC_DILITHIUM_OMEGA; ++j) {
		if (hint[j]) {
			ret = -1;
			goto out;
		}
	}

	if (ctx->external_mu) {
		if (ctx->external_mu_len != LC_DILITHIUM_CRHBYTES)
			return -EINVAL;

		/* Call random oracle and verify challenge */
		CKINT(lc_hash_init(hash_ctx));
		lc_hash_update(hash_ctx, ctx->external_mu,
			       LC_DILITHIUM_CRHBYTES);
	} else {
		lc_hash_set_digestsize(hash_ctx, LC_DILITHIUM_CRHBYTES);
		lc_hash_final(hash_ctx, ws->mu);

		/* Call random oracle and verify challenge */
		CKINT(lc_hash_init(hash_ctx));
		lc_hash_update(hash_ctx, ws->mu, LC_DILITHIUM_CRHBYTES);
	}

	lc_hash_update(hash_ctx, ws->buf.coeffs,
		       LC_DILITHIUM_K * LC_DILITHIUM_POLYW1_PACKEDBYTES);
	lc_hash_set_digestsize(hash_ctx, LC_DILITHIUM_CTILDE_BYTES);
	lc_hash_final(hash_ctx, ws->buf.coeffs);
	lc_hash_zero(hash_ctx);

	/* Signature verification operation */
	if (lc_memcmp_secure(ws->buf.coeffs, LC_DILITHIUM_CTILDE_BYTES,
			     sig->sig, LC_DILITHIUM_CTILDE_BYTES))
		ret = -EBADMSG;

out:
	LC_RELEASE_MEM(ws);
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_verify_ctx_avx2,
		      const struct lc_dilithium_sig *sig,
		      struct lc_dilithium_ctx *ctx, const uint8_t *m,
		      size_t mlen, const struct lc_dilithium_pk *pk)
{
	uint8_t tr[LC_DILITHIUM_TRBYTES];
	int ret = 0;

	if (!sig || !pk || !ctx)
		return -EINVAL;
	/* Either the message or the external mu must be provided */
	if (!m && !ctx->external_mu)
		return -EINVAL;

	/* Compute CRH(H(rho, t1), msg) */
	CKINT(lc_xof(lc_shake256, pk->pk, LC_DILITHIUM_PUBLICKEYBYTES, tr,
		     LC_DILITHIUM_TRBYTES));

	if (m) {
		struct lc_hash_ctx *hash_ctx = &ctx->dilithium_hash_ctx;
		CKINT(lc_hash_init(hash_ctx));
		lc_hash_update(hash_ctx, tr, LC_DILITHIUM_TRBYTES);
		CKINT(signature_domain_separation(
			&ctx->dilithium_hash_ctx, ctx->ml_dsa_internal,
			ctx->dilithium_prehash_type, ctx->userctx,
			ctx->userctxlen, m, mlen, ctx->randomizer,
			ctx->randomizerlen, LC_DILITHIUM_NIST_CATEGORY));
	}

	ret = lc_dilithium_verify_avx2_internal(sig, pk, ctx);

out:
	lc_memset_secure(tr, 0, sizeof(tr));
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_verify_avx2,
		      const struct lc_dilithium_sig *sig, const uint8_t *m,
		      size_t mlen, const struct lc_dilithium_pk *pk)
{
	LC_DILITHIUM_CTX_ON_STACK(ctx);
	int ret = lc_dilithium_verify_ctx_avx2(sig, ctx, m, mlen, pk);

	lc_dilithium_ctx_zero(ctx);
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_verify_init_avx2,
		      struct lc_dilithium_ctx *ctx,
		      const struct lc_dilithium_pk *pk)
{
	uint8_t tr[LC_DILITHIUM_TRBYTES];
	struct lc_hash_ctx *hash_ctx;
	int ret;

	/* rng_ctx is allowed to be NULL as handled below */
	if (!ctx || !pk)
		return -EINVAL;

	hash_ctx = &ctx->dilithium_hash_ctx;

	/* Require the use of SHAKE256 */
	if (hash_ctx->hash != lc_shake256)
		return -EOPNOTSUPP;

	/* Compute CRH(H(rho, t1), msg) */
	CKINT(lc_xof(lc_shake256, pk->pk, LC_DILITHIUM_PUBLICKEYBYTES, tr,
		     LC_DILITHIUM_TRBYTES));

	CKINT(lc_hash_init(hash_ctx));
	lc_hash_update(hash_ctx, tr, LC_DILITHIUM_TRBYTES);
	lc_memset_secure(tr, 0, sizeof(tr));

	CKINT(signature_domain_separation(
		&ctx->dilithium_hash_ctx, ctx->ml_dsa_internal,
		ctx->dilithium_prehash_type, ctx->userctx, ctx->userctxlen,
		NULL, 0, ctx->randomizer, ctx->randomizerlen,
		LC_DILITHIUM_NIST_CATEGORY));

out:
	return ret;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_verify_update_avx2,
		      struct lc_dilithium_ctx *ctx, const uint8_t *m,
		      size_t mlen)
{
	struct lc_hash_ctx *hash_ctx;

	if (!ctx || !m)
		return -EINVAL;

	hash_ctx = &ctx->dilithium_hash_ctx;

	/* Compute CRH(H(rho, t1), msg) */
	lc_hash_update(hash_ctx, m, mlen);

	return 0;
}

LC_INTERFACE_FUNCTION(int, lc_dilithium_verify_final_avx2,
		      const struct lc_dilithium_sig *sig,
		      struct lc_dilithium_ctx *ctx,
		      const struct lc_dilithium_pk *pk)
{
	int ret = 0;

	if (!sig || !ctx || !pk) {
		ret = -EINVAL;
		goto out;
	}

	ret = lc_dilithium_verify_avx2_internal(sig, pk, ctx);

out:
	lc_dilithium_ctx_zero(ctx);
	return ret;
}
