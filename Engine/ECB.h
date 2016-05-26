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
// An implementation of an Electronic CodeBook Mode (ECB).
// Written by John Underhill, September 24, 2014
// contact: develop@vtdev.com

#ifndef _CEXENGINE_ECB_H
#define _CEXENGINE_ECB_H

#include "ICipherMode.h"

NAMESPACE_MODE

/// <summary>
/// Implements an Electronic Cookbook Mode: ECB (Insecure Mode; For Testing Only!)
/// </summary> 
/// 
/// <example>
/// <description>Example using an <c>ICipherMode</c> interface:</description>
/// <code>
/// ECB cipher(new RDX());
/// // initialize for encryption
/// cipher.Initialize(true, KeyParams(Key));
/// // encrypt a block
/// cipher.Transform(Input, Output);
/// </code>
/// </example>
/// 
/// <seealso cref="CEX::Cipher::Symmetric::Block"/>
/// <seealso cref="CEX::Cipher::Symmetric::Block::Mode::ICipherMode"/>
/// <seealso cref="CEX::Enumeration::BlockCiphers"/>
/// 
/// <remarks>
/// <description>Implementation Notes:</description>
/// <list type="bullet">
/// <item><description>ECB is not a secure mode, and should only be used for testing.</description></item>
/// </list>
/// 
/// <description>Guiding Publications:</description>
/// <list type="number">
/// <item><description>NIST <a href="http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf">SP800-38A</a>.</description></item>
/// </list>
/// </remarks>
class ECB : public ICipherMode
{
private:
	IBlockCipher* _blockCipher;
	size_t _blockSize;
	std::vector<byte> _ecbIv;
	bool _isDestroyed;
	bool _isEncryption;
	bool _isInitialized;
	bool _isParallel;
	size_t _parallelBlockSize;
	size_t _processorCount;

public:

	// *** Properties *** //

	/// <summary>
	/// Get: Unit block size of internal cipher
	/// </summary>
	virtual const size_t BlockSize() { return _blockSize; }

	/// <summary>
	/// Get: Underlying Cipher
	/// </summary>
	virtual IBlockCipher* Engine() { return _blockCipher; }

	/// <summary>
	/// Get: The cipher modes type name
	/// </summary>
	virtual const CEX::Enumeration::CipherModes Enumeral() { return CEX::Enumeration::CipherModes::ECB; }

	/// <summary>
	/// Get: Initialized for encryption, false for decryption
	/// </summary>
	virtual const bool IsEncryption() { return _isEncryption; }

	/// <summary>
	/// Get: Cipher is ready to transform data
	/// </summary>
	virtual const bool IsInitialized() { return _isInitialized; }

	/// <summary>
	/// Get/Set: Automatic processor parallelization
	/// </summary>
	virtual bool &IsParallel() { return _isParallel; }

	/// <summary>
	/// Get: The current state of the initialization Vector
	/// </summary>
	virtual const std::vector<byte> &IV() { return _ecbIv; }

	/// <summary>
	/// Get: Available Encryption Key Sizes in bytes
	/// </summary>
	virtual const std::vector<size_t> &LegalKeySizes() { return _blockCipher->LegalKeySizes(); }

	/// <summary>
	/// Get: Cipher name
	/// </summary>
	virtual const char* Name() { return "ECB"; }

	/// <summary>
	/// Get/Set: Parallel block size. Must be a multiple of <see cref="ParallelMinimumSize"/>.
	/// <para>The parallel block size is calculated automatically based on the number of available processors on the system (n * 64kb).</para>
	/// </summary>
	/// 
	/// <exception cref="CryptoCipherModeException">Thrown if a parallel block size is not evenly divisible by ParallelMinimumSize, 
	/// or block size is less than ParallelMinimumSize or more than ParallelMaximumSize values</exception>
	virtual size_t &ParallelBlockSize() { return _parallelBlockSize; }

