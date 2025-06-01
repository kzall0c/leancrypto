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
 * @file fft.c
 * @brief Implementation of the additive FFT and its transpose.
 * This implementation is based on the paper from Gao and Mateer: <br>
 * Shuhong Gao and Todd Mateer, Additive Fast Fourier Transforms over Finite
 * Fields, IEEE Transactions on Information Theory 56 (2010), 6265--6272.
 * http://www.math.clemson.edu/~sgao/papers/GM10.pdf <br>
 * and includes improvements proposed by Bernstein, Chou and Schwabe here:
 * https://binary.cr.yp.to/mcbits-20130616.pdf
 */

#include "fft_avx2.h"
#include "gf_avx2.h"

static void radix_big(uint16_t *f0, uint16_t *f1, const uint16_t *f,
		      uint32_t m_f);

/**
 * @brief Computes the basis of betas (omitting 1) used in the additive FFT and
 * its transpose
 *
 * @param[out] betas Array of size PARAM_M-1
 */
static void compute_fft_betas(uint16_t *betas)
{
	size_t i;

	for (i = 0; i < LC_HQC_PARAM_M - 1; ++i)
		betas[i] = (uint16_t)(1 << (LC_HQC_PARAM_M - 1 - i));
}

/**
 * @brief Computes the subset sums of the given set
 *
 * The array subset_sums is such that its ith element is
 * the subset sum of the set elements given by the binary form of i.
 *
 * @param[out] subset_sums Array of size 2^set_size receiving the subset sums
 * @param[in] set Array of set_size elements
 * @param[in] set_size Size of the array set
 */
static void compute_subset_sums(uint16_t *subset_sums, const uint16_t *set,
				uint16_t set_size)
{
	uint16_t i, j;
	subset_sums[0] = 0;

	for (i = 0; i < set_size; ++i) {
		for (j = 0; j < (1 << i); ++j)
			subset_sums[(1 << i) + j] = set[i] ^ subset_sums[j];
	}
}

/**
 * @brief Computes the radix conversion of a polynomial f in GF(2^m)[x]
 *
 * Computes f0 and f1 such that f(x) = f0(x^2-x) + x.f1(x^2-x)
 * as proposed by Bernstein, Chou and Schwabe:
 * https://binary.cr.yp.to/mcbits-20130616.pdf
 *
 * @param[out] f0 Array half the size of f
 * @param[out] f1 Array half the size of f
 * @param[in] f Array of size a power of 2
 * @param[in] m_f 2^{m_f} is the smallest power of 2 greater or equal to the
 *		  number of coefficients of f
 */
static void radix(uint16_t *f0, uint16_t *f1, const uint16_t *f, uint32_t m_f)
{
	switch (m_f) {
	case 4:
		f0[4] = f[8] ^ f[12];
		f0[6] = f[12] ^ f[14];
		f0[7] = f[14] ^ f[15];
		f1[5] = f[11] ^ f[13];
		f1[6] = f[13] ^ f[14];
		f1[7] = f[15];
		f0[5] = f[10] ^ f[12] ^ f1[5];
		f1[4] = f[9] ^ f[13] ^ f0[5];

		f0[0] = f[0];
		f1[3] = f[7] ^ f[11] ^ f[15];
		f0[3] = f[6] ^ f[10] ^ f[14] ^ f1[3];
		f0[2] = f[4] ^ f0[4] ^ f0[3] ^ f1[3];
		f1[1] = f[3] ^ f[5] ^ f[9] ^ f[13] ^ f1[3];
		f1[2] = f[3] ^ f1[1] ^ f0[3];
		f0[1] = f[2] ^ f0[2] ^ f1[1];
		f1[0] = f[1] ^ f0[1];
		break;

	case 3:
		f0[0] = f[0];
		f0[2] = f[4] ^ f[6];
		f0[3] = f[6] ^ f[7];
		f1[1] = f[3] ^ f[5] ^ f[7];
		f1[2] = f[5] ^ f[6];
		f1[3] = f[7];
		f0[1] = f[2] ^ f0[2] ^ f1[1];
		f1[0] = f[1] ^ f0[1];
		break;

	case 2:
		f0[0] = f[0];
		f0[1] = f[2] ^ f[3];
		f1[0] = f[1] ^ f0[1];
		f1[1] = f[3];
		break;

	case 1:
		f0[0] = f[0];
		f1[0] = f[1];
		break;

	default:
		radix_big(f0, f1, f, m_f);
		break;
	}
}

