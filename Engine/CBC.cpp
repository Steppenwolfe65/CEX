#include "CBC.h"
#include "IntUtils.h"
#include "ParallelUtils.h"

NAMESPACE_MODE

void CBC::DecryptBlock(const std::vector<byte> &Input, std::vector<byte> &Output)
{
	// copy input to temp iv
	memcpy(&_cbcNextIv[0], &Input[0], Input.size());
	// decrypt input
	_blockCipher->DecryptBlock(Input, Output);
	// xor output and iv
	CEX::Utility::IntUtils::XORBLK(_cbcIv, 0, Output, 0, _cbcIv.size());
	// copy forward iv
	memcpy(&_cbcIv[0], &_cbcNextIv[0], _cbcIv.size());
}

void CBC::DecryptBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset)
{
	// copy input to temp iv
	memcpy(&_cbcNextIv[0], &Input[InOffset], _blockSize);
	// decrypt input
	_blockCipher->DecryptBlock(Input, InOffset, Output, OutOffset);
	// xor output and iv
	CEX::Utility::IntUtils::XORBLK(_cbcIv, 0, Output, OutOffset, _cbcIv.size());
	// copy forward iv
	memcpy(&_cbcIv[0], &_cbcNextIv[0], _cbcIv.size());
}

void CBC::Destroy()
{
	if (!_isDestroyed)
	{
		_isDestroyed = true;
		_blockSize = 0;
		_isEncryption = false;
		_isInitialized = false;
		_processorCount = 0;
		_isParallel = false;
		_parallelBlockSize = 0;

		CEX::Utility::IntUtils::ClearVector(_cbcIv);
		CEX::Utility::IntUtils::ClearVector(_cbcNextIv);
		CEX::Utility::IntUtils::ClearArray(_threadVectors);
	}
}

void CBC::EncryptBlock(const std::vector<byte> &Input, std::vector<byte> &Output)
{
	// xor iv and input
	CEX::Utility::IntUtils::XORBLK(Input, 0, _cbcIv, 0, _cbcIv.size());
	// encrypt iv
	_blockCipher->EncryptBlock(_cbcIv, Output);
	// copy output to iv
	memcpy(&_cbcIv[0], &Output[0], _blockSize);
}

void CBC::EncryptBlock(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset)
{
	// xor iv and input
	CEX::Utility::IntUtils::XORBLK(Input, InOffset, _cbcIv, 0, _cbcIv.size());
	// encrypt iv
	_blockCipher->EncryptBlock(_cbcIv, 0, Output, OutOffset);
	// copy output to iv
	memcpy(&_cbcIv[0], &Output[OutOffset], _blockSize);
}

void CBC::Initialize(bool Encryption, const CEX::Common::KeyParams &KeyParam)
{
	_blockCipher->Initialize(Encryption, KeyParam);
	_cbcIv = KeyParam.IV();
	_cbcNextIv.resize(_cbcIv.size(), 0);
	_isEncryption = Encryption;
	_isInitialized = true;
}

void CBC::Transform(const std::vector<byte> &Input, std::vector<byte> &Output)
{
	if (_isEncryption)
	{
		EncryptBlock(Input, Output);
	}
	else
	{
		if (_isParallel)
			ParallelDecrypt(Input, Output);
		else
			DecryptBlock(Input, Output);
	}
}

void CBC::Transform(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset)
{
	if (_isEncryption)
	{
		EncryptBlock(Input, InOffset, Output, OutOffset);
	}
	else
	{
		if (_isParallel)
			ParallelDecrypt(Input, InOffset, Output, OutOffset);
		else
			DecryptBlock(Input, InOffset, Output, OutOffset);
	}
}

