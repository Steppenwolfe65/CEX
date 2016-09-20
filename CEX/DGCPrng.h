// The MIT License (MIT)
// 
// Copyright (c) 2016 vtdev.com
// This file is part of the CEX Cryptographic library.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef _CEXENGINE_DGCPRNG_H
#define _CEXENGINE_DGCPRNG_H

#include "IRandom.h"
#include "DGCDrbg.h"
#include "Digests.h"
#include "IDigest.h"
#include "ISeed.h"
#include "SeedGenerators.h"

NAMESPACE_PRNG

using CEX::Generator::DGCDrbg;
using CEX::Enumeration::Digests;
using CEX::Digest::IDigest;
using CEX::Seed::ISeed;
using CEX::Enumeration::SeedGenerators;

/// <summary>
/// DGCPrng: An implementation of a Digest Counter based Random Number Generator
/// </summary> 
/// 
/// <example>
/// <description>Example of generating a pseudo random integer:</description>
/// <code>
/// DGCPrng rnd([Digests], [SeedGenerators], [Buffer Size]);
/// int num = rnd.Next([Minimum], [Maximum]);
/// </code>
/// </example>
/// 
/// <seealso cref="CEX::Digest"/>
/// <seealso cref="CEX::Digest::IDigest"/>
/// <seealso cref="CEX::Enumeration::Digests"/>
/// 
/// <remarks>
/// <description>Implementation Notes:</description>
/// <list type="bullet">
/// <item><description>Can be initialized with any digest.</description></item>
/// <item><description>Can use either a random seed generator for initialization, or a user supplied Seed array.</description></item>
/// <item><description>Numbers generated with the same seed will produce the same random output.</description></item>
/// </list>
/// 
/// <description>Guiding Publications:</description>
/// <list type="number">
/// <item><description>NIST <a href="http://csrc.nist.gov/publications/nistpubs/800-90A/SP800-90A.pdf">SP800-90A</a>: Appendix E1.</description></item>
/// <item><description>NIST <a href="http://csrc.nist.gov/publications/drafts/800-90/draft-sp800-90b.pdf">SP800-90B</a>: Recommendation for the Entropy Sources Used for Random Bit Generation.</description></item>
/// <item><description>NIST <a href="http://csrc.nist.gov/publications/fips/fips140-2/fips1402.pdf">Fips 140-2</a>: Security Requirments For Cryptographic Modules.</description></item>
/// <item><description>NIST <a href="http://csrc.nist.gov/groups/ST/toolkit/rng/documents/SP800-22rev1a.pdf">SP800-22 1a</a>: A Statistical Test Suite for Random and Pseudorandom Number Generators for Cryptographic Applications.</description></item>
/// <item><description>NIST <a href="http://eprint.iacr.org/2006/379.pdf">Security Bounds</a> for the NIST Codebook-based: Deterministic Random Bit Generator.</description></item>
/// </list>
/// 
/// </remarks>
class DGCPrng : public IRandom
{
private:
	static constexpr size_t BUFFER_SIZE = 1024;

	size_t m_bufferIndex = 0;
	size_t m_bufferSize = 0;
	std::vector<byte> m_byteBuffer;
	IDigest* m_digestEngine;
	Digests m_digestType;
	bool m_isDestroyed;
	DGCDrbg* m_rngGenerator;
	ISeed* m_seedGenerator;
	SeedGenerators m_seedType;
	std::vector<byte> m_stateSeed;

public:

	//~~~Properties~~~//

	/// <summary>
	/// Get: The prngs type name
	/// </summary>
	virtual const Prngs Enumeral() { return Prngs::DGCPrng; }

	/// <summary>
	/// Get: Algorithm name
	/// </summary>
	virtual const char *Name() { return "DGCPrng"; }

	//~~~Constructor~~~//

	/// <summary>
	/// Initialize the class
	/// </summary>
	/// 
	/// <param name="DigestEngine">The digest that powers the rng (default is Keccak512)</param>
	/// <param name="SeedEngine">The Seed engine used to create the salt (default is CSPRsg)</param>
	/// <param name="BufferSize">The size of the internal state buffer in bytes; must be at least 128 bytes size (default is 1024)</param>
	/// 
	/// <exception cref="CEX::Exception::CryptoRandomException">Thrown if the buffer size is too small (min. 64)</exception>
	DGCPrng(Digests DigestEngine = Digests::Keccak512, SeedGenerators SeedEngine = SeedGenerators::CSPRsg, size_t BufferSize = BUFFER_SIZE)
		:
		m_bufferIndex(0),
		m_bufferSize(BufferSize),
		m_byteBuffer(BufferSize),
		m_digestType(DigestEngine),
		m_isDestroyed(false),
		m_seedType(SeedEngine)
	{
#if defined(CPPEXCEPTIONS_ENABLED)
		if (BufferSize < 64)
			throw CryptoRandomException("DGCPrng:Ctor", "BufferSize must be at least 64 bytes!");
#endif
		Reset();
	}

