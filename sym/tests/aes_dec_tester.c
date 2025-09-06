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
 * https://github.com/kokke/tiny-AES-c
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "aes_aesni.h"
#include "aes_armce.h"
#include "aes_c.h"
#include "aes_riscv64.h"
#include "aes_internal.h"
#include "lc_aes.h"
#include "compare.h"
#include "ret_checkers.h"
#include "timecop.h"
#include "visibility.h"

#define LC_EXEC_ONE_TEST(aes_impl)                                             \
	if (aes_impl)                                                          \
		ret += test_decrypt(aes_impl, #aes_impl);

static const uint8_t key256[] = { 0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71,
				  0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d,
				  0x77, 0x81, 0x1f, 0x35, 0x2c, 0x07, 0x3b,
				  0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3,
				  0x09, 0x14, 0xdf, 0xf4 };
static const uint8_t in256[] = {
	0xf3, 0xee, 0xd1, 0xbd, 0xb5, 0xd2, 0xa0, 0x3c,
	0x06, 0x4b, 0x5a, 0x7e, 0x3d, 0xb1, 0x81, 0xf8
};
static const uint8_t key192[] = { 0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e,
				  0x64, 0x52, 0xc8, 0x10, 0xf3, 0x2b,
				  0x80, 0x90, 0x79, 0xe5, 0x62, 0xf8,
				  0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b };
static const uint8_t in192[] = {
	0xbd, 0x33, 0x4f, 0x1d, 0x6e, 0x45, 0xf2, 0x5f,
	0xf7, 0x12, 0xa2, 0x14, 0x57, 0x1f, 0xa5, 0xcc
};
static const uint8_t key128[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae,
				  0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88,
				  0x09, 0xcf, 0x4f, 0x3c };
static const uint8_t in128[] = {
	0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60,
	0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97
};

static int test_decrypt_one(struct lc_sym_ctx *ctx, const uint8_t *key,
			    size_t keylen, uint8_t *in)
{
	static const uint8_t out[] = { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40,
				       0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11,
				       0x73, 0x93, 0x17, 0x2a };
	int ret;

	/* Unpoison key to let implementation poison it */
	unpoison(key, keylen);

	CKINT(lc_sym_init(ctx));
	CKINT(lc_sym_setkey(ctx, key, keylen));
	lc_sym_decrypt(ctx, in, in, sizeof(out));
	ret = lc_compare(in, out, sizeof(out), "AES block decrypt");

out:
	return ret;
}

static int test_decrypt(const struct lc_sym *aes_impl, const char *name)
{
	struct lc_sym_ctx *aes_heap;
	uint8_t in2[sizeof(in256)];
	int ret;
	LC_SYM_CTX_ON_STACK(aes, aes_impl);

	printf("AES block ctx %s (%s implementation) len %u\n", name,
	       aes_impl == lc_aes_c ? "C" : "accelerated",
	       (unsigned int)LC_SYM_CTX_SIZE(aes_impl));

	memcpy(in2, in256, sizeof(in256));
	ret = test_decrypt_one(aes, key256, sizeof(key256), in2);
	lc_sym_zero(aes);

	memcpy(in2, in192, sizeof(in192));
	ret += test_decrypt_one(aes, key192, sizeof(key192), in2);
	lc_sym_zero(aes);

	memcpy(in2, in128, sizeof(in128));
	ret += test_decrypt_one(aes, key128, sizeof(key128), in2);
	lc_sym_zero(aes);

	if (lc_sym_alloc(lc_aes_c, &aes_heap))
		return ret + 1;
	memcpy(in2, in256, sizeof(in256));
	ret += test_decrypt_one(aes_heap, key256, sizeof(key256), in2);
	lc_sym_zero_free(aes_heap);

	return ret;
}

LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	int ret = 0;

	(void)argc;
	(void)argv;

	LC_EXEC_ONE_TEST(lc_aes_aesni);
	LC_EXEC_ONE_TEST(lc_aes_armce);
	LC_EXEC_ONE_TEST(lc_aes_c);
	LC_EXEC_ONE_TEST(lc_aes_riscv64);

	return ret;
}