static void radix_big(uint16_t *f0, uint16_t *f1, const uint16_t *f,
		      uint32_t m_f)
{
	uint16_t Q[2 * (1 << (LC_HQC_PARAM_FFT - 2)) + 1] = { 0 };
	uint16_t R[2 * (1 << (LC_HQC_PARAM_FFT - 2)) + 1] = { 0 };

	uint16_t Q0[1 << (LC_HQC_PARAM_FFT - 2)] = { 0 };
	uint16_t Q1[1 << (LC_HQC_PARAM_FFT - 2)] = { 0 };
	uint16_t R0[1 << (LC_HQC_PARAM_FFT - 2)] = { 0 };
	uint16_t R1[1 << (LC_HQC_PARAM_FFT - 2)] = { 0 };

	size_t i, n;

	n = 1;
	n <<= (m_f - 2);
	memcpy(Q, f + 3 * n, 2 * n);
	memcpy(Q + n, f + 3 * n, 2 * n);
	memcpy(R, f, 4 * n);

	for (i = 0; i < n; ++i) {
		Q[i] ^= f[2 * n + i];
		R[n + i] ^= Q[i];
	}

	radix(Q0, Q1, Q, m_f - 1);
	radix(R0, R1, R, m_f - 1);

	memcpy(f0, R0, 2 * n);
	memcpy(f0 + n, Q0, 2 * n);
	memcpy(f1, R1, 2 * n);
	memcpy(f1 + n, Q1, 2 * n);
}

/**
 * @brief Evaluates f at all subset sums of a given set
 *
 * This function is a subroutine of the function fft.
 *
 * @param[out] w Array
 * @param[in] f Array
 * @param[in] f_coeffs Number of coefficients of f
 * @param[in] m Number of betas
 * @param[in] m_f Number of coefficients of f (one more than its degree)
 * @param[in] betas FFT constants
 */
static void fft_rec(uint16_t *w, uint16_t *f, size_t f_coeffs, uint8_t m,
		    uint32_t m_f, const uint16_t *betas)
{
	uint16_t f0[1 << (LC_HQC_PARAM_FFT - 2)] = { 0 };
	uint16_t f1[1 << (LC_HQC_PARAM_FFT - 2)] = { 0 };
	uint16_t gammas[LC_HQC_PARAM_M - 2] = { 0 };
	uint16_t deltas[LC_HQC_PARAM_M - 2] = { 0 };
	uint16_t gammas_sums[1 << (LC_HQC_PARAM_M - 2)] = { 0 };
	uint16_t u[1 << (LC_HQC_PARAM_M - 2)] = { 0 };
	uint16_t v[1 << (LC_HQC_PARAM_M - 2)] = { 0 };
	uint16_t tmp[LC_HQC_PARAM_M - (LC_HQC_PARAM_FFT - 1)] = { 0 };

	uint16_t beta_m_pow;
	size_t i, j, k;
	size_t x;

	// Step 1
	if (m_f == 1) {
		for (i = 0; i < m; ++i) {
			tmp[i] = gf_mul_avx2(betas[i], f[1]);
		}

		w[0] = f[0];
		x = 1;
		for (j = 0; j < m; ++j) {
			for (k = 0; k < x; ++k) {
				w[x + k] = w[k] ^ tmp[j];
			}
			x <<= 1;
		}

		return;
	}

	// Step 2: compute g
	if (betas[m - 1] != 1) {
		beta_m_pow = 1;
		x = 1;
		x <<= m_f;
		for (i = 1; i < x; ++i) {
			beta_m_pow = gf_mul_avx2(beta_m_pow, betas[m - 1]);
			f[i] = gf_mul_avx2(beta_m_pow, f[i]);
		}
	}

	// Step 3
	radix(f0, f1, f, m_f);

	// Step 4: compute gammas and deltas
	for (i = 0; i + 1 < m; ++i) {
		gammas[i] =
			gf_mul_avx2(betas[i], gf_inverse_avx2(betas[m - 1]));
		deltas[i] = gf_square_avx2(gammas[i]) ^ gammas[i];
	}

	// Compute gammas sums
	compute_subset_sums(gammas_sums, gammas, m - 1);

	// Step 5
	fft_rec(u, f0, (f_coeffs + 1) / 2, m - 1, m_f - 1, deltas);

	k = 1;
	k <<= ((m - 1) &
	       0xf); // &0xf is to let the compiler know that m-1 is small.
	if (f_coeffs <= 3) { // 3-coefficient polynomial f case: f1 is constant
		w[0] = u[0];
		w[k] = u[0] ^ f1[0];
		for (i = 1; i < k; ++i) {
			w[i] = u[i] ^ gf_mul_avx2(gammas_sums[i], f1[0]);
			w[k + i] = w[i] ^ f1[0];
		}
	} else {
		fft_rec(v, f1, f_coeffs / 2, m - 1, m_f - 1, deltas);

		// Step 6
		memcpy(w + k, v, 2 * k);
		w[0] = u[0];
		w[k] ^= u[0];
		for (i = 1; i < k; ++i) {
			w[i] = u[i] ^ gf_mul_avx2(gammas_sums[i], v[i]);
			w[k + i] ^= w[i];
		}
	}
}