	/// <summary>
	/// Initialize the class with a Seed; note: the same seed will produce the same random output
	/// </summary>
	/// 
	/// <param name="Seed">The Seed bytes used to initialize the digest counter; (min. length is digest blocksize + 8)</param>
	/// <param name="DigestEngine">The digest that powers the rng (default is Keccak512)</param>
	/// <param name="BufferSize">The size of the internal state buffer in bytes; must be at least 128 bytes size (default is 1024)</param>
	/// 
	/// <exception cref="CEX::Exception::CryptoRandomException">Thrown if the seed is null or buffer size is too small; (min. seed = digest blocksize + 8)</exception>
	explicit DGCPrng(std::vector<byte> Seed, Digests DigestEngine = Digests::Keccak512, size_t BufferSize = BUFFER_SIZE)
		:
		m_bufferIndex(0),
		m_bufferSize(BufferSize),
		m_byteBuffer(BufferSize),
		m_digestType(DigestEngine),
		m_isDestroyed(false)
	{
#if defined(CPPEXCEPTIONS_ENABLED)
		if (Seed.size() == 0)
			throw CryptoRandomException("DGCPrng:Ctor", "Seed can not be null!");
		if (GetMinimumSeedSize(DigestEngine) < Seed.size())
			throw CryptoRandomException("DGCPrng:Ctor", "The state seed is too small! must be at least digest block size + 8 bytes");
		if (BufferSize < 128)
			throw CryptoRandomException("DGCPrng:Ctor", "BufferSize must be at least 128 bytes!");
#endif
		m_seedType = SeedGenerators::CSPRsg;
		Reset();
	}

	/// <summary>
	/// Finalize objects
	/// </summary>
	virtual ~DGCPrng()
	{
		Destroy();
	}

	//~~~Public Methods~~~//

	/// <summary>
	/// Release all resources associated with the object
	/// </summary>
	virtual void Destroy();

	/// <summary>
	/// Return an array filled with pseudo random bytes
	/// </summary>
	/// 
	/// <param name="Size">Size of requested byte array</param>
	/// 
	/// <returns>Random byte array</returns>
	virtual std::vector<byte> GetBytes(size_t Size);

	/// <summary>
	/// Fill an array with pseudo random bytes
	/// </summary>
	///
	/// <param name="Output">Output array</param>
	virtual void GetBytes(std::vector<byte> &Data);

	/// <summary>
	/// Get a pseudo random unsigned 32bit integer
	/// </summary>
	/// 
	/// <returns>Random 32bit integer</returns>
	virtual uint Next();

	/// <summary>
	/// Get an pseudo random unsigned 32bit integer
	/// </summary>
	/// 
	/// <param name="Maximum">Maximum value</param>
	/// 
	/// <returns>Random 32bit integer</returns>
	virtual uint Next(uint Maximum);

	/// <summary>
	/// Get a pseudo random unsigned 32bit integer
	/// </summary>
	/// 
	/// <param name="Minimum">Minimum value</param>
	/// <param name="Maximum">Maximum value</param>
	/// 
	/// <returns>Random 32bit integer</returns>
	virtual uint Next(uint Minimum, uint Maximum);

	/// <summary>
	/// Get a pseudo random unsigned 64bit integer
	/// </summary>
	/// 
	/// <returns>Random 64bit integer</returns>
	virtual ulong NextLong();

	/// <summary>
	/// Get a ranged pseudo random unsigned 64bit integer
	/// </summary>
	/// 
	/// <param name="Maximum">Maximum value</param>
	/// 
	/// <returns>Random 64bit integer</returns>
	virtual ulong NextLong(ulong Maximum);

	/// <summary>
	/// Get a ranged pseudo random unsigned 64bit integer
	/// </summary>
	/// 
	/// <param name="Minimum">Minimum value</param>
	/// <param name="Maximum">Maximum value</param>
	/// 
	/// <returns>Random 64bit integer</returns>
	virtual ulong NextLong(ulong Minimum, ulong Maximum);

	/// <summary>
	/// Reset the generator instance
	/// </summary>
	virtual void Reset();

private:
	std::vector<byte> GetBits(std::vector<byte> Data, ulong Maximum);
	std::vector<byte> GetByteRange(ulong Maximum);
	IDigest* GetInstance(Digests RngEngine);
	uint GetMinimumSeedSize(Digests RngEngine);
	ISeed* GetSeedGenerator(SeedGenerators SeedEngine);
};

NAMESPACE_PRNGEND
#endif