// The GPL version 3 License (GPLv3)
// Copyright (c) 2018 vtdev.com
// This file is part of the CEX Cryptographic library.
// This program is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//
// 
// Principal Algorithms:
// This cipher is based on the Threefish1024 stream cipher designed by Daniel J. Bernstein:
// Threefish1024: <a href="http://www.ecrypt.eu.org/stream/salsa20pf.html"/>.
// 
// Implementation Details:
// Threefish1024: An implementation if the Threefish1024 implemented as a stream cipher
// Written by John Underhill, September 11, 2018
// Updated December 20, 2018
// Contact: develop@vtdev.com

#ifndef CEX_THREEFISH1024_H
#define CEX_THREEFISH1024_H

#include "IStreamCipher.h"
#include "ShakeModes.h"
#include "SymmetricSecureKey.h"

NAMESPACE_STREAM

using Enumeration::ShakeModes;
using Key::Symmetric::SymmetricSecureKey;

/// <summary>
/// A parallelized and vectorized Threefish1024 120-round stream cipher [TSX1024] implementation.
/// <para>Uses an optional authentication mode (HMAC(SHA2) or KMAC set through the constructor) to authenticate the stream.</para>
/// </summary>
/// 
/// <example>
/// <description>Encrypt an array:</description>
/// <code>
/// SymmetricKey k(Key, Nonce);
/// Threefish1024 cipher;
/// // set to false to run in sequential mode
/// cipher.IsParallel() = true;
/// // calculated automatically based on cache size, but overridable
/// cipher.ParallelBlockSize() = cipher.ProcessorCount() * 6400;
/// cipher.Initialize(true, k);
/// cipher.Transform(Input, InOffset, Output, OutOffset, Length);
/// </code>
///
/// <description>Encrypt and authenticate an array:</description>
/// <code>
/// SymmetricKey k(Key, Nonce);
/// Threefish1024 cipher(StreamAuthenticators::HMACSHA256);
/// // set to false to run in sequential mode
/// cipher.IsParallel() = true;
/// // initialize for encryption
/// cipher.Initialize(true, k);
/// cipher.Transform(Input, InOffset, Output, OutOffset, Length);
/// // copy mac to end of ciphertext array
/// cipher.Finalize(Output, OutOffset + Length, CodeLength);
/// </code>
///
/// <description>Decrypt and authenticate an array:</description>
/// <code>
/// SymmetricKey k(Key, Nonce);
/// Threefish1024 cipher(StreamAuthenticators::HMACSHA256);
/// // set parallel to true to run in parallel mode
/// cipher.IsParallel() = true;
/// // initialize for decryption
/// cipher.Initialize(false, k);
/// // decrypt the ciphertext
/// cipher.Transform(Input, InOffset, Output, OutOffset, Length);
/// // copy mac to temp for comparison
/// std::vector&lt;byte&gt; mac(cipher.TagSize(), 0);
/// cipher.Finalize(mac, 0, mac.size());
/// // constant time comparison of mac to embedded  code
/// IntUtils::Compare(Input, InOffset + Length, mac, 0, mac.size());
/// </code>
/// </example>
/// 
/// <remarks>
/// <description><B>Overview:</B></description>
/// <para></para>
///
/// <description><B>Multi-Threading:</B></description>
/// <para>The transformation function used by Threefish is not limited by a dependency chain; this mode can be both SIMD pipelined and multi-threaded. \n
/// This is achieved by pre-calculating the counters positional offset over multiple 'chunks' of key-stream, which are then generated independently across threads. \n 
/// The key stream generated by encrypting the counter array(s), is used as a source of random, and XOR'd with the message input to produce the cipher text.</para>
///
/// <description>Implementation Notes:</description>
/// <list type="bullet">
/// <item><description></description></item>
/// <item><description>The Key size is 64 bytes (512 bits).</description></item>
/// <item><description>This cipher is capable of authentication by setting the constructors StreamAuthenticators enumeration to one of the HMAC or KMAC options.</description></item>
/// <item><description>Use the Finalize(Output, Offset, Length) function to calculate the MAC code; that code can either be appended to the cipher-text on encryption, or used to compare to an existing code in the stream using the decryption mode.</description></item>
/// <item><description>If authentication is enabled, the cipher-key and MAC seed are generated using SHAKE, this will change the cipher-text output.</description></item>
/// <item><description>In authenticated mode, the cipher-key generated by SHAKE will be constant even with differing MAC generators; only two cipher-text outputs are possible, authenticated or non-authenticated.</description></item>
/// <item><description>The nonce size is 16 bytes (128 bits), this value is optional but recommended.</description></item>
/// <item><description>Block size is 64 bytes (512 bits) wide.</description></item>
/// <item><description>The Info string is optional, but can be used to create a tweakable cipher; must be no more than 16 bytes in length.</description></item>
/// <item><description>Authentication using HMAC or KMAC, can be invoked by setting the StreamAuthenticators parameter in the constructor.</description></item>
/// <item><description>The authentication code can be generated and added to an encrypted stream using the Finalize(Output, Offset, Length) function.</description></item>
/// <item><description>A MAC code can be verified by calling the boolean Verify(Input, Offset, Length) function.</description></item>
/// <item><description>Permutation rounds are fixed at 96.</description></item>
/// <item><description>Encryption can both be pipelined (AVX2 or AVX512), and multi-threaded with any even number of threads.</description></item>
/// <item><description>The Transform functions are virtual, and can be accessed from an ICipherMode instance.</description></item>
/// <item><description>The transformation methods can not be called until the Initialize(SymmetricKey) function has been called.</description></item>
/// <item><description>If the system supports Parallel processing, and IsParallel() is set to true; passing an input block of ParallelBlockSize() to the transform will be auto parallelized.</description></item>
/// <item><description>The ParallelThreadsMax() property is used as the thread count in the parallel loop; this must be an even number no greater than the number of processer cores on the system.</description></item>
/// <item><description>ParallelBlockSize() is calculated automatically based on processor(s) cache size but can be user defined, but must be evenly divisible by ParallelMinimumSize().</description></item>
/// <item><description>The ParallelBlockSize() can be changed through the ParallelProfile() property</description></item>
/// <item><description>Parallel block calculation ex. <c>ParallelBlockSize = N - (N % .ParallelMinimumSize);</c></description></item>
/// </list>
/// 
/// <description>Guiding Publications:</description>
/// <list type="number">
/// <item><description>The Skein Hash Function Family <a href="https://www.schneier.com/academic/paperfiles/skein1.3.pdf">Skein V1.1</a>.</description></item>
/// <item><description>NIST Round 3 <a href="https://www.schneier.com/academic/paperfiles/skein-1.3-modifications.pdf">Tweak Description</a>.</description></item>
/// <item><description>Skein <a href="https://www.schneier.com/academic/paperfiles/skein-proofs.pdf">Provable Security</a> Support for the Skein Hash Family.</description></item>
/// <item><description>NIST <a href="http://nvlpubs.nist.gov/nistpubs/ir/2012/NIST.IR.7896.pdf">SHA3 Third-Round Report</a> of the SHA-3 Cryptographic Hash Algorithm Competition>.</description></item>
/// <item><description>FIPS 202: <a href="http://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf">Permutation Based Hash</a> and Extendable Output Functions</description></item>
/// <item><description>NIST <a href="http://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-185.pdf">SP800-185</a> SHA-3 Derived Functions.</description></item>
/// <item><description>Team Keccak <a href="https://keccak.team/index.html">Homepage</a>.</description></item>
/// </list>
/// 
/// </remarks>
class Threefish1024 final : public IStreamCipher
{
private:

	static const size_t BLOCK_SIZE = 128;
	static const std::vector<byte> CSHAKE_CUST;
	static const std::string CLASS_NAME;
	static const size_t INFO_SIZE = 16;
	static const size_t KEY_SIZE = 128;
	static const size_t NONCE_SIZE = 2;
	static const std::vector<byte> OMEGA_INFO;
	static const size_t ROUND_COUNT = 120;
	static const size_t STATE_PRECACHED = 2048;
	static const size_t STATE_SIZE = 128;

	struct Threefish1024State;

	StreamAuthenticators m_authenticatorType;
	std::unique_ptr<Threefish1024State> m_cipherState;
	bool m_isAuthenticated;
	bool m_isDestroyed;
	bool m_isEncryption;
	bool m_isInitialized;
	std::vector<SymmetricKeySize> m_legalKeySizes;
	std::unique_ptr<IMac> m_macAuthenticator;
	ulong m_macCounter;
	std::unique_ptr<SymmetricSecureKey> m_macKey;
	std::vector<byte> m_macTag;
	ParallelOptions m_parallelProfile;

public:

	//~~~Constructor~~~//

	/// <summary>
	/// Copy constructor: copy is restricted, this function has been deleted
	/// </summary>
	Threefish1024(const Threefish1024&) = delete;

