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
 * https://github.com/awslabs/bike-kem
 *
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0"
 *
 * Written by Nir Drucker, Shay Gueron and Dusan Kostic,
 * AWS Cryptographic Algorithms Group.
 */

#include "bike_utilities.h"
#include "bike_sampling_internal.h"
#include "build_bug_on.h"
#include "ext_headers_internal.h"
#include "small_stack_support.h"

#define MAX_WLIST_SIZE (LC_BIKE_MAX_RAND_INDICES_T)

int secure_set_bits_port(pad_r_t *r, const uint32_t first_pos,
			 const idx_t *wlist, const uint32_t w_size)
{
	struct workspace {
		// The size of wlist can be either D or T. So, we set it to
		// max(D, T)
		uint64_t pos_bit[MAX_WLIST_SIZE];
		uint32_t pos_qw[MAX_WLIST_SIZE];
	};
	uint64_t *a64 = (uint64_t *)r;
	uint64_t val, mask;
	uint32_t i;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));

	assert(w_size <= MAX_WLIST_SIZE);

	// Ideally we would like to cast r.val but it is not guaranteed to be aligned
	// as the entire pad_r_t structure. Thus, we assert that the position of val
	// is at the beginning of r.
	BUILD_BUG_ON(offsetof(pad_r_t, val) != 0);

	// Identify the QW position of every value, and the bit position inside this QW.
	for (i = 0; i < w_size; i++) {
		uint32_t w = wlist[i] - first_pos;

		ws->pos_qw[i] = (w >> 6);
		ws->pos_bit[i] = LC_BIKE_BIT(w & LC_BIKE_MASK(6));
	}

	// Fill each QW in constant time
	for (i = 0; i < (sizeof(*r) / sizeof(uint64_t)); i++) {
		size_t j;

		val = 0;
		for (j = 0; j < w_size; j++) {
			mask = (-1ULL) + (!secure_cmp32(ws->pos_qw[j], i));
			val |= (ws->pos_bit[j] & mask);
		}
		a64[i] = val;
	}

	LC_RELEASE_MEM(ws);
	return 0;
}

#if defined(UNIFORM_SAMPLING)
void sample_error_vec_indices_port(idx_t *out, struct lc_hash_ctx *prf_state)
{
	// To generate T indices in constant-time, i.e. without rejection sampling,
	// we generate MAX_RAND_INDICES_T random values with the appropriate bit
	// length (the bit size of N) and in constant time copy the first T valid
	// indices to the output.

	uint32_t ctr = 0; // Current number of valid and distinct indices.
	const idx_t bit_mask =
		LC_BIKE_MASK(bit_scan_reverse_vartime(2 * LC_BIKE_R_BITS));

	lc_hash_set_digestsize(prf_state, sizeof(idx_t));

	// Label all output elements as invalid.
	memset((uint8_t *)out, 0xff, LC_BIKE_T * sizeof(idx_t));

	// Generate MAX_RAND_INDICES_T random values.
	for (size_t i = 0; i < LC_BIKE_MAX_RAND_INDICES_T; i++) {
		// Generate random index with the required bit length.
		uint32_t idx;

		lc_hash_final(prf_state, (uint8_t *)&idx);
		idx &= bit_mask;

		// Loop over the output array to determine if |idx| is a duplicate,
		// and store it in the lcoation pointed to by |ctr|.
		uint32_t is_dup = 0;
		for (uint32_t j = 0; j < LC_BIKE_T; j++) {
			is_dup |= secure_cmp32(idx, out[j]);

			// Set |mask| to 0 if |ctr| != |j|, to all ones otherwise.
			uint32_t mask = -secure_cmp32(j, ctr);
			// Write |idx| to out if |ctr| == |j|.
			out[j] = (~mask & out[j]) | (mask & idx);
		}

		// Check if |idx| is a valid index (< N) and increase the counter
		// only if |idx| is valid and it is not a duplicate.
		uint32_t is_valid = secure_l32(idx, 2 * LC_BIKE_R_BITS);
		ctr += ((1 - is_dup) & is_valid);
	}
}
#endif
