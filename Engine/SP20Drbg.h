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
// 
// Implementation Details:
// An implementation of a Salsa20 Counter based Deterministic Random Byte Generator (SP20Drbg). 
// Written by John Underhill, November 21, 2014
// contact: develop@vtdev.com

#ifndef _CEXENGINE_SP20DRBG_H
#define _CEXENGINE_SP20DRBG_H

#include "IGenerator.h"

NAMESPACE_GENERATOR

/// <summary>
/// SP20Drbg: A parallelized Salsa20 deterministic random byte generator implementation
/// </summary>
/// 
/// <example>
/// <description>Generate an array of pseudo random bytes:</description>
/// <code>
/// SP20Drbg rnd(20);
/// // initialize
/// rnd.Initialize(Salt, [Ikm], [Nonce]);
/// // generate bytes
/// rnd.Generate(Output, [Offset], [Size]);
/// </code>
/// </example>
/// 
/// <remarks>
/// <description>Implementation Notes:</description>
/// <list type="bullet">
/// <item><description>Valid Key sizes are 128, 256 (16 and 32 bytes).</description></item>
/// <item><description>Block size is 64 bytes wide.</description></item>
/// <item><description>Valid rounds are 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28 and 30.</description></item>
/// <item><description>Parallel block size is 64,000 bytes by default; but is configurable.</description></item>
/// </list>
/// 
/// <description>Guiding Publications:</description>
/// <list type="number">
/// <item><description>Salsa20 <a href="http://www.ecrypt.eu.org/stream/salsa20pf.html">Specification</a>.</description></item>
/// <item><description>Salsa20 <a href="http://cr.yp.to/snuffle/design.pdf">Design</a>.</description></item>
/// <item><description>Salsa20 <a href="http://cr.yp.to/snuffle/security.pdf">Security</a>.</description></item>
/// </list>
/// 
/// </remarks>
class SP20Drbg : public IGenerator
{
private:
	static constexpr size_t BLOCK_SIZE = 64;
	static constexpr size_t DEFAULT_ROUNDS = 20;
	static constexpr size_t KEY_SIZE = 32;
	static constexpr size_t MAXALLOC_MB100 = 100000000;
	static constexpr size_t MAX_PARALLEL = 1024000;
	static constexpr size_t MAX_ROUNDS = 30;
	static constexpr size_t MIN_PARALLEL = 1024;
	static constexpr size_t MIN_ROUNDS = 8;
	static constexpr size_t PARALLEL_CHUNK = 1024;
	static constexpr size_t PARALLEL_DEFBLOCK = 64000;
	static constexpr const char *SIGMA = "expand 32-byte k";
	static constexpr size_t STATE_SIZE = 16;
	static constexpr size_t VECTOR_SIZE = 8;

	std::vector<uint> _ctrVector;
	std::vector<byte> _dstCode;
	bool _isDestroyed;
	bool _isInitialized;
	bool _isParallel;
	std::vector<size_t> _legalKeySizes;
	std::vector<size_t> _legalRounds;
	size_t _parallelBlockSize;
	size_t _processorCount;
	size_t _rndCount;
	std::vector<std::vector<uint>> _threadVectors;
	std::vector<uint> _wrkState;

public:

	// *** Properties *** //

	/// <summary>
	/// Get: The generators type name
	/// </summary>
	virtual const CEX::Enumeration::Generators Enumeral() { return CEX::Enumeration::Generators::SP20Drbg; }

	/// <summary>
	/// Get: Generator is ready to produce data
	/// </summary>
	virtual const bool IsInitialized() { return _isInitialized; }

	/// <summary>
	/// Get/Set: Automatic processor parallelization
	/// </summary>
	bool &IsParallel() { return _isParallel; }

	/// <summary>
	/// Get: The current state of the initialization Vector
	/// </summary>
	const std::vector<uint> IV() { return _ctrVector; }

	/// <summary>
	/// <para>Minimum initialization key size in bytes; 
	/// combined sizes of Salt, Ikm, and Nonce must be at least this size.</para>
	/// </summary>
	virtual size_t KeySize() { return KEY_SIZE; }

	/// <summary>
	/// Get: Available Encryption Key Sizes in bytes
	/// </summary>
	const std::vector<size_t>&LegalKeySizes() { return _legalKeySizes; };

	/// <summary>
	/// Get: Available diffusion round assignments
	/// </summary>
	const std::vector<size_t> &LegalRounds() { return _legalRounds; }

	/// <summary>
	/// Get: Cipher name
	/// </summary>
	virtual const char *Name() { return "SP20Drbg"; }

