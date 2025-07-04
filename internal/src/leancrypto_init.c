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

#include "ext_headers.h"
#include "initialization.h"
#include "lc_init.h"
#include "visibility.h"

LC_INIT_FUNCTION(int, lc_init, unsigned int flags)
{
	(void)flags;

#if (defined(LC_ASCON_HASH) || defined(CONFIG_LEANCRYPTO_ASCON_HASH))
	ascon_fastest_impl();
#endif

#if (defined(LC_SHA2_256) || defined(CONFIG_LEANCRYPTO_SHA2_256))
	sha256_fastest_impl();
#endif

#if (defined(LC_SHA2_512) || defined(CONFIG_LEANCRYPTO_SHA2_512))
	sha512_fastest_impl();
#endif

#if (defined(LC_SHA3) || defined(CONFIG_LEANCRYPTO_SHA3))
	sha3_fastest_impl();
#endif

#if (defined(LC_AES) || defined(CONFIG_LEANCRYPTO_AES))
	aes_fastest_impl();
#endif

#if ((defined(LC_KYBER) || defined(CONFIG_LEANCRYPTO_KEM)) &&                  \
     defined(LC_HOST_RISCV64))
	kyber_riscv_rvv_selector();
#endif
#if (defined(LC_SECEXEC_LINIX) && !defined(LINUX_KERNEL))
	secure_execution_linux();
#endif

#if (defined(LC_CHACHA20) || defined(CONFIG_LEANCRYPTO_CHACHA20))
	//TODO: currently this does not compile due to FIPS
	//chacha20_fastest_impl();
#endif

	return 0;
}
