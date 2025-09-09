/*
 * Copyright (C) 2016 - 2025, Stephan Mueller <smueller@chronox.de>
 *
 * License: see COPYING file in root directory
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

#include "alignment.h"
#include "compare.h"
#include "conv_be_le.h"
#include "lc_chacha20.h"
#include "lc_chacha20_drng.h"
#include "lc_chacha20_private.h"
#include "math_helper.h"
#include "visibility.h"

static void cc20_drng_selftest(void)
{
	uint8_t outbuf[LC_CC20_KEY_SIZE] __align(sizeof(uint32_t));
	struct lc_sym_ctx *sym_ctx;
	struct lc_sym_state *chacha20_state;

	/*
	 * Expected result when ChaCha20 DRNG state is zero:
	 *	* constants are set to "expand 32-byte k"
	 *	* remaining state is 0
	 * and pulling one ChaCha20 DRNG block.
	 */
	LC_FIPS_RODATA_SECTION
	static const uint8_t expected_block[LC_CC20_KEY_SIZE] = {
		0x76, 0xb8, 0xe0, 0xad, 0xa0, 0xf1, 0x3d, 0x90,
		0x40, 0x5d, 0x6a, 0xe5, 0x53, 0x86, 0xbd, 0x28,
		0xbd, 0xd2, 0x19, 0xb8, 0xa0, 0x8d, 0xed, 0x1a,
		0xa8, 0x36, 0xef, 0xcc, 0x8b, 0x77, 0x0d, 0xc7
	};
	LC_CC20_DRNG_CTX_ON_STACK(cc20_ctx);

	LC_SELFTEST_RUN(LC_ALG_STATUS_CHACHA20_DRNG);

	sym_ctx = &cc20_ctx->cc20;
	chacha20_state = sym_ctx->sym_state;

	/* Generate with zero state */
	chacha20_state->counter[0] = 0;

	lc_cc20_drng_generate(cc20_ctx, outbuf, sizeof(expected_block));
	lc_compare_selftest(LC_ALG_STATUS_CHACHA20_DRNG, outbuf, expected_block,
			    sizeof(expected_block), "ChaCha20 DRNG");
	lc_cc20_drng_zero(cc20_ctx);
}

/**
 * Update of the ChaCha20 state by generating one ChaCha20 block which is
 * equal to the state of the ChaCha20. The generated block is XORed into
 * the key part of the state. This shall ensure backtracking resistance as well
 * as a proper mix of the ChaCha20 state once the key is injected.
 */
static inline void lc_cc20_drng_update(struct lc_chacha20_drng_ctx *cc20_ctx,
				       uint32_t *buf, size_t used_words)
{
	struct lc_sym_ctx *sym_ctx;
	struct lc_sym_state *chacha20_state;
	uint32_t i, tmp[LC_CC20_BLOCK_SIZE_WORDS];

	if (!cc20_ctx)
		return;
	sym_ctx = &cc20_ctx->cc20;
	chacha20_state = sym_ctx->sym_state;

	if (used_words > LC_CC20_KEY_SIZE_WORDS) {
		cc20_block(chacha20_state, tmp);
		for (i = 0; i < LC_CC20_KEY_SIZE_WORDS; i++)
			chacha20_state->key.u[i] ^= le_bswap32(tmp[i]);
		lc_memset_secure(tmp, 0, sizeof(tmp));
	} else {
		for (i = 0; i < LC_CC20_KEY_SIZE_WORDS; i++) {
			chacha20_state->key.u[i] ^=
				le_bswap32(buf[i + used_words]);
		}
	}

	/* Deterministic increment of nonce as required in RFC 7539 chapter 4 */
	chacha20_state->counter[1]++;
	if (chacha20_state->counter[1] == 0) {
		chacha20_state->counter[2]++;
		if (chacha20_state->counter[2] == 0)
			chacha20_state->counter[3]++;
	}

	/* Leave counter untouched as it is start value is undefined in RFC */
}

/**
 * Seed the ChaCha20 DRNG by injecting the input data into the key part of
 * the ChaCha20 state. If the input data is longer than the ChaCha20 key size,
 * perform a ChaCha20 operation after processing of key size input data.
 * This operation shall spread out the entropy into the ChaCha20 state before
 * new entropy is injected into the key part.
 *
 * The approach taken here is logically similar to a CBC-MAC: The input data
 * is processed chunk-wise. Each chunk is encrypted, the output is XORed with
 * the next chunk of the input and then encrypted again. I.e. the
 * ChaCha20 CBC-MAC of the seed data is injected into the DRNG state.
 */