	/// <summary>
	/// Copy operator: copy is restricted, this function has been deleted
	/// </summary>
	Threefish1024& operator=(const Threefish1024&) = delete;

	/// <summary>
	/// Initialize the Threefish-1024 cipher.
	/// <para>Setting the optional AuthenticatorType parameter to any value other than None, enables authentication for this cipher.
	/// Use the Finalize function to derive the Mac code once processing of the message stream has completed.
	/// The default authenticator parameter in Threefish-1024 is KMAC1024</para>
	/// </summary>
	/// 
	/// <param name="AuthenticatorType">The authentication engine, the default is KMAC1024</param>
	///
	/// <exception cref="Exception::CryptoSymmetricCipherException">Thrown if an invalid authentication method is chosen</exception>
	explicit Threefish1024(StreamAuthenticators AuthenticatorType = StreamAuthenticators::KMAC1024);

	/// <summary>
	/// Destructor: finalize this class
	/// </summary>
	~Threefish1024() override;

	//~~~Accessors~~~//

	/// <summary>
	/// Read Only: Internal block size of internal cipher in bytes.
	/// <para>Block size is 128 bytes wide.</para>
	/// </summary>
	const size_t BlockSize() override;

	/// <summary>
	/// Read Only: The maximum size of the distribution code in bytes.
	/// <para>The distribution code is set with the ISymmetricKey Info parameter; and can be used as a secondary domain key.</para>
	/// </summary>
	const size_t DistributionCodeMax() override;

	/// <summary>
	/// Read Only: The stream ciphers type name
	/// </summary>
	const StreamCiphers Enumeral() override;

	/// <summary>
	/// Read Only: Cipher has authentication enabled
	/// </summary>
	const bool IsAuthenticator() override;

	/// <summary>
	/// Read Only: Cipher is ready to transform data
	/// </summary>
	const bool IsInitialized() override;

	/// <summary>
	/// Read Only: Processor parallelization availability.
	/// <para>Indicates whether parallel processing is available with this mode.
	/// If parallel capable, input/output data arrays passed to the transform must be ParallelBlockSize in bytes to trigger parallelization.</para>
	/// </summary>
	const bool IsParallel() override;

	/// <summary>
	/// Read Only: Array of SymmetricKeySize containers, containing legal cipher input key sizes
	/// </summary>
	const std::vector<SymmetricKeySize> &LegalKeySizes() override;

	/// <summary>
	/// Read Only: The stream ciphers class name
	/// </summary>
	const std::string Name() override;

	/// <summary>
	/// Read Only: Parallel block size; the byte-size of the input/output data arrays passed to a transform that trigger parallel processing.
	/// <para>This value can be changed through the ParallelProfile class.</para>
	/// </summary>
	const size_t ParallelBlockSize() override;