	/// <summary>
	/// Get/Set: Parallel block size. Must be a multiple of <see cref="ParallelMinimumSize"/>.
	/// </summary>
	size_t &ParallelBlockSize() { return _parallelBlockSize; }

	/// <summary>
	/// Get: Maximum input size with parallel processing
	/// </summary>
	const size_t ParallelMaximumSize() { return MAXALLOC_MB100; }

	/// <summary>
	/// Get: The smallest parallel block size. Parallel blocks must be a multiple of this size.
	/// </summary>
	const size_t ParallelMinimumSize() { return _processorCount * (STATE_SIZE * 4); }

	/// <remarks>
	/// Get: Processor count
	/// </remarks>
	const size_t ProcessorCount() { return _processorCount; }

	/// <summary>
	/// Get: Initialization vector size
	/// </summary>
	const size_t VectorSize() { return VECTOR_SIZE; }

	// *** Constructor *** //

	/// <summary>
	/// Initialize the SP20 generator
	/// </summary>
	/// 
	/// <param name="Rounds">The number of transformation rounds</param>
	explicit SP20Drbg(size_t Rounds = 20)
		:
		_ctrVector(2, 0),
		_dstCode(0),
		_isDestroyed(false),
		_isInitialized(false),
		_isParallel(false),
		_legalKeySizes(2),
		_legalRounds(12),
		_parallelBlockSize(PARALLEL_DEFBLOCK),
		_processorCount(0),
		_rndCount(Rounds),
		_wrkState(14, 0)
	{
		_legalKeySizes = { 16, 32 };
		_legalRounds = { 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30 };
		SetScope();
	}

	/// <summary>
	/// Finalize objects
	/// </summary>
	virtual ~SP20Drbg()
	{
		Destroy();
	}

	// *** Public Methods *** //

	/// <summary>
	/// Release all resources associated with the object
	/// </summary>
	virtual void Destroy();

	/// <summary>
	/// Generate a block of pseudo random bytes
	/// </summary>
	/// 
	/// <param name="Output">Output array filled with random bytes</param>
	/// 
	/// <returns>Number of bytes generated</returns>
	virtual size_t Generate(std::vector<byte> &Output);

	/// <summary>
	/// Generate pseudo random bytes
	/// </summary>
	/// 
	/// <param name="Output">Output array filled with random bytes</param>
	/// <param name="OutOffset">The starting position within Output array</param>
	/// <param name="Size">Number of bytes to generate</param>
	/// 
	/// <returns>Number of bytes generated</returns>
	///
	/// <exception cref="CEX::Exception::CryptoGeneratorException">Thrown if the output buffer is too small</exception>
	virtual size_t Generate(std::vector<byte> &Output, size_t OutOffset, size_t Size);

	/// <summary>
	/// Initialize the generator with a Key
	/// </summary>
	/// 
	/// <param name="Ikm">The Key value; minimum size is 2* the digests output size</param>
	/// 
	/// <exception cref="CEX::Exception::CryptoGeneratorException">Thrown if the Key is too small</exception>
	virtual void Initialize(const std::vector<byte> &Ikm);

	/// <summary>
	/// Initialize the generator with a Salt value and a Key
	/// </summary>
	/// 
	/// <param name="Salt">The Salt value</param>
	/// <param name="Ikm">The Key value</param>
	virtual void Initialize(const std::vector<byte> &Salt, const std::vector<byte> &Ikm);

	/// <summary>
	/// Initialize the generator with a Salt value, a Key, and an Information nonce
	/// </summary>
	/// 
	/// <param name="Salt">The Salt value</param>
	/// <param name="Ikm">The Key value</param>
	/// <param name="Nonce">The Nonce value</param>
	virtual void Initialize(const std::vector<byte> &Salt, const std::vector<byte> &Ikm, const std::vector<byte> &Nonce);

	/// <summary>
	/// Update the Salt material
	/// </summary>
	/// 
	/// <param name="Salt">Pseudo random seed material</param>
	/// 
	/// <exception cref="CEX::Exception::CryptoGeneratorException">Thrown if the Salt value is too small</exception>
	virtual void Update(const std::vector<byte> &Salt);

private:
	void Generate(const size_t Length, std::vector<uint> &Counter, std::vector<byte> &Output, const size_t OutOffset);
	void Increase(const std::vector<uint> &Counter, const size_t Size, std::vector<uint> &Vector);
	void Increment(std::vector<uint> &Counter);
	void SalsaCore(std::vector<byte> &Output, const size_t OutOffset, const std::vector<uint> &Counter);
	void SetKey(const std::vector<byte> &Key, const std::vector<byte> &Iv);
	void SetScope();
	void Transform(std::vector<byte> &Output, size_t OutOffset);
};

NAMESPACE_GENERATOREND
#endif