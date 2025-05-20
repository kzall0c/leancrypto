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

use std::ptr;
use std::sync::atomic;
use crate::ffi::leancrypto;
use crate::error::SignatureError;

pub enum lcr_dilithium_type {
	lcr_dilithium_44,
	lcr_dilithium_65,
	lcr_dilithium_87,
}

/// Leancrypto wrapper for lc_dilithium
pub struct lcr_dilithium {
	// Context
	//dilithium_ctx: *mut leancrypto::lc_dilithium_ctx,

	/// Dilithium public key
	pk: leancrypto::lc_dilithium_pk,

	/// Dilithium secret key
	sk: leancrypto::lc_dilithium_sk,

	/// Dilithium signature
	sig: leancrypto::lc_dilithium_sig,

	pk_set: bool,
	sk_set: bool,
	sig_set: bool,
}

#[allow(dead_code)]
impl lcr_dilithium {
	pub fn new() -> Self {
		lcr_dilithium {
			//dilithium_ctx: ptr::null_mut(),
			pk: unsafe { std::mem::zeroed() },
			sk: unsafe { std::mem::zeroed() },
			sig: unsafe { std::mem::zeroed() },
			pk_set: false,
			sk_set: false,
			sig_set: false,
		}
	}

	/// Load secret key for using with leancrypto
	///
	/// [sk_buf] buffer with raw secret key
	pub fn sk_load(&mut self, sk_buf: &[u8]) -> Result<(), SignatureError> {
		// No check for self.sk_set == false as we allow overwriting
		// of existing key.

		let result = unsafe {
			leancrypto::lc_dilithium_sk_load(&mut self.sk,
							 sk_buf.as_ptr(),
							 sk_buf.len())
		};
		if result < 0 {
			return Err(SignatureError::ProcessingError);
		}

		self.sk_set = true;

		Ok(())
	}

	/// Load public key for using with leancrypto
	///
	/// [pk_buf] buffer with raw public key
	pub fn pk_load(&mut self, pk_buf: &[u8]) -> Result<(), SignatureError> {
		// No check for self.pk_set == false as we allow overwriting
		// of existing key.

		let result = unsafe {
			leancrypto::lc_dilithium_pk_load(&mut self.pk,
							 pk_buf.as_ptr(),
							 pk_buf.len())
		};
		if result < 0 {
			return Err(SignatureError::ProcessingError);
		}

		self.pk_set = true;

		Ok(())
	}

	/// Load signature using with leancrypto
	///
	/// [sig_buf] buffer with raw signature
	pub fn sig_load(&mut self, sig_buf: &[u8]) ->
		Result<(), SignatureError> {
		// No check for self.sig_set == false as we allow overwriting
		// of existing key.

		let result = unsafe {
			leancrypto::lc_dilithium_sig_load(&mut self.sig,
							  sig_buf.as_ptr(),
							  sig_buf.len())
		};
		if result < 0 {
			return Err(SignatureError::ProcessingError);
		}

		self.sig_set = true;

		Ok(())
	}

	fn lcr_dilithium_type_mapping(dilithium_type: lcr_dilithium_type) ->
		u32 {
		match dilithium_type {
			lcr_dilithium_type::lcr_dilithium_44 =>
				leancrypto::lc_dilithium_type_LC_DILITHIUM_44,
			lcr_dilithium_type::lcr_dilithium_65 =>
				leancrypto::lc_dilithium_type_LC_DILITHIUM_65,
			lcr_dilithium_type::lcr_dilithium_87 =>
				leancrypto::lc_dilithium_type_LC_DILITHIUM_87,
		}
	}

	/// Generate Dilithium / ML-DSA key pair
	///
	/// [dilithium_type] key type
	pub fn keypair(&mut self, dilithium_type: lcr_dilithium_type) ->
		Result<(), SignatureError> {
		let result = unsafe {
			leancrypto::lc_dilithium_keypair(
				&mut self.pk, &mut self.sk,
				leancrypto::lc_seeded_rng,
				Self::lcr_dilithium_type_mapping(dilithium_type))
		};
		if result < 0 {
			return Err(SignatureError::ProcessingError);
		}

		self.sk_set = true;
		self.pk_set = true;

		Ok(())
	}

	/// Sign message with pure signature operation
	///
	/// [msg] holds the message to be signed
	pub fn sign(&mut self, msg: &[u8]) -> Result<(), SignatureError> {
		if self.sk_set == false {
			return Err(SignatureError::UninitializedContext);
		}

		let result = unsafe {
			leancrypto::lc_dilithium_sign(
				&mut self.sig, msg.as_ptr(), msg.len(),
				&self.sk, leancrypto::lc_seeded_rng)
		};
		if result < 0 {
			return Err(SignatureError::ProcessingError);
		}

		self.sig_set = true;

		Ok(())
	}

	/// Deterministically sign message with pure signature operation
	///
	/// [msg] holds the message to be signed
	pub fn sign_deterministic(&mut self, msg: &[u8]) ->
		Result<(), SignatureError> {
		if self.sk_set == false {
			return Err(SignatureError::UninitializedContext);
		}

		let result = unsafe {
			leancrypto::lc_dilithium_sign(
				&mut self.sig, msg.as_ptr(), msg.len(),
				&self.sk, ptr::null_mut())
		};
		if result < 0 {
			return Err(SignatureError::ProcessingError);
		}

		self.sig_set = true;

		Ok(())
	}

	/// Verify message with pure signature operation
	///
	/// [msg] holds the message to be verified
	pub fn verify(&mut self, msg: &[u8]) -> Result<(), SignatureError> {
		if self.pk_set == false || self.sig_set == false {
			return Err(SignatureError::UninitializedContext);
		}

		let result = unsafe {
			leancrypto::lc_dilithium_verify(
				&mut self.sig, msg.as_ptr(), msg.len(),
				&self.pk)
		};
		if result == -1*(leancrypto::EBADMSG as i32) {
			return Err(SignatureError::VerificationError);
		}
		if result < 0 {
			return Err(SignatureError::ProcessingError);
		}

		Ok(())
	}

	/// Method for safe immutable access to signature buffer
	pub fn sig(&mut self) -> (&[u8], Result<(), SignatureError>) {
		if self.sig_set == false {
			return (&[], Err(SignatureError::UninitializedContext));
		}

		let mut ptr: *mut u8 = ptr::null_mut();
		let mut len: usize = 0;

		let result = unsafe {
			leancrypto::lc_dilithium_sig_ptr(&mut ptr, &mut len,
							 &mut self.sig)
		};
		if result < 0 {
			return (&[], Err(SignatureError::ProcessingError));
		}

		let slice = unsafe { std::slice::from_raw_parts(ptr, len) };

		(&slice, Ok(()))
	}

	/// Method for safe immutable access to secret key buffer
	pub fn sk(&mut self) -> (&[u8], Result<(), SignatureError>) {
		if self.sk_set == false {
			return (&[], Err(SignatureError::UninitializedContext));
		}

		let mut ptr: *mut u8 = ptr::null_mut();
		let mut len: usize = 0;

		let result = unsafe {
			leancrypto::lc_dilithium_sk_ptr(&mut ptr, &mut len,
							&mut self.sk)
		};
		if result < 0 {
			return (&[], Err(SignatureError::ProcessingError));
		}

		let slice = unsafe { std::slice::from_raw_parts(ptr, len) };

		(&slice, Ok(()))
	}

	/// Method for safe immutable access to public key buffer
	pub fn pk(&mut self) -> (&[u8], Result<(), SignatureError>) {
		if self.pk_set == false {
			return (&[], Err(SignatureError::UninitializedContext));
		}

		let mut ptr: *mut u8 = ptr::null_mut();
		let mut len: usize = 0;

		let result = unsafe {
			leancrypto::lc_dilithium_pk_ptr(&mut ptr, &mut len,
							&mut self.pk)
		};
		if result < 0 {
			return (&[], Err(SignatureError::ProcessingError));
		}

		let slice = unsafe { std::slice::from_raw_parts(ptr, len) };

		(&slice, Ok(()))
	}

	/// This is a test function verifying whether the zeroization succeeds
	fn zeroize_check(&mut self, sk: &mut leancrypto::lc_dilithium_sk)
	{
		let mut ptr: *mut u8 = ptr::null_mut();
		let mut len: usize = 0;
		unsafe {
			leancrypto::lc_dilithium_sk_ptr(&mut ptr, &mut len, sk)
		};

		let slice = unsafe { std::slice::from_raw_parts(ptr, len) };

		assert_eq!(Self::sk(self).0, slice);
	}
}

/// This ensures the sensitive buffers are always zeroized
/// regardless of when it goes out of scope
impl Drop for lcr_dilithium {
	fn drop(&mut self) {
		let /*mut*/ sk: leancrypto::lc_dilithium_sk = unsafe {
			std::mem::zeroed()
		};

		unsafe { std::ptr::write_volatile(&mut self.sk, sk) };
		atomic::compiler_fence(atomic::Ordering::SeqCst);
		//Self::zeroize_check(self, &mut sk);
	}
}