void CBC::ParallelDecrypt(const std::vector<byte> &Input, std::vector<byte> &Output)
{
	if (Output.size() < _parallelBlockSize)
	{
		size_t blocks = Output.size() / _blockSize;

		// output is input xor with random
		for (size_t i = 0; i < blocks; i++)
			DecryptBlock(Input, i * _blockSize, Output, i * _blockSize);
	}
	else
	{
		// parallel CBC decryption
		const size_t cnkSize = _parallelBlockSize / _processorCount;
		const size_t blkSize = _blockSize;
		const size_t blkCount = (cnkSize / blkSize);

		_threadVectors.resize(_processorCount);

		for (size_t i = 0; i < _processorCount; i++)
		{
			_threadVectors[i].resize(_blockSize, 0);

			if (i != 0)
				memcpy(&_threadVectors[i][0], &Input[(i * cnkSize) - blkSize], blkSize);
			else
				memcpy(&_threadVectors[i][0], &_cbcIv[0], blkSize);
		}

		CEX::Utility::ParallelUtils::ParallelFor(0, _processorCount, [this, &Input, &Output, cnkSize, blkCount, blkSize](size_t i)
		{
			std::vector<byte> &thdVec = _threadVectors[i];
			this->ProcessDecrypt(Input, i * cnkSize, Output, i * cnkSize, thdVec, blkCount);
		});

		// copy the last vector to class variable
		memcpy(&_cbcIv[0], &_threadVectors[_processorCount - 1][0], _cbcIv.size());
	}
}

void CBC::ParallelDecrypt(const std::vector<byte> &Input, const size_t InOffset, std::vector<byte> &Output, const size_t OutOffset)
{
	if ((Output.size() - OutOffset) < _parallelBlockSize)
	{
		size_t blocks = (Output.size() - OutOffset) / _blockSize;

		// output is input xor with random
		for (size_t i = 0; i < blocks; i++)
			DecryptBlock(Input, (i * _blockSize) + InOffset, Output, (i * _blockSize) + OutOffset);
	}
	else
	{
		// parallel CBC decryption //
		const size_t cnkSize = _parallelBlockSize / _processorCount;
		const size_t blkSize = _blockSize;
		const size_t blkCount = (cnkSize / blkSize);

		_threadVectors.resize(_processorCount);

		for (size_t i = 0; i < _processorCount; i++)
		{
			_threadVectors[i].resize(blkSize, 0);

			// get the vectors
			if (i != 0)
				memcpy(&_threadVectors[i][0], &Input[(InOffset + (i * cnkSize)) - blkSize], blkSize);
			else
				memcpy(&_threadVectors[i][0], &_cbcIv[0], blkSize);
		}


		CEX::Utility::ParallelUtils::ParallelFor(0, _processorCount, [this, &Input, InOffset, &Output, OutOffset, cnkSize, blkCount, blkSize](size_t i)
		{
			std::vector<byte> &thdVec = _threadVectors[i];
			this->ProcessDecrypt(Input, InOffset + i * cnkSize, Output, OutOffset + i * cnkSize, thdVec, blkCount);
		});
		// copy the last vector to class variable
		memcpy(&_cbcIv[0], &_threadVectors[_processorCount - 1][0], _cbcIv.size());
	}
}

void CBC::ProcessDecrypt(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset, std::vector<byte> &Iv, const size_t BlockCount)
{
	std::vector<byte> nextIv(Iv.size(), 0);

	for (size_t i = 0; i < BlockCount; i++)
	{
		memcpy(&nextIv[0], &Input[InOffset], nextIv.size());
		// decrypt input
		_blockCipher->DecryptBlock(Input, InOffset, Output, OutOffset);
		// xor output and iv
		CEX::Utility::IntUtils::XORBLK(Iv, 0, Output, OutOffset, Iv.size());
		memcpy(&Iv[0], &nextIv[0], nextIv.size());
		InOffset += Iv.size();
		OutOffset += Iv.size();
	}
}

void CBC::SetScope()
{
	_processorCount = CEX::Utility::ParallelUtils::ProcessorCount();
	if (_processorCount % 2 != 0)
		_processorCount--;
	if (_processorCount > 1)
		_isParallel = true;

	// calc default parallel block size as n * 64kb
	_parallelBlockSize = _processorCount * PARALLEL_DEFBLOCK;
}

NAMESPACE_MODEEND