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
/*
 * This code is derived in parts from the code distribution provided with
 * https://github.com/pq-crystals/kyber
 *
 * That code is released under Public Domain
 * (https://creativecommons.org/share-your-work/public-domain/cc0/).
 */

#ifndef SIDECHANNEL_RESISTANCE_H
#define SIDECHANNEL_RESISTANCE_H

#include "ext_headers_internal.h"
#include "null_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief cmov - Copy len bytes from x to r if b is 1;
 *		 don't modify x if b is 0. Requires b to be in {0,1};
 *		 assumes two's complement representation of negative integers.
 *		 Runs in constant time.
 *
 * @param [out] r pointer to output byte array
 * @param [in] x pointer to input byte array
 * @param [in] len Amount of bytes to be copied
 * @param [in] b Condition bit; has to be in {0,1}
 */
static inline void cmov(uint8_t *r, const uint8_t *x, size_t len, uint8_t b)
{
	size_t i;
	uint8_t opt_blocker;

	/*
	 * Goal: copy data only depending on a given condition without
	 * the use of a branching operation which alters the timing behavior
	 * depending on the condition. As the condition here depends on
	 * secret data, the code has to ensure that no branching is used to have
	 * time-invariant code. This solution below also shall ensure that the
	 * compiler cannot optimize this code such that it brings back the
	 * branching.
	 *
	 * (condition ^ opt_blocker) can be any value at run-time to the
	 * compiler, making it impossible to skip the computation (except the
	 * compiler would care to create a branch for opt_blocker to be either
	 * 0 or 1, which would be extremely unlikely). Yet the volatile
	 * variable has to be loaded only once at the beginning of the function
	 * call.
	 *
	 * Note, the opt_blocker is not required in most instances, but in the
	 * ARMv8 Neon implementation of SLH-DSA the compiler managed to still
	 * create time-variant code without the optimization blocker.
	 */
	opt_blocker = (uint8_t)optimization_blocker_int8;

	b = -b;
	for (i = 0; i < len; i++)
		r[i] ^= (b & (r[i] ^ x[i])) ^ opt_blocker;
}

/**
 * @brief cmov_int16 - Copy input v to *r if b is 1, don't modify *r if b is 0.
 *		       Requires b to be in {0,1}; Runs in constant time.
 *
 * @param [out] r pointer to output int16_t
 * @param [in] v input int16_t
 * @param [in] b Condition bit; has to be in {0,1}
 */
static inline void cmov_int16(int16_t *r, int16_t v, uint16_t b)
{
	b = -b;
	*r ^= (int16_t)(b & ((*r) ^ v));
}

/**
 * @brief cmov_uint32 - Copy input v to *r if b is 1, don't modify *r if b is 0.
 *			Requires b to be in {0,1}; Runs in constant time.
 *
 * @param [out] r pointer to output int16_t
 * @param [in] v input int16_t
 * @param [in] b Condition bit; has to be in {0,1}
 */
static inline void cmov_uint32(uint32_t *r, uint32_t v, uint32_t b)
{
	b = -b;
	*r ^= (uint32_t)(b & ((*r) ^ v));
}

/**
 * @brief cmov_int - Copy input v to *r if b is 1, don't modify *r if b is 0.
 *		       Requires b to be in {0,1}; Runs in constant time.
 *
 * @param [out] r pointer to output int16_t
 * @param [in] v input int16_t
 * @param [in] b Condition bit; has to be in {0,1}
 */
static inline void cmov_int(int *r, int v, uint16_t b)
{
	b = -b;
	*r ^= (int)(b & ((*r) ^ v));
}
#ifdef __cplusplus
}
#endif

#endif /* SIDECHANNEL_RESISTANCE_H */
