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

#include "compare.h"
#include "cpufeatures.h"
#include "small_stack_support.h"
#include "sphincs_type.h"
#include "static_rng.h"
#include "ret_checkers.h"
#include "test_helper_common.h"
#include "visibility.h"

#ifdef LC_SPHINCS_TYPE_128F
#include "lc_sphincs_shake_128f.h"
#include "sphincs_tester_vectors_shake_128f.h"
#elif defined(LC_SPHINCS_TYPE_128F_ASCON)
#include "lc_sphincs_ascon_128f.h"
#include "sphincs_tester_vectors_ascon_128f.h"
#elif defined(LC_SPHINCS_TYPE_128S)
#include "lc_sphincs_shake_128s.h"
#include "sphincs_tester_vectors_shake_128s.h"
#elif defined(LC_SPHINCS_TYPE_128S_ASCON)
#include "lc_sphincs_ascon_128s.h"
#include "sphincs_tester_vectors_ascon_128s.h"
#elif defined(LC_SPHINCS_TYPE_192F)
#include "lc_sphincs_shake_192f.h"
#include "sphincs_tester_vectors_shake_192f.h"
#elif defined(LC_SPHINCS_TYPE_192S)
#include "lc_sphincs_shake_192s.h"
#include "sphincs_tester_vectors_shake_192s.h"
#elif defined(LC_SPHINCS_TYPE_256F)
#include "lc_sphincs_shake_256f.h"
#include "sphincs_tester_vectors_shake_256f.h"
#else
#include "lc_sphincs_shake_256s.h"
#include "sphincs_tester_vectors_shake_256s.h"
#endif

enum lc_sphincs_test_type {
	LC_SPHINCS_REGRESSION,
	LC_SPHINCS_PERF_KEYGEN,
	LC_SPHINCS_PERF_SIGN,
	LC_SPHINCS_PERF_VERIFY,
};

static int lc_sphincs_test(const struct lc_sphincs_test *tc,
			   enum lc_sphincs_test_type t)
{
	struct workspace {
		struct lc_sphincs_pk pk;
		struct lc_sphincs_sk sk;
		struct lc_sphincs_sig sig;
	};
	unsigned int rounds, i;
	int ret = 0;
	LC_SPHINCS_CTX_ON_STACK(ctx);
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));

	if (t == LC_SPHINCS_REGRESSION || t == LC_SPHINCS_PERF_KEYGEN) {
		struct lc_static_rng_data s_rng_state;
		LC_STATIC_DRNG_ON_STACK(s_drng, &s_rng_state);

		rounds = (t == LC_SPHINCS_PERF_KEYGEN) ? 100 : 1;

		for (i = 0; i < rounds; i++) {
			/*
			 * Set the seed that the key generation can pull via the
			 * RNG.
			 */
			s_rng_state.seed = tc->seed;
			s_rng_state.seedlen = sizeof(tc->seed);
			ret |= lc_sphincs_keypair(&ws->pk, &ws->sk, &s_drng);
		}
		lc_compare((uint8_t *)&ws->pk, tc->pk, sizeof(tc->pk), "PK");
		lc_compare((uint8_t *)&ws->sk, tc->sk, sizeof(tc->sk), "SK");
	}

	if (t == LC_SPHINCS_REGRESSION || t == LC_SPHINCS_PERF_SIGN) {
		rounds = (t == LC_SPHINCS_PERF_SIGN) ? 10 : 1;

		for (i = 0; i < rounds; i++) {
			ret |= lc_sphincs_sign_ctx(
				&ws->sig, ctx, tc->msg, sizeof(tc->msg),
				(struct lc_sphincs_sk *)tc->sk, NULL);
		}
		lc_compare((uint8_t *)&ws->sig, tc->sig, sizeof(tc->sig),
			   "SIG");
	}

	if (t == LC_SPHINCS_REGRESSION || t == LC_SPHINCS_PERF_VERIFY) {
		rounds = (t == LC_SPHINCS_PERF_VERIFY) ? 1000 : 1;

		for (i = 0; i < rounds; i++) {
			ret |= lc_sphincs_verify_ctx(
				(struct lc_sphincs_sig *)tc->sig, ctx, tc->msg,
				sizeof(tc->msg),
				(struct lc_sphincs_pk *)tc->pk);
		}
	}

	LC_RELEASE_MEM(ws);
	return !!ret;
}

LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	enum lc_sphincs_test_type t = LC_SPHINCS_REGRESSION;
	int ret = 0;
	int feat_disabled = 0;

#ifdef LC_FIPS140_DEBUG
	/*
	 * Both algos are used for the random number generation as part of
	 * the key generation. Thus we need to enable them for executing the
	 * test.
	 */
	alg_status_set_result(lc_alg_status_result_passed, LC_ALG_STATUS_SHAKE);
	alg_status_set_result(lc_alg_status_result_passed, LC_ALG_STATUS_SHA3);
#endif

	if (argc >= 2) {
		if (argv[1][0] == 'k')
			t = LC_SPHINCS_PERF_KEYGEN;
		if (argv[1][0] == 's')
			t = LC_SPHINCS_PERF_SIGN;
		if (argv[1][0] == 'v')
			t = LC_SPHINCS_PERF_VERIFY;
		if (argv[1][0] == 'c') {
			lc_cpu_feature_disable();
			feat_disabled = 1;
		}
	}

	if (argc >= 3) {
		if (argv[2][0] == 'c') {
			lc_cpu_feature_disable();
			feat_disabled = 1;
		}
	}

#ifdef LC_SPHINCS_TESTER_C
	lc_cpu_feature_disable();
	feat_disabled = 1;
#endif

	ret = lc_sphincs_test(&tests[0], t);

	if (argc < 2) {
		ret = test_validate_status(ret, LC_ALG_STATUS_SLHDSA_KEYGEN, 1);
		ret = test_validate_status(ret, LC_ALG_STATUS_SLHDSA_SIGGEN, 1);
		ret = test_validate_status(ret, LC_ALG_STATUS_SLHDSA_SIGVER, 1);

#if (defined(LC_SPHINCS_TYPE_128F_ASCON) || defined(LC_SPHINCS_TYPE_128S_ASCON))
		ret = test_validate_status(ret, LC_ALG_STATUS_ASCONXOF, 1);
#else
#ifndef LC_FIPS140_DEBUG
		ret = test_validate_status(ret, LC_ALG_STATUS_SHAKE, 1);
#endif
#endif
	}

	ret += test_print_status();

	if (feat_disabled)
		lc_cpu_feature_enable();
	return ret;
}
