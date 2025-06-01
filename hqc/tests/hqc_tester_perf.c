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

#include "hqc_type.h"

#include "compare.h"
#include "cpufeatures.h"
#include "lc_rng.h"
#include "ret_checkers.h"
#include "small_stack_support.h"
#include "visibility.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

struct workspace {
	struct lc_hqc_pk pk;
	struct lc_hqc_sk sk;
	struct lc_hqc_ct ct;
	struct lc_hqc_ss ss, ss2;
};

static int hqc_tester_perf_one(struct workspace *ws)
{
	int ret;

	CKINT_LOG(lc_hqc_keypair(&ws->pk, &ws->sk, lc_seeded_rng),
		  "HQC keypair failed: %d\n", ret);
	CKINT_LOG(lc_hqc_enc(&ws->ct, &ws->ss, &ws->pk),
		  "HQC encapsulation failed: %d\n", ret);
	CKINT_LOG(lc_hqc_dec(&ws->ss2, &ws->ct, &ws->sk),
		  "HQC decapsulation failed: %d\n", ret);

out:
	if (ret == -EOPNOTSUPP)
		ret = 77;
	return ret;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="
LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	unsigned int i;
	int ret = 0;
	LC_DECLARE_MEM(ws, struct workspace, LC_HQC_ALIGN_BYTES);

	(void)argv;

	/* Disable any accelerations when there is one parameter */
	if (argc > 1)
		lc_cpu_feature_disable();

	for (i = 0; i < 1000; i++) {
		ret = hqc_tester_perf_one(ws);
		if (ret)
			break;
	}

	/* Enable any accelerations when there is one parameter */
	if (argc > 1)
		lc_cpu_feature_enable();

	LC_RELEASE_MEM(ws);
	return ret;
}
#pragma GCC diagnostic pop
