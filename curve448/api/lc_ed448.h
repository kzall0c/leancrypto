/*
 * Copyright (C) 2023 - 2025, Stephan Mueller <smueller@chronox.de>
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

#ifndef LC_ED448_H
#define LC_ED448_H

#include "lc_rng.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LC_ED448_SECRETKEYBYTES (57U)
#define LC_ED448_PUBLICKEYBYTES (57U)
#define LC_ED448_SIGBYTES (LC_ED448_PUBLICKEYBYTES + LC_ED448_SECRETKEYBYTES)

struct lc_ed448_sk {
	uint8_t sk[LC_ED448_SECRETKEYBYTES];
};

struct lc_ed448_pk {
	uint8_t pk[LC_ED448_PUBLICKEYBYTES];
};

struct lc_ed448_sig {
	uint8_t sig[LC_ED448_SIGBYTES];
};

int lc_ed448_keypair(struct lc_ed448_pk *pk, struct lc_ed448_sk *sk,
		     struct lc_rng_ctx *rng_ctx);
int lc_ed448_sign(struct lc_ed448_sig *sig, const uint8_t *msg, size_t mlen,
		  const struct lc_ed448_sk *sk, struct lc_rng_ctx *rng_ctx);
int lc_ed448_verify(const struct lc_ed448_sig *sig, const uint8_t *msg,
		    size_t mlen, const struct lc_ed448_pk *pk);

/* Receive a message pre-hashed with SHA-512 */
int lc_ed448ph_sign(struct lc_ed448_sig *sig, const uint8_t *msg, size_t mlen,
		    const struct lc_ed448_sk *sk, struct lc_rng_ctx *rng_ctx);
int lc_ed448ph_verify(const struct lc_ed448_sig *sig, const uint8_t *msg,
		      size_t mlen, const struct lc_ed448_pk *pk);

#ifdef __cplusplus
}
#endif

#endif /* LC_ED448_H */
