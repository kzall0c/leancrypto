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

#include "bitshift.h"
#include "cpufeatures.h"
#include "ext_headers_arm.h"
#include "shake_2x_armv8.h"
#include "sphincs_type.h"
#include "sphincs_address.h"
#include "sphincs_hashx2_armv8.h"

/*
 * By using this function directly, we have to guard this code
 * with LC_CPU_FEATURE_ARM_SHA3 as it requires the SHA3 CE extensions.
 *
 * We could also use KeccakF1600_StatePermutex2, but this may use the ARM
 * Neon intrinsics which are not available in the kernel.
 */
#define f1600x2(s) keccak_f1600x2_armce((s), neon_KeccakF_RoundConstants)

/*
 * 2-way parallel version of prf_addr; takes 2x as much input and output
 */
void prf_addrx2(unsigned char *out0, unsigned char *out1, const spx_ctx *ctx,
		const uint32_t addrx2[2 * 8])
{
	enum lc_cpu_features feat = lc_cpu_feature_available();

	/*
	 * As we write and read only a few quadwords, it is more efficient to
	 * build and extract from the fourway SHAKE256 state by hand.
	 */
	union {
		v128 state128[25];
		uint64_t state[50];
	} s = { 0 };

	for (int i = 0; i < LC_SPX_N / 8; i++) {
		uint64_t x = ptr_to_le64(ctx->pub_seed + 8 * i);
		s.state[2 * i] = x;
		s.state[2 * i + 1] = x;
	}
	for (int i = 0; i < 4; i++) {
		s.state[2 * (LC_SPX_N / 8 + i)] =
			(((uint64_t)addrx2[1 + 2 * i]) << 32) |
			(uint64_t)addrx2[2 * i];
		s.state[2 * (LC_SPX_N / 8 + i) + 1] =
			(((uint64_t)addrx2[8 + 1 + 2 * i]) << 32) |
			(uint64_t)addrx2[8 + 2 * i];
	}
	for (int i = 0; i < LC_SPX_N / 8; i++) {
		uint64_t x = ptr_to_le64(ctx->sk_seed + 8 * i);
		s.state[2 * (LC_SPX_N / 8 + i + 4)] = x;
		s.state[2 * (LC_SPX_N / 8 + i + 4) + 1] = x;
	}

	/* SHAKE domain separator and padding. */
	s.state[2 * (LC_SPX_N / 4 + 4)] = 0x1f;
	s.state[2 * (LC_SPX_N / 4 + 4) + 1] = 0x1f;

	s.state[2 * 16] = 0x80ULL << 56;
	s.state[2 * 16 + 1] = 0x80ULL << 56;

	if (feat & LC_CPU_FEATURE_ARM_SHA3) {
		LC_NEON_ENABLE;
		f1600x2(s.state);
		LC_NEON_DISABLE;
	} else {
		KeccakF1600_StatePermutex2(s.state128);
	}

	for (int i = 0; i < LC_SPX_N / 8; i++) {
		le64_to_ptr(out0 + 8 * i, s.state[2 * i]);
		le64_to_ptr(out1 + 8 * i, s.state[2 * i + 1]);
	}
}
