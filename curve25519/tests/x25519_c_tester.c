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

#include "ext_headers_internal.h"

#include "lc_x25519.h"
#include "../src/x25519_scalarmult.h"
#include "../src/x25519_scalarmult_c.h"
#include "compare.h"
#include "ret_checkers.h"
#include "selftest_rng.h"
#include "visibility.h"

static int x25519_scalarmult_base_tester(void)
{
	/*
	 * Test vector is
	 * from https://github.com/jedisct1/libsodium.git/test/default/ed25519_convert.exp
	 */
	static const uint8_t sk[] = { 0x80, 0x52, 0x03, 0x03, 0x76, 0xd4, 0x71,
				      0x12, 0xbe, 0x7f, 0x73, 0xed, 0x7a, 0x01,
				      0x92, 0x93, 0xdd, 0x12, 0xad, 0x91, 0x0b,
				      0x65, 0x44, 0x55, 0x79, 0x8b, 0x46, 0x67,
				      0xd7, 0x3d, 0xe1, 0x66 };
	static const uint8_t pk_exp[] = { 0xf1, 0x81, 0x4f, 0x0e, 0x8f, 0xf1,
					  0x04, 0x3d, 0x8a, 0x44, 0xd2, 0x5b,
					  0xab, 0xff, 0x3c, 0xed, 0xca, 0xe6,
					  0xc2, 0x2c, 0x3e, 0xda, 0xa4, 0x8f,
					  0x85, 0x7a, 0xe7, 0x0d, 0xe2, 0xba,
					  0xae, 0x50 };
	uint8_t pk[sizeof(pk_exp)];
	int ret;

	if (sizeof(pk_exp) != LC_X25519_PUBLICKEYBYTES) {
		printf("X25519 pubkey size mismatch\n");
		return 1;
	}

	if (sizeof(sk) != LC_X25519_SECRETKEYBYTES) {
		printf("X25519 secret size mismatch\n");
		return 1;
	}

	CKINT(crypto_scalarmult_curve25519_base(pk, sk));

	lc_compare(pk, pk_exp, sizeof(pk_exp),
		   "X25519 base scalar multiplication\n");

out:
	return ret;
}

static int x25519_scalarmult_tester(unsigned int loops)
{
	/*
	 * Test vector is
	 * from https://github.com/jedisct1/libsodium.git/test/default/scalarmult7.c
	 * by taking variable p1 and printing out the out1 variable.
	 */
	static unsigned char p1[] = { 0x72, 0x20, 0xf0, 0x09, 0x89, 0x30, 0xa7,
				      0x54, 0x74, 0x8b, 0x7d, 0xdc, 0xb4, 0x3e,
				      0xf7, 0x5a, 0x0d, 0xbf, 0x3a, 0x0d, 0x26,
				      0x38, 0x1a, 0xf4, 0xeb, 0xa4, 0xa9, 0x8e,
				      0xaa, 0x9b, 0x4e, 0xea };
	static const uint8_t exp[] = { 0x03, 0xad, 0x40, 0x80, 0xc2, 0x91, 0x0b,
				       0x5e, 0x0b, 0xe2, 0x2f, 0x6c, 0x5f, 0x7c,
				       0x7e, 0x08, 0xe6, 0x42, 0x46, 0x2e, 0xf0,
				       0xec, 0x93, 0xa6, 0x54, 0xc5, 0xc3, 0x4d,
				       0xc9, 0x5b, 0x55, 0x6d };
	static const uint8_t scalar[] = {
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	uint8_t out[sizeof(exp)];
	unsigned int i;
	int ret;

	for (i = 0; i < loops; i++)
		CKINT(crypto_scalarmult_curve25519_c(out, scalar, p1));
	lc_compare(out, exp, sizeof(exp), "X25519 scalar multiplication\n");

out:
	return ret;
}

static int x25519_keygen(void)
{
	static const uint8_t pk_exp[] = { 0x20, 0x48, 0xc3, 0xf7, 0x98, 0xba,
					  0xd4, 0x19, 0x2c, 0x2e, 0x86, 0x43,
					  0xb4, 0xc8, 0x13, 0x70, 0x7b, 0x65,
					  0x5f, 0xe6, 0x53, 0x4f, 0xb6, 0x75,
					  0x68, 0x96, 0x49, 0xbe, 0x30, 0xc6,
					  0x5d, 0x54 };
	static const uint8_t sk_exp[] = { 0x7f, 0x9c, 0x2b, 0xa4, 0xe8, 0x8f,
					  0x82, 0x7d, 0x61, 0x60, 0x45, 0x50,
					  0x76, 0x05, 0x85, 0x3e, 0xd7, 0x3b,
					  0x80, 0x93, 0xf6, 0xef, 0xbc, 0x88,
					  0xeb, 0x1a, 0x6e, 0xac, 0xfa, 0x66,
					  0xef, 0x26 };
	struct lc_x25519_pk pk;
	struct lc_x25519_sk sk;
	int ret;
	LC_SELFTEST_DRNG_CTX_ON_STACK(selftest_rng);

	CKINT(lc_x25519_keypair(&pk, &sk, selftest_rng));

	lc_compare(pk.pk, pk_exp, sizeof(pk_exp), "X25519 pk keygen\n");
	lc_compare(sk.sk, sk_exp, sizeof(sk_exp), "X25519 sk keygen\n");

out:
	return ret;
}

LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	int ret = 0;

	(void)argv;
	(void)argc;

	if (argc != 2) {
		ret += x25519_scalarmult_base_tester();
		ret += x25519_scalarmult_tester(1);
		ret += x25519_keygen();
	} else {
		ret += x25519_scalarmult_tester(100000);
	}

	return ret;
}