/**
 * @brief Evaluates f on all fields elements using an additive FFT algorithm
 *
 * f_coeffs is the number of coefficients of f (one less than its degree). <br>
 * The FFT proceeds recursively to evaluate f at all subset sums of a basis B. <br>
 * This implementation is based on the paper from Gao and Mateer: <br>
 * Shuhong Gao and Todd Mateer, Additive Fast Fourier Transforms over Finite
 * Fields, IEEE Transactions on Information Theory 56 (2010), 6265--6272.
 * http://www.math.clemson.edu/~sgao/papers/GM10.pdf <br>
 * and includes improvements proposed by Bernstein, Chou and Schwabe here:
 * https://binary.cr.yp.to/mcbits-20130616.pdf <br>
 * Note that on this first call (as opposed to the recursive calls to fft_rec),
 * gammas are equal to betas, meaning the first gammas subset sums are actually
 * the subset sums of betas (except 1). <br>
 * Also note that f is altered during computation (twisted at each level).
 *
 * @param[out] w Array
 * @param[in] f Array of 2^PARAM_FFT elements
 * @param[in] f_coeffs Number coefficients of f (i.e. deg(f)+1)
 */
void fft_avx2(uint16_t *w, const uint16_t *f, size_t f_coeffs)
{
	uint16_t betas[LC_HQC_PARAM_M - 1] = { 0 };
	uint16_t betas_sums[1 << (LC_HQC_PARAM_M - 1)] = { 0 };
	uint16_t f0[1 << (LC_HQC_PARAM_FFT - 1)] = { 0 };
	uint16_t f1[1 << (LC_HQC_PARAM_FFT - 1)] = { 0 };
	uint16_t deltas[LC_HQC_PARAM_M - 1] = { 0 };
	uint16_t u[1 << (LC_HQC_PARAM_M - 1)] = { 0 };
	uint16_t v[1 << (LC_HQC_PARAM_M - 1)] = { 0 };

	size_t i, k;

	// Follows Gao and Mateer algorithm
	compute_fft_betas(betas);

	// Step 1: PARAM_FFT > 1, nothing to do

	// Compute gammas sums
	compute_subset_sums(betas_sums, betas, LC_HQC_PARAM_M - 1);

	// Step 2: beta_m = 1, nothing to do

	// Step 3
	radix(f0, f1, f, LC_HQC_PARAM_FFT);

	// Step 4: Compute deltas
	for (i = 0; i < LC_HQC_PARAM_M - 1; ++i)
		deltas[i] = gf_square_avx2(betas[i]) ^ betas[i];

	// Step 5
	fft_rec(u, f0, (f_coeffs + 1) / 2, LC_HQC_PARAM_M - 1,
		LC_HQC_PARAM_FFT - 1, deltas);
	fft_rec(v, f1, f_coeffs / 2, LC_HQC_PARAM_M - 1, LC_HQC_PARAM_FFT - 1,
		deltas);

	k = 1 << (LC_HQC_PARAM_M - 1);
	// Step 6, 7 and error polynomial computation
	memcpy(w + k, v, 2 * k);

	// Check if 0 is root
	w[0] = u[0];

	// Check if 1 is root
	w[k] ^= u[0];

	// Find other roots
	for (i = 1; i < k; ++i) {
		w[i] = u[i] ^ gf_mul_avx2(betas_sums[i], v[i]);
		w[k + i] ^= w[i];
	}
}

/**
 * @brief Retrieves the error polynomial error from the evaluations w of the ELP
 * (Error Locator Polynomial) on all field elements.
 *
 * @param[out] error Array with the error
 * @param[out] error_compact Array with the error in a compact form
 * @param[in] w Array of size 2^PARAM_M
 */
void fft_retrieve_error_poly_avx2(uint8_t *error, const uint16_t *w)
{
	uint16_t gammas[LC_HQC_PARAM_M - 1] = { 0 };
	uint16_t gammas_sums[1 << (LC_HQC_PARAM_M - 1)] = { 0 };
	uint16_t k;
	size_t i, index;

	compute_fft_betas(gammas);
	compute_subset_sums(gammas_sums, gammas, LC_HQC_PARAM_M - 1);

	k = 1 << (LC_HQC_PARAM_M - 1);
	error[0] ^= 1 ^ ((uint16_t)-w[0] >> 15);
	error[0] ^= 1 ^ ((uint16_t)-w[k] >> 15);

	for (i = 1; i < k; ++i) {
		index = LC_HQC_PARAM_GF_MUL_ORDER - gf_log[gammas_sums[i]];
		error[index] ^= 1 ^ ((uint16_t)-w[i] >> 15);

		index = LC_HQC_PARAM_GF_MUL_ORDER - gf_log[gammas_sums[i] ^ 1];
		error[index] ^= 1 ^ ((uint16_t)-w[k + i] >> 15);
	}
}
