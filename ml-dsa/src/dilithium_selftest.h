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

#ifndef DILITHIUM_SELFTEST_H
#define DILITHIUM_SELFTEST_H

#include "dilithium_type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dilithium_testvector {
	uint8_t m[64];
	struct lc_dilithium_pk pk;
	struct lc_dilithium_sk sk;
	struct lc_dilithium_sig sig;
};

void dilithium_keypair_tester(int (*_lc_dilithium_keypair_from_seed)(
	struct lc_dilithium_pk *pk, struct lc_dilithium_sk *sk,
	const uint8_t *seed, size_t seedlen));

void dilithium_siggen_tester(int (*_lc_dilithium_sign)(
	struct lc_dilithium_sig *sig, struct lc_dilithium_ctx *ctx,
	const uint8_t *m, size_t mlen, const struct lc_dilithium_sk *sk,
	struct lc_rng_ctx *rng_ctx));

void dilithium_sigver_tester(int (*_lc_dilithium_verify)(
	const struct lc_dilithium_sig *sig, struct lc_dilithium_ctx *ctx,
	const uint8_t *m, size_t mlen, const struct lc_dilithium_pk *pk));

#ifdef __cplusplus
}
#endif

#endif /* DILITHIUM_SELFTEST_H */