	/// <summary>
	/// Get: Maximum input size with parallel processing
	/// </summary>
	virtual const size_t ParallelMaximumSize() { return 0; }

	/// <summary>
	/// Get: The smallest parallel block size. Parallel blocks must be a multiple of this size.
	/// </summary>
	virtual const size_t ParallelMinimumSize() { return 0; }

	/// <remarks>
	/// Get: Processor count
	/// </remarks>
	virtual const size_t ProcessorCount() { return _processorCount; }

	// *** Constructor *** //

	/// <summary>
	/// Initialize the Cipher
	/// </summary>
	///
	/// <param name="Cipher">Underlying encryption algorithm</param>
	///
	/// <exception cref="CEX::Exception::CryptoCipherModeException">Thrown if a null Cipher or valid block size is used</exception>
	explicit ECB(IBlockCipher* Cipher)
		:
		_blockCipher(Cipher),
		_blockSize(Cipher->BlockSize()),
		_ecbIv(0),
		_isDestroyed(false),
		_isEncryption(false),
		_isInitialized(false),
		_isParallel(false),
		_parallelBlockSize(0),
		_processorCount(0)
	{
		if (Cipher == 0)
			throw CryptoCipherModeException("ECB:CTor", "The Cipher can not be null!");

		SetScope();
	}

	/// <summary>
	/// Finalize objects
	/// </summary>
	virtual ~ECB()
	{
		Destroy();
	}

	// *** Public Methods *** //

	/// <summary>
	/// Decrypt a single block of bytes.
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Encrypted bytes</param>
	/// <param name="Output">Decrypted bytes</param>
	void DecryptBlock(const std::vector<byte> &Input, std::vector<byte> &Output);

	/// <summary>
	/// Decrypt a block of bytes with offset parameters.
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Encrypted bytes</param>
	/// <param name="InOffset">Offset in the Input array</param>
	/// <param name="Output">Decrypted bytes</param>
	/// <param name="OutOffset">Offset in the Output array</param>
	void DecryptBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset);

	/// <summary>
	/// Release all resources associated with the object
	/// </summary>
	virtual void Destroy();

	/// <summary>
	/// Encrypt a block of bytes. 
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Input bytes to Transform</param>
	/// <param name="Output">Output product of Transform</param>
	void EncryptBlock(const std::vector<byte> &Input, std::vector<byte> &Output);

	/// <summary>
	/// Encrypt a block of bytes with offset parameters. 
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Input bytes to Transform</param>
	/// <param name="InOffset">Offset in the Input array</param>
	/// <param name="Output">Output product of Transform</param>
	/// <param name="OutOffset">Offset in the Output array</param>
	void EncryptBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset);

	/// <summary>
	/// Initialize the Cipher
	/// </summary>
	/// 
	/// <param name="Encryption">True if cipher is used for encryption, false to decrypt</param>
	/// <param name="KeyParam">KeyParam containing key and vector</param>
	/// 
	/// <exception cref="CryptoCipherModeException">Thrown if a null Key or IV is used</exception>
	virtual void Initialize(bool Encryption, const CEX::Common::KeyParams &KeyParam);

	/// <summary>
	/// Transform a block of bytes. 
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// <param name="Input">Input bytes to Transform</param>
	/// <param name="Output">Output product of Transform</param>
	virtual void Transform(const std::vector<byte> &Input, std::vector<byte> &Output);

	/// <summary>
	/// Transform a block of bytes with offset parameters. 
	/// <para>Initialize(bool, KeyParams) must be called before this method can be used.</para>
	/// </summary>
	/// 
	/// <param name="Input">Input bytes to Transform</param>
	/// <param name="InOffset">Offset in the Input array</param>
	/// <param name="Output">Output product of Transform</param>
	/// <param name="OutOffset">Offset in the Output array</param>
	virtual void Transform(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset);

private:
	void SetScope();
};

NAMESPACE_MODEEND
#endif