/*
 * Copyright (C) 2020 - 2025, Stephan Mueller <smueller@chronox.de>
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

#include "compare.h"
#include "lc_hmac.h"
#include "lc_sha3.h"
#include "test_helper_common.h"
#include "visibility.h"

#include "sha3_c.h"
#include "sha3_arm_asm.h"
#include "sha3_arm_ce.h"
#include "sha3_arm_neon.h"
#include "sha3_avx2.h"
#include "sha3_avx512.h"

#define LC_EXEC_ONE_TEST(sha3_impl)                                            \
	if (sha3_impl)                                                         \
	ret += _sha3_224_tester(sha3_impl, #sha3_impl)

static int _sha3_224_tester(const struct lc_hash *sha3_224, const char *name)
{
	static const uint8_t msg_224[] = { 0x35, 0x8E, 0x06, 0xBA, 0x03, 0x21,
					   0x83, 0xFC, 0x18, 0x20, 0x58, 0xBD,
					   0xB7, 0xBB, 0x13, 0x40 };
	static const uint8_t key_224[] = { 0xBB, 0x00, 0x95, 0xC4, 0xA4,
					   0xA6, 0x67, 0xD2, 0xE7, 0x43,
					   0x30, 0xE5, 0xD6 };
	static const uint8_t exp_224[] = { 0x16, 0xf7, 0xb2, 0x7e, 0x25, 0x37,
					   0x6c, 0x38, 0xcf, 0xaa, 0x6f, 0xcc,
					   0xe2, 0x85, 0xc5, 0x14, 0x28, 0xdb,
					   0x33, 0xa0, 0xfe, 0x7a, 0xf0, 0xaf,
					   0x53, 0x95, 0xde, 0xa2 };
	uint8_t act[LC_SHA3_512_SIZE_DIGEST];

	printf("hash ctx %s (%s implementation) len %lu\n", name,
	       sha3_224 == lc_sha3_224_c ? "C" : "accelerated",
	       LC_HASH_CTX_SIZE(sha3_224));
	if (lc_hmac(sha3_224, key_224, 13, msg_224, 16, act))
		return 1;
	return lc_compare(act, exp_224, LC_SHA3_224_SIZE_DIGEST,
			  "HMAC SHA3-224");
}

static int sha3_hmac_tester(void)
{
	int ret = 0;

	LC_EXEC_ONE_TEST(lc_sha3_224);
	LC_EXEC_ONE_TEST(lc_sha3_224_c);
	LC_EXEC_ONE_TEST(lc_sha3_224_arm_asm);
	LC_EXEC_ONE_TEST(lc_sha3_224_arm_ce);
	LC_EXEC_ONE_TEST(lc_sha3_224_arm_neon);
	LC_EXEC_ONE_TEST(lc_sha3_224_avx2);
	LC_EXEC_ONE_TEST(lc_sha3_224_avx512);

	return ret;
}

LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	int ret;

	(void)argc;
	(void)argv;

	ret = sha3_hmac_tester();

	ret = test_validate_status(ret, LC_ALG_STATUS_SHA256, 1);
	ret = test_validate_status(ret, LC_ALG_STATUS_HMAC, 1);
#ifndef LC_FIPS140_DEBUG
	ret = test_validate_status(ret, LC_ALG_STATUS_SHA3, 1);
#endif
	ret += test_print_status();

	return ret;
}
