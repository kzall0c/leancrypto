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

#ifndef SPHINCS_SELFTEST_H
#define SPHINCS_SELFTEST_H

#include "sphincs_type.h"

#ifdef __cplusplus
extern "C" {
#endif

void sphincs_selftest_keygen(void);
void sphincs_selftest_siggen(void);
void sphincs_selftest_sigver(void);

int lc_sphincs_keypair_nocheck(struct lc_sphincs_pk *pk,
			       struct lc_sphincs_sk *sk,
			       struct lc_rng_ctx *rng_ctx);
int lc_sphincs_sign_ctx_nocheck(struct lc_sphincs_sig *sig,
				struct lc_sphincs_ctx *ctx, const uint8_t *m,
				size_t mlen, const struct lc_sphincs_sk *sk,
				struct lc_rng_ctx *rng_ctx);
int lc_sphincs_verify_ctx_nocheck(const struct lc_sphincs_sig *sig,
				  struct lc_sphincs_ctx *ctx, const uint8_t *m,
				  size_t mlen, const struct lc_sphincs_pk *pk);

#ifdef __cplusplus
}
#endif

#endif /* SPHINCS_SELFTEST_H */
