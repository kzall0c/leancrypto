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

#ifndef ASCON_HASH_COMMON_H
#define ASCON_HASH_COMMON_H

#include "lc_ascon_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

int ascon_256_init(void *_state);
int ascon_256_init_nocheck(void *_state);
int ascon_128a_init(void *_state);
int ascon_xof_init(void *_state);
int ascon_xof_init_nocheck(void *_state);
int ascon_cxof_init(void *_state);
int ascon_cxof_init_nocheck(void *_state);
size_t ascon_digestsize(void *_state);
void ascon_xof_set_digestsize(void *_state, size_t digestsize);
size_t ascon_xof_get_digestsize(void *_state);

#ifdef __cplusplus
}
#endif

#endif /* ASCON_HASH_COMMON_H */
