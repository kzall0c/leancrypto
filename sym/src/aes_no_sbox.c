/* AES (s)ide-(c)hannel-(r)esistant implementation
 *
 * Copyright (C) 2024 - 2025, Stephan Mueller <smueller@chronox.de>
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
 * https://github.com/peterferrie/aes
 *
 *   Copyright © 2015 Odzhan, Peter Ferrie. All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  3. The name of the author may not be used to endorse or promote products
 *  derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY AUTHORS "AS IS" AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#define AES_NO_SBOX

#include "aes_internal.h"
#include "aes_scr_helper.h"
#include "alignment.h"
#include "bitshift.h"
#include "ext_headers_internal.h"
#include "lc_aes.h"
#include "rotate.h"
#include "timecop.h"
#include "visibility.h"
#include "xor.h"

/*
 * Substitute 16 bytes
 */
static void aes_sub_bytes(state_t *state)
{
	unsigned int i;

	for (i = 0; i < 16; i++)
		state->b[i] = aes_sub_byte(state->b[i]);
}

static void aes_sub_bytes_inv(state_t *state)
{
	unsigned int i;

	for (i = 0; i < 16; i++)
		state->b[i] = aes_sub_byte_inv(state->b[i]);
}

static void aes_shift_rows(state_t *state)
{
	uint32_t x, j;
	uint8_t i;

	// shift 4 rows
	for (i = 0; i < 4; i++) {
		x = 0;

		// get row
		for (j = i; j < 16; j += 4) {
			x |= state->b[j];
			x = ror32(x, 8);
		}

		// rotate depending on enc
		x = ror32(x, i * 8);

		// set row
		for (j = i; j < 16; j += 4) {
			state->b[j] = (x & 0xff);
			x >>= 8;
		}
	}
}

static void aes_shift_rows_inv(state_t *state)
{
	uint32_t x, j;
	uint8_t i;

	// shift 4 rows
	for (i = 0; i < 4; i++) {
		x = 0;

		// get row
		for (j = i; j < 16; j += 4) {
			x |= state->b[j];
			x = ror32(x, 8);
		}

		// rotate depending on enc
		x = rol32(x, i * 8);

		// set row
		for (j = i; j < 16; j += 4) {
			state->b[j] = (x & 0xff);
			x >>= 8;
		}
	}
}

static uint32_t aes_mix_one_column(uint32_t w)
{
	return ror32(w, 8) ^ ror32(w, 16) ^ ror32(w, 24) ^
	       gf_mul2(ror32(w, 8) ^ w);
}

static void aes_mix_columns(state_t *state)
{
	state->w[0] = aes_mix_one_column(state->w[0]);
	state->w[1] = aes_mix_one_column(state->w[1]);
	state->w[2] = aes_mix_one_column(state->w[2]);
	state->w[3] = aes_mix_one_column(state->w[3]);
}

static void aes_mix_columns_inv(state_t *state)
{
	uint32_t i, t, w;

	for (i = 0; i < 4; i++) {
		w = state->w[i];
		t = ror32(w, 16) ^ w;
		t = gf_mul2(gf_mul2(t));
		w ^= t;
		state->w[i] = aes_mix_one_column(w);
	}
}

static void aes_add_round_key(state_t *state, uint32_t w[], int rnd)
{
	xor_64(state->b, (uint8_t *)&w[rnd * 4], AES_BLOCKLEN);

	/*
	 * Timecop: Once the round key is XORed with the state (originating
	 * from the plaintext or ciphertext and perhaps already partially
	 * processed by previous rounds), any lookup using this state data
	 * in the SBox is not considered to reveal a timing channel to
	 * resolve the key / round key. Thus, let us unmark this memory.
	 */
	unpoison(state, sizeof(state_t));
}

static void aes_setkey(struct aes_block_ctx *ctx, const uint8_t *key)
{
	unsigned int i;
	uint32_t x;
	uint32_t *w = (uint32_t *)ctx->round_key;
	uint32_t rcon = 1;

	if (aligned(key, sizeof(uint32_t) - 1)) {
		for (i = 0; i < ctx->nk; i++) {
			/*
			 * We can ignore the alignment warning as we checked
			 * for proper alignment.
			 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
			w[i] = ((uint32_t *)key)[i];
#pragma GCC diagnostic pop
		}
	} else {
		for (i = 0; i < ctx->nk; i++) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			w[i] = ptr_to_le32(key + (i * sizeof(uint32_t)));
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			w[i] = ptr_to_be32(key + (i * sizeof(uint32_t)));
#else
#error "Endianess not defined"
#endif
		}
	}

	for (i = ctx->nk; i < (unsigned int)(Nb * (ctx->nr + 1)); i++) {
		x = w[i - 1];

		/* Shut up clang-scan */
		if (!ctx->nk)
			break;

		if ((i % ctx->nk) == 0) {
			x = ror32(x, 8);
			x = aes_sub_word(x) ^ rcon;
			rcon = gf_mul2(rcon);
		} else if ((ctx->nk > 6) && ((i % ctx->nk) == 4)) {
			x = aes_sub_word(x);
		}
		w[i] = w[i - ctx->nk] ^ x;
	}
}
void aes_key_expansion_scr(struct aes_block_ctx *block_ctx, const uint8_t *Key)
{
	aes_setkey(block_ctx, Key);
}

/*
 * Cipher is the main function that encrypts the PlainText.
 */
void aes_cipher_scr(state_t *state, const struct aes_block_ctx *block_ctx)
{
	uint8_t round;
	uint32_t *w = (uint32_t *)block_ctx->round_key;

	aes_add_round_key(state, w, 0);

	for (round = 1; round < block_ctx->nr; round++) {
		aes_sub_bytes(state);
		aes_shift_rows(state);
		aes_mix_columns(state);
		aes_add_round_key(state, w, round);
	}

	/* Last round */
	aes_sub_bytes(state);
	aes_shift_rows(state);
	aes_add_round_key(state, w, round);
}

void aes_inv_cipher_scr(state_t *state, const struct aes_block_ctx *block_ctx)
{
	uint8_t round;
	uint32_t *w = (uint32_t *)block_ctx->round_key;

	aes_add_round_key(state, w, block_ctx->nr);

	for (round = block_ctx->nr - 1; round > 0; round--) {
		aes_sub_bytes_inv(state);
		aes_shift_rows_inv(state);
		aes_add_round_key(state, w, round);
		aes_mix_columns_inv(state);
	}

	/* Last round */
	aes_sub_bytes_inv(state);
	aes_shift_rows_inv(state);
	aes_add_round_key(state, w, round);
}