	/// <summary>
	/// Read/Write: Parallel and SIMD capability flags and recommended sizes.
	/// <para>The maximum number of threads allocated when using multi-threaded processing can be set with the ParallelMaxDegree() property.
	/// The ParallelBlockSize() property is auto-calculated, but can be changed; the value must be evenly divisible by ParallelMinimumSize().
	/// Changes to these values must be made before the Initialize(bool, ISymmetricKey) function is called.</para>
	/// </summary>
	ParallelOptions &ParallelProfile() override;

	/// <summary>
	/// Read Only: The current MAC tag value
	/// </summary>
	const std::vector<byte> &Tag() override;

	/// <summary>
	/// Read Only: The legal tag length in bytes
	/// </summary>
	const size_t TagSize() override;

	//~~~Public Functions~~~//

	/// <summary>
	/// The stream ciphers authentication MAC generator type.
	/// <para>Change the MAC generator (HMAC, KMAK -N), type used to authenticate the stream.</para>
	/// </summary>
	/// 
	/// <param name="AuthenticatorType">The MAC generator type used to calculate the authentication code</param>
	void Authenticator(StreamAuthenticators AuthenticatorType) override;

	/// <summary>
	/// Initialize the cipher with an ISymmetricKey key container.
	/// <para>If authentication is enabled, setting the Encryption parameter to false will decrypt and authenticate a ciphertext stream.
	/// Authentication on a decrypted stream can be performed by manually by comparing output with the the Finalize(Output, Offset, Length) function.
	/// If encryption and authentication are set to true, the MAC code can be appended to the ciphertext array using the Finalize(Output, Offset, Length) function.</para>
	/// </summary>
	/// 
	/// <param name="Encryption">Using Encryption or Decryption mode</param>
	/// <param name="KeyParams">Cipher key structure, containing cipher key, nonce and optional info array</param>
	///
	/// <exception cref="Exception::CryptoSymmetricCipherException">Thrown if a null or invalid key is used</exception>
	void Initialize(bool Encryption, ISymmetricKey &KeyParams) override;

	/// <summary>
	/// Set the maximum number of threads allocated when using multi-threaded processing.
	/// <para>When set to zero, thread count is set automatically. If set to 1, sets IsParallel() to false and runs in sequential mode. 
	/// Thread count must be an even number, and not exceed the number of processor [virtual] cores.</para>
	/// </summary>
	///
	/// <param name="Degree">The desired number of threads</param>
	void ParallelMaxDegree(size_t Degree) override;

	/// <summary>
	/// Add additional data to the authentication generator.  
	/// <para>Must be called after Initialize(bool, ISymmetricKey), and can be called before or after a stream segment has been processed.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input array of bytes to process</param>
	/// <param name="Offset">Starting offset within the input array</param>
	/// <param name="Length">The number of bytes to process</param>
	///
	/// <exception cref="Exception::CryptoSymmetricCipherException">Thrown if the cipher is not initialized</exception>
	void SetAssociatedData(const std::vector<byte> &Input, const size_t Offset, const size_t Length) override;

	/// <summary>
	/// Encrypt/Decrypt an array of bytes with offset and length parameters.
	/// <para>Initialize(bool, ISymmetricKey) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">The input array of bytes to transform</param>
	/// <param name="InOffset">Starting offset within the input array</param>
	/// <param name="Output">The output array of transformed bytes</param>
	/// <param name="OutOffset">Starting offset within the output array</param>
	/// <param name="Length">Number of bytes to process</param>
	void Transform(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset, const size_t Length) override;

private:

	void Finalize(std::vector<byte> &Output, const size_t OutOffset, const size_t Length);
	void Generate(std::array<ulong, 2> &Counter, std::vector<byte> &Output, const size_t OutOffset, const size_t Length);
	void Process(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset, const size_t Length);
	void Reset();
};

NAMESPACE_STREAMEND
#endif

