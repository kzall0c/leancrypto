/*
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
 * https://github.com/sphincs/sphincsplus
 *
 * That code is released under Public Domain
 * (https://creativecommons.org/share-your-work/public-domain/cc0/).
 */

#include "ret_checkers.h"
#include "small_stack_support.h"
#include "sphincs_address.h"
#include "sphincs_hash.h"
#include "sphincs_thash.h"
#include "sphincs_wots.h"
#include "sphincs_wotsx1.h"
#include "sphincs_utils.h"
#include "sphincs_utilsx1.h"

// TODO clarify address expectations, and make them more uniform.
// TODO i.e. do we expect types to be set already?
// TODO and do we expect modifications or copies?

/**
 * Computes the chaining function.
 * out and in have to be n-byte arrays.
 *
 * Interprets in as start-th value of the chain.
 * addr has to contain the address of the chain.
 */
static int gen_chain(uint8_t *out, const uint8_t *in, unsigned int start,
		     unsigned int steps, const spx_ctx *ctx, uint32_t addr[8])
{
	LC_HASH_CTX_ON_STACK(hash_ctx, LC_SPHINCS_HASH_TYPE);
	uint64_t ascon_state[LC_ASCON_HASH_STATE_WORDS];
	uint32_t i;
	int ret = 0;

	(void)ascon_state;

	/* Initialize out with the value at position 'start'. */
	memcpy(out, in, LC_SPX_N);

	/* Iterate 'steps' calls to the hash function. */
	for (i = start; i < (start + steps) && i < LC_SPX_WOTS_W; i++) {
		/* only last word changes */
		set_hash_addr(addr, i);
#if defined(LC_SPHINCS_TYPE_128F_ASCON) || defined(LC_SPHINCS_TYPE_128S_ASCON)
		CKINT(thash_ascon(
			hash_ctx, out, out, 1, ctx->pub_seed, addr,
			LC_SPX_ADDR_BYTES - LC_ASCON_HASH_RATE,
			(uint8_t *)ascon_state, i == start));
#else
		CKINT(thash(hash_ctx, out, out, 1, ctx->pub_seed, addr));
#endif
	}

out:
#if defined(LC_SPHINCS_TYPE_128F_ASCON) || defined(LC_SPHINCS_TYPE_128S_ASCON)
	lc_memset_secure(ascon_state, 0, sizeof(ascon_state));
#endif
	lc_hash_zero(hash_ctx);
	return ret;
}

/**
 * base_w algorithm as described in draft.
 * Interprets an array of bytes as integers in base w.
 * This only works when log_w is a divisor of 8.
 */
static void base_w(unsigned int *output, const int out_len,
		   const uint8_t *input)
{
	int in = 0;
	int out = 0;
	uint8_t total;
	int bits = 0;
	int consumed;

	for (consumed = 0; consumed < out_len; consumed++) {
		if (bits == 0) {
			total = input[in];
			in++;
			bits += 8;
		}
		bits -= LC_SPX_WOTS_LOGW;
		output[out] = (total >> bits) & (LC_SPX_WOTS_W - 1);
		out++;
	}
}

/* Computes the WOTS+ checksum over a message (in base_w). */
static void wots_checksum(unsigned int *csum_base_w,
			  const unsigned int *msg_base_w)
{
	unsigned int csum = 0;
	uint8_t csum_bytes[(LC_SPX_WOTS_LEN2 * LC_SPX_WOTS_LOGW + 7) / 8];
	unsigned int i;

	/* Compute checksum. */
	for (i = 0; i < LC_SPX_WOTS_LEN1; i++) {
		csum += LC_SPX_WOTS_W - 1 - msg_base_w[i];
	}

	/* Convert checksum to base_w. */
	/* Make sure expected empty zero bits are the least significant bits. */
	csum = csum << ((8 - ((LC_SPX_WOTS_LEN2 * LC_SPX_WOTS_LOGW) % 8)) % 8);
	ull_to_bytes(csum_bytes, sizeof(csum_bytes), csum);
	base_w(csum_base_w, LC_SPX_WOTS_LEN2, csum_bytes);
}

/* Takes a message and derives the matching chain lengths. */
void chain_lengths_c(unsigned int *lengths, const uint8_t *msg)
{
	base_w(lengths, LC_SPX_WOTS_LEN1, msg);
	wots_checksum(lengths + LC_SPX_WOTS_LEN1, lengths);
}

/**
 * Takes a WOTS signature and an n-byte message, computes a WOTS public key.
 *
 * Writes the computed public key to 'pk'.
 */
int wots_pk_from_sig_c(uint8_t pk[LC_SPX_WOTS_BYTES], const uint8_t *sig,
		       const uint8_t *msg, const spx_ctx *ctx, uint32_t addr[8])
{
	struct workspace {
		unsigned int lengths[LC_SPX_WOTS_LEN];
	};
	uint32_t i;
	int ret = 0;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));

	chain_lengths_c(ws->lengths, msg);

	for (i = 0; i < LC_SPX_WOTS_LEN; i++) {
		set_chain_addr(addr, i);
		CKINT(gen_chain(pk + i * LC_SPX_N, sig + i * LC_SPX_N,
				ws->lengths[i],
				LC_SPX_WOTS_W - 1 - ws->lengths[i], ctx, addr));
	}

out:
	LC_RELEASE_MEM(ws);
	return 0;
}