LC_INTERFACE_FUNCTION(int, lc_cc20_drng_seed,
		      struct lc_chacha20_drng_ctx *cc20_ctx,
		      const uint8_t *inbuf, size_t inbuflen)
{
	struct lc_sym_ctx *sym_ctx;
	struct lc_sym_state *chacha20_state;

	if (!cc20_ctx)
		return -EINVAL;
	sym_ctx = &cc20_ctx->cc20;
	chacha20_state = sym_ctx->sym_state;

	cc20_drng_selftest();
	LC_SELFTEST_COMPLETED(LC_ALG_STATUS_CHACHA20_DRNG);

	while (inbuflen) {
		size_t i, todo = min_size(inbuflen, LC_CC20_KEY_SIZE);

		for (i = 0; i < todo; i++)
			chacha20_state->key.b[i] ^= inbuf[i];

		/* Break potential dependencies between the inbuf key blocks */
		lc_cc20_drng_update(cc20_ctx, NULL, LC_CC20_BLOCK_SIZE_WORDS);
		inbuf += todo;
		inbuflen -= todo;
	}

	return 0;
}

/**
 * Chacha20 DRNG generation of random numbers: the stream output of ChaCha20
 * is the random number. After the completion of the generation of the
 * stream, the entire ChaCha20 state is updated.
 *
 * Note, as the ChaCha20 implements a 32 bit counter, we must ensure
 * that this function is only invoked for at most 2^32 - 1 ChaCha20 blocks
 * before a reseed or an update happens. This is ensured by the variable
 * outbuflen which is a 32 bit integer defining the number of bytes to be
 * generated by the ChaCha20 DRNG. At the end of this function, an update
 * operation is invoked which implies that the 32 bit counter will never be
 * overflown in this implementation.
 */
LC_INTERFACE_FUNCTION(void, lc_cc20_drng_generate,
		      struct lc_chacha20_drng_ctx *cc20_ctx, uint8_t *outbuf,
		      size_t outbuflen)
{
	struct lc_sym_ctx *sym_ctx;
	struct lc_sym_state *chacha20_state;
	uint32_t aligned_buf[(LC_CC20_BLOCK_SIZE / sizeof(uint32_t))];
	size_t used = LC_CC20_BLOCK_SIZE_WORDS;
	int zeroize_buf = 0;

	if (!cc20_ctx)
		return;
	sym_ctx = &cc20_ctx->cc20;
	chacha20_state = sym_ctx->sym_state;

	while (outbuflen >= LC_CC20_BLOCK_SIZE) {
		if ((uintptr_t)outbuf & (sizeof(aligned_buf[0]) - 1)) {
			cc20_block(chacha20_state, aligned_buf);
			memcpy(outbuf, aligned_buf, LC_CC20_BLOCK_SIZE);
			zeroize_buf = 1;
		} else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
			cc20_block(chacha20_state, (uint32_t *)outbuf);
#pragma GCC diagnostic pop
		}

		outbuf += LC_CC20_BLOCK_SIZE;
		outbuflen -= LC_CC20_BLOCK_SIZE;
	}

	if (outbuflen) {
		cc20_block(chacha20_state, aligned_buf);
		memcpy(outbuf, aligned_buf, outbuflen);
		used = ((outbuflen + sizeof(aligned_buf[0]) - 1) /
			sizeof(aligned_buf[0]));
		zeroize_buf = 1;
	}

	lc_cc20_drng_update(cc20_ctx, aligned_buf, used);

	if (zeroize_buf)
		lc_memset_secure(aligned_buf, 0, sizeof(aligned_buf));
}

LC_INTERFACE_FUNCTION(void, lc_cc20_drng_zero_free,
		      struct lc_chacha20_drng_ctx *cc20_ctx)
{
	if (!cc20_ctx)
		return;

	lc_cc20_drng_zero(cc20_ctx);
	lc_free(cc20_ctx);
}

LC_INTERFACE_FUNCTION(int, lc_cc20_drng_alloc,
		      struct lc_chacha20_drng_ctx **cc20_ctx)
{
	struct lc_chacha20_drng_ctx *out_ctx = NULL;
	int ret;

	if (!cc20_ctx)
		return -EINVAL;

	ret = lc_alloc_aligned_secure((void *)&out_ctx,
				      LC_SYM_ALIGNMENT(lc_chacha20),
				      LC_CC20_DRNG_CTX_SIZE);
	if (ret)
		return -ret;

	LC_CC20_DRNG_SET_CTX(out_ctx);
	lc_cc20_drng_zero(out_ctx);

	*cc20_ctx = out_ctx;

	return 0;
}
