/*
 * Copyright (C) 2025, Stephan Mueller <smueller@chronox.de>
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
 * https://github.com/PQClean/PQClean/
 *
 * The code is referenced as Public Domain
 */
/**
 * @file hqc.h
 * @brief Functions of the HQC_PKE IND_CPA scheme
 */

#ifndef HQC_H
#define HQC_H

#include "hqc_type.h"
#include "lc_rng.h"

#ifdef __cplusplus
extern "C" {
#endif

int hqc_pke_keygen(struct lc_hqc_pk *pk, struct lc_hqc_sk *sk,
		   struct lc_rng_ctx *rng_ctx);

void hqc_pke_encrypt(uint64_t *u, uint64_t *v, uint8_t *m, uint8_t *theta,
		     const uint8_t *pk, struct hqc_pke_encrypt_ws *ws);

uint8_t hqc_pke_decrypt(uint8_t *m, uint8_t *sigma, const uint64_t *u,
			const uint64_t *v, const uint8_t *sk,
			struct hqc_pke_decrypt_ws *ws);

#ifdef __cplusplus
}
#endif

#endif /* HQC_H */
