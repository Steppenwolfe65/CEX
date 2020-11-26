#include "RCS.h"
#include "CpuDetect.h"
#include "IntegerTools.h"
#include "KMAC.h"
#include "MemoryTools.h"
#include "Rijndael.h"
#include "SHAKE.h"
#include "StreamAuthenticators.h"

NAMESPACE_STREAM

using namespace Cipher::Block::RijndaelBase;
using Tools::IntegerTools;
using Mac::KMAC;
using Enumeration::KmacModes;
using Tools::MemoryTools;
using Tools::ParallelTools;
using Enumeration::ShakeModes;
using Enumeration::StreamAuthenticators;
using Enumeration::StreamCipherConvert;

class RCS::RcsState
{
public:

	SecureVector<uint> RoundKeys;
	SecureVector<byte> Associated;
	SecureVector<byte> Custom;
	SecureVector<byte> MacKey;
	SecureVector<byte> MacTag;
	SecureVector<byte> Name;
	std::vector<SymmetricKeySize> LegalKeySizes;
	std::vector<byte> Nonce;
	ulong Counter;
	uint Rounds;
	KmacModes Authenticator;
	ShakeModes Mode;
	bool IsAuthenticated;
	bool IsEncryption;
	bool IsInitialized;

	RcsState(bool Authenticate)
		:
		RoundKeys(0),
		Associated(0),
		Custom(0),
		MacKey(0),
		MacTag(0),
		Name(0),
		LegalKeySizes{
			SymmetricKeySize(IK256_SIZE, BLOCK_SIZE, INFO_SIZE),
			SymmetricKeySize(IK512_SIZE, BLOCK_SIZE, INFO_SIZE),
			SymmetricKeySize(IK1024_SIZE, BLOCK_SIZE, INFO_SIZE) },
		Nonce(BLOCK_SIZE, 0x00),
		Counter(0),
		Rounds(0),
		Authenticator(KmacModes::None),
		Mode(ShakeModes::None),
		IsAuthenticated(Authenticate),
		IsEncryption(false),
		IsInitialized(false)
	{
	}

	RcsState(SecureVector<byte> &State)
		:
		RoundKeys(0),
		Associated(0),
		Custom(0),
		MacKey(0),
		MacTag(0),
		Name(0),
		LegalKeySizes{
			SymmetricKeySize(IK256_SIZE, BLOCK_SIZE, INFO_SIZE),
			SymmetricKeySize(IK512_SIZE, BLOCK_SIZE, INFO_SIZE),
			SymmetricKeySize(IK1024_SIZE, BLOCK_SIZE, INFO_SIZE) },
		Nonce(BLOCK_SIZE, 0x00),
		Counter(0),
		Rounds(0),
		Authenticator(KmacModes::None),
		Mode(ShakeModes::None),
		IsAuthenticated(false),
		IsEncryption(false),
		IsInitialized(false)
	{
		DeSerialize(State);
	}

	~RcsState()
	{
		MemoryTools::Clear(RoundKeys, 0, RoundKeys.size() * sizeof(uint));
		MemoryTools::Clear(Associated, 0, Associated.size());
		MemoryTools::Clear(Custom, 0, Custom.size());
		MemoryTools::Clear(MacKey, 0, MacKey.size());
		MemoryTools::Clear(MacTag, 0, MacTag.size());
		MemoryTools::Clear(Name, 0, Name.size());
		MemoryTools::Clear(Nonce, 0, Nonce.size());
		LegalKeySizes.clear();
		Counter = 0;
		Rounds = 0;
		Authenticator = KmacModes::None;
		Mode = ShakeModes::None;
		IsAuthenticated = false;
		IsEncryption = false;
		IsInitialized = false;
	}

	void DeSerialize(SecureVector<byte> &SecureState)
	{
		size_t soff;
		ushort vlen; 

		soff = 0;

		MemoryTools::CopyToObject(SecureState, soff, &vlen, sizeof(ushort));
		RoundKeys.resize(vlen / sizeof(uint));
		soff += sizeof(ushort);
		MemoryTools::Copy(SecureState, soff, RoundKeys, 0, vlen);
		soff += vlen;

		MemoryTools::CopyToObject(SecureState, soff, &vlen, sizeof(ushort));
		Associated.resize(vlen);
		soff += sizeof(ushort);
		MemoryTools::Copy(SecureState, soff, Associated, 0, Associated.size());
		soff += vlen;

		MemoryTools::CopyToObject(SecureState, soff, &vlen, sizeof(ushort));
		Custom.resize(vlen);
		soff += sizeof(ushort);
		MemoryTools::Copy(SecureState, soff, Custom, 0, Custom.size());
		soff += vlen;

		MemoryTools::CopyToObject(SecureState, soff, &vlen, sizeof(ushort));
		MacKey.resize(vlen);
		soff += sizeof(ushort);
		MemoryTools::Copy(SecureState, soff, MacKey, 0, MacKey.size());
		soff += vlen;

		MemoryTools::CopyToObject(SecureState, soff, &vlen, sizeof(ushort));
		MacTag.resize(vlen);
		soff += sizeof(ushort);
		MemoryTools::Copy(SecureState, soff, MacTag, 0, MacTag.size());
		soff += vlen;

		MemoryTools::CopyToObject(SecureState, soff, &vlen, sizeof(ushort));
		Name.resize(vlen);
		soff += sizeof(ushort);
		MemoryTools::Copy(SecureState, soff, Name, 0, Name.size());
		soff += vlen;

		MemoryTools::CopyToObject(SecureState, soff, &vlen, sizeof(ushort));
		Nonce.resize(vlen);
		soff += sizeof(ushort);
		MemoryTools::Copy(SecureState, soff, Nonce, 0, Nonce.size());
		soff += vlen;

		MemoryTools::CopyToObject(SecureState, soff, &Counter, sizeof(ulong));
		soff += sizeof(ulong);
		MemoryTools::CopyToObject(SecureState, soff, &Rounds, sizeof(uint));
		soff += sizeof(uint);

		MemoryTools::CopyToObject(SecureState, soff, &Authenticator, sizeof(KmacModes));
		soff += sizeof(KmacModes);
		MemoryTools::CopyToObject(SecureState, soff, &Mode, sizeof(ShakeModes));
		soff += sizeof(ShakeModes);

		MemoryTools::CopyToObject(SecureState, soff, &IsAuthenticated, sizeof(bool));
		soff += sizeof(bool);
		MemoryTools::CopyToObject(SecureState, soff, &IsEncryption, sizeof(bool));
		soff += sizeof(bool);
		MemoryTools::CopyToObject(SecureState, soff, &IsInitialized, sizeof(bool));
	}

	void Reset()
	{
		MemoryTools::Clear(RoundKeys, 0, RoundKeys.size() * sizeof(uint));
		MemoryTools::Clear(Associated, 0, Associated.size());
		MemoryTools::Clear(Custom, 0, Custom.size());
		MemoryTools::Clear(MacKey, 0, MacKey.size());
		MemoryTools::Clear(MacTag, 0, MacTag.size());
		MemoryTools::Clear(Name, 0, Name.size());
		MemoryTools::Clear(Nonce, 0, Nonce.size());
		Counter = 0;
		Rounds = 0;
		IsEncryption = false;
		IsInitialized = false;
	}

	SecureVector<byte> Serialize()
	{
		const size_t STASZE = (RoundKeys.size() * sizeof(uint)) + Associated.size() + Custom.size() + MacKey.size() + MacTag.size() +
			Name.size() + Nonce.size() + sizeof(Counter) + sizeof(Rounds) + sizeof(Authenticator) + sizeof(Mode) + (3 * sizeof(bool)) + (7 * sizeof(ushort));

		size_t soff;
		ushort vlen;
		SecureVector<byte> state(STASZE);

		soff = 0;
		vlen = static_cast<ushort>(RoundKeys.size() * sizeof(uint));
		MemoryTools::CopyFromObject(&vlen, state, soff, sizeof(ushort));
		soff += sizeof(ushort);
		MemoryTools::Copy(RoundKeys, 0, state, soff, static_cast<size_t>(vlen));
		soff += vlen;

		vlen = static_cast<ushort>(Associated.size());
		MemoryTools::CopyFromObject(&vlen, state, soff, sizeof(ushort));
		soff += sizeof(ushort);
		MemoryTools::Copy(Associated, 0, state, soff, Associated.size());
		soff += Associated.size();

		vlen = static_cast<ushort>(Custom.size());
		MemoryTools::CopyFromObject(&vlen, state, soff, sizeof(ushort));
		soff += sizeof(ushort);
		MemoryTools::Copy(Custom, 0, state, soff, Custom.size());
		soff += Custom.size();

		vlen = static_cast<ushort>(MacKey.size());
		MemoryTools::CopyFromObject(&vlen, state, soff, sizeof(ushort));
		soff += sizeof(ushort);
		MemoryTools::Copy(MacKey, 0, state, soff, MacKey.size());
		soff += MacKey.size();

		vlen = static_cast<ushort>(MacTag.size());
		MemoryTools::CopyFromObject(&vlen, state, soff, sizeof(ushort));
		soff += sizeof(ushort);
		MemoryTools::Copy(MacTag, 0, state, soff, MacTag.size());
		soff += MacTag.size();

		vlen = static_cast<ushort>(Name.size());
		MemoryTools::CopyFromObject(&vlen, state, soff, sizeof(ushort));
		soff += sizeof(ushort);
		MemoryTools::Copy(Name, 0, state, soff, Name.size());
		soff += Name.size();

		vlen = static_cast<ushort>(Nonce.size());
		MemoryTools::CopyFromObject(&vlen, state, soff, sizeof(ushort));
		soff += sizeof(ushort);
		MemoryTools::Copy(Nonce, 0, state, soff, Nonce.size());
		soff += Nonce.size();

		MemoryTools::CopyFromObject(&Counter, state, soff, sizeof(ulong));
		soff += sizeof(ulong);
		MemoryTools::CopyFromObject(&Rounds, state, soff, sizeof(uint));
		soff += sizeof(uint);

		MemoryTools::CopyFromObject(&Authenticator, state, soff, sizeof(KmacModes));
		soff += sizeof(KmacModes);
		MemoryTools::CopyFromObject(&Mode, state, soff, sizeof(ShakeModes));
		soff += sizeof(ShakeModes);

		MemoryTools::CopyFromObject(&IsAuthenticated, state, soff, sizeof(bool));
		soff += sizeof(bool);
		MemoryTools::CopyFromObject(&IsEncryption, state, soff, sizeof(bool));
		soff += sizeof(bool);
		MemoryTools::CopyFromObject(&IsInitialized, state, soff, sizeof(bool));

		return state;
	}
};

//~~~Constructor~~~//

RCS::RCS(bool Authenticate)
	:
	m_rcsState(new RcsState(Authenticate)),
	m_parallelProfile(BLOCK_SIZE, true, STATE_PRECACHED, true)
{
}

RCS::RCS(SecureVector<byte> &State)
	:
	m_rcsState(State.size() > STATE_THRESHOLD ? new RcsState(State) :
		throw CryptoSymmetricException(std::string("RCS"), std::string("Constructor"), std::string("The State array is invalid!"), ErrorCodes::InvalidKey)),
	m_parallelProfile(BLOCK_SIZE, true, STATE_PRECACHED, true)
{
}

RCS::~RCS()
{
}

//~~~Accessors~~~//

const StreamCiphers RCS::Enumeral()
{
	StreamAuthenticators auth;
	StreamCiphers tmpn;

	auth = IsAuthenticator() ?
		static_cast<StreamAuthenticators>(m_rcsState->Authenticator) :
		StreamAuthenticators::None;

	tmpn = StreamCipherConvert::FromDescription(StreamCiphers::RCS, auth);

	return tmpn;
}

const bool RCS::IsAuthenticator()
{
	return m_rcsState->IsAuthenticated;
}

const bool RCS::IsEncryption()
{
	return m_rcsState->IsEncryption;
}

const bool RCS::IsInitialized()
{
	return m_rcsState->IsInitialized;
}

const bool RCS::IsParallel()
{
	return m_parallelProfile.IsParallel();
}

const std::vector<SymmetricKeySize> &RCS::LegalKeySizes()
{
	return m_rcsState->LegalKeySizes;
}

const std::string RCS::Name()
{
	std::string name;

	name = StreamCipherConvert::ToName(Enumeral());

	return name;
}

const std::vector<byte> RCS::Nonce()
{
	return m_rcsState->Nonce;
}

const size_t RCS::ParallelBlockSize()
{
	return m_parallelProfile.ParallelBlockSize();
}

ParallelOptions &RCS::ParallelProfile()
{
	return m_parallelProfile;
}

const std::vector<byte> RCS::Tag()
{
	if (m_rcsState->MacTag.size() == 0 || IsAuthenticator() == false)
	{
		throw CryptoSymmetricException(std::string("RCS"), std::string("Tag"), std::string("The cipher is not initialized for authentication or has not run!"), ErrorCodes::NotInitialized);
	}

	return SecureUnlock(m_rcsState->MacTag);
}

const void RCS::Tag(SecureVector<byte> &Output)
{
	if (m_rcsState->MacTag.size() == 0 || IsAuthenticator() == false)
	{
		throw CryptoSymmetricException(std::string("RCS"), std::string("Tag"), std::string("The cipher is not initialized for authentication or has not run!"), ErrorCodes::NotInitialized);
	}

	SecureCopy(m_rcsState->MacTag, 0, Output, 0, m_rcsState->MacTag.size());
}

const size_t RCS::TagSize()
{
	if (IsInitialized() == false)
	{
		throw CryptoSymmetricException(std::string("RCS"), std::string("TagSize"), std::string("The cipher has not been initialized!"), ErrorCodes::NotInitialized);
	}

	return m_rcsState->MacTag.size();
}

//~~~Public Functions~~~//

void RCS::Initialize(bool Encryption, ISymmetricKey &Parameters)
{
	size_t i;

	if (!SymmetricKeySize::Contains(LegalKeySizes(), Parameters.KeySizes().KeySize()))
	{
		throw CryptoSymmetricException(Name(), std::string("Initialize"), std::string("Invalid key size; key must be one of the LegalKeySizes in length."), ErrorCodes::InvalidKey);
	}
	if (Parameters.KeySizes().IVSize() != BLOCK_SIZE)
	{
		throw CryptoSymmetricException(Name(), std::string("Initialize"), std::string("Requires a nonce equal in size to the ciphers block size!"), ErrorCodes::InvalidNonce);
	}

	if (m_parallelProfile.IsParallel())
	{
		if (m_parallelProfile.IsParallel() && m_parallelProfile.ParallelBlockSize() < m_parallelProfile.ParallelMinimumSize() || m_parallelProfile.ParallelBlockSize() > m_parallelProfile.ParallelMaximumSize())
		{
			throw CryptoSymmetricException(Name(), std::string("Initialize"), std::string("The parallel block size is out of bounds!"), ErrorCodes::InvalidSize);
		}
		if (m_parallelProfile.IsParallel() && m_parallelProfile.ParallelBlockSize() % m_parallelProfile.ParallelMinimumSize() != 0)
		{
			throw CryptoSymmetricException(Name(), std::string("Initialize"), std::string("The parallel block size must be evenly aligned to the ParallelMinimumSize!"), ErrorCodes::InvalidParam);
		}
	}

	// reset for a new key
	if (IsInitialized() == true)
	{
		Reset();
	}

	// set the initial processed-bytes count to one
	m_rcsState->Counter = 1;

	// set the number of rounds -v1.0d
	m_rcsState->Rounds = (Parameters.KeySizes().KeySize() == IK256_SIZE) ?
		RK256_COUNT : 
		(Parameters.KeySizes().KeySize() == IK512_SIZE) ?
			RK512_COUNT : 
			RK1024_COUNT;

	if (m_rcsState->IsAuthenticated)
	{
		m_rcsState->Authenticator = (Parameters.KeySizes().KeySize() == IK1024_SIZE) ?
			KmacModes::KMAC1024 :
			(Parameters.KeySizes().KeySize() == IK512_SIZE) ?
			KmacModes::KMAC512 :
			KmacModes::KMAC256;

		size_t mlen = (m_rcsState->Authenticator == KmacModes::KMAC1024) ? 128 :
			(m_rcsState->Authenticator == KmacModes::KMAC512) ? 64 :
			32;
		 m_rcsState->MacTag.resize(mlen);
	}

	// store the customization string -v1.0d
	if (Parameters.KeySizes().InfoSize() != 0)
	{
		m_rcsState->Custom.resize(Parameters.KeySizes().InfoSize());
		// copy the user defined string to the customization parameter
		MemoryTools::Copy(Parameters.Info(), 0, m_rcsState->Custom, 0, Parameters.KeySizes().InfoSize());
	}

	// create the cSHAKE name string
	std::string tmpn = Name();
	// add mac counter, key-size bits, and algorithm name to name string
	m_rcsState->Name.resize(sizeof(ulong) + sizeof(ushort) + tmpn.size());
	// mac counter is always first 8 bytes
	IntegerTools::Le64ToBytes(m_rcsState->Counter, m_rcsState->Name, 0);
	// add the cipher key size in bits as an unsigned short integer
	ushort kbits = static_cast<ushort>(Parameters.KeySizes().KeySize() * 8);
	IntegerTools::Le16ToBytes(kbits, m_rcsState->Name, sizeof(ulong));
	// copy the name string to state
	MemoryTools::CopyFromObject(tmpn.data(), m_rcsState->Name, sizeof(ulong) + sizeof(ushort), tmpn.size());

	// copy the nonce to state
	MemoryTools::Copy(Parameters.IV(), 0, m_rcsState->Nonce, 0, BLOCK_SIZE);

	// cipher key size determines key expansion function and Mac generator type; 256 or 512-bit
	m_rcsState->Mode = (Parameters.KeySizes().KeySize() == IK512_SIZE) ?
		ShakeModes::SHAKE512 : 
		(Parameters.KeySizes().KeySize() == IK256_SIZE) ?
			ShakeModes::SHAKE256 : 
			ShakeModes::SHAKE1024;

	Kdf::SHAKE gen(m_rcsState->Mode);
	// initialize cSHAKE with k,c,n
	gen.Initialize(Parameters.SecureKey(), m_rcsState->Custom, m_rcsState->Name);

	// size the round key array
	const size_t RNKLEN = static_cast<size_t>(BLOCK_SIZE / sizeof(uint)) * static_cast<size_t>(m_rcsState->Rounds + 1UL);
	m_rcsState->RoundKeys.resize(RNKLEN);
	// generate the round keys to a temporary byte array
	SecureVector<byte> tmpr(RNKLEN * sizeof(uint));
	// generate the ciphers round-keys
	gen.Generate(tmpr);

	// realign in big endian format for ACS test vectors; RCS is the fallback to the AES-NI implementation
	for (i = 0; i < tmpr.size() / sizeof(uint); ++i)
	{
		m_rcsState->RoundKeys[i] = IntegerTools::BeBytesTo32(tmpr, i * sizeof(uint));
	}

	MemoryTools::Clear(tmpr, 0, tmpr.size());
	m_rcsState->IsEncryption = Encryption;
	m_rcsState->IsInitialized = true;
}

void RCS::ParallelMaxDegree(size_t Degree)
{
	if (Degree == 0 || Degree % 2 != 0 || Degree > m_parallelProfile.ProcessorCount())
	{
		throw CryptoSymmetricException(Name(), std::string("ParallelMaxDegree"), std::string("Degree setting is invalid!"), ErrorCodes::NotSupported);
	}

	m_parallelProfile.SetMaxDegree(Degree);
}

void RCS::SetAssociatedData(const std::vector<byte> &Input, size_t Offset, size_t Length)
{
	if (IsInitialized() == false)
	{
		throw CryptoSymmetricException(Name(), std::string("SetAssociatedData"), std::string("The cipher has not been initialized!"), ErrorCodes::NotInitialized);
	}
	if (IsAuthenticator() == false)
	{
		throw CryptoSymmetricException(Name(), std::string("SetAssociatedData"), std::string("The cipher has not been configured for authentication!"), ErrorCodes::IllegalOperation);
	}

	// store the associated data
	m_rcsState->Associated.resize(Length);
	MemoryTools::Copy(Input, Offset, m_rcsState->Associated, 0, Length);
}

void RCS::Transform(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset, size_t Length)
{
	CEXASSERT(IsInitialized(), "The cipher mode has not been initialized!");
	CEXASSERT(IntegerTools::Min(Input.size() - InOffset, Output.size() - OutOffset) >= Length, "The data arrays are smaller than the block-size!");

	if (IsEncryption() == true)
	{
		if (IsAuthenticator() == true)
		{
			if (Output.size() < Length + OutOffset + TagSize())
			{
				throw CryptoSymmetricException(Name(), std::string("Transform"), std::string("The vector is not long enough to add the MAC code!"), ErrorCodes::InvalidSize);
			}

			std::vector<byte> tmpn(BLOCK_SIZE);
			MemoryTools::Copy(m_rcsState->Nonce, 0, tmpn, 0, tmpn.size());
			// encrypt the stream
			Process(Input, InOffset, Output, OutOffset, Length);
			// update the processed bytes counter
			m_rcsState->Counter += Length;
			// finalize the mac and copy the tag to the end of the output stream
			Finalize(m_rcsState, tmpn, Output, OutOffset, Length);
			MemoryTools::Copy(m_rcsState->MacTag, 0, Output, OutOffset + Length, m_rcsState->MacTag.size());
		}
		else
		{
			// encrypt the stream
			Process(Input, InOffset, Output, OutOffset, Length);
		}
	}
	else
	{
		if (IsAuthenticator())
		{
			// update the processed bytes counter
			m_rcsState->Counter += Length;
			// finalize the mac and verify
			Finalize(m_rcsState, m_rcsState->Nonce, Input, InOffset, Length);

			if (!IntegerTools::Compare(Input, InOffset + Length, m_rcsState->MacTag, 0, m_rcsState->MacTag.size()))
			{
				throw CryptoAuthenticationFailure(Name(), std::string("Transform"), std::string("The authentication tag does not match!"), ErrorCodes::AuthenticationFailure);
			}
		}

		// decrypt the stream
		Process(Input, InOffset, Output, OutOffset, Length);
	}
}

//~~~Private Functions~~~//

void RCS::Finalize(std::unique_ptr<RcsState> &State, std::vector<byte> &Nonce, const std::vector<byte> &Input, size_t InOffset, size_t Length)
{
	std::vector<byte> mctr(sizeof(ulong));
	ulong mlen;

	KMAC auth(State->Authenticator);
	// name string is an unsigned 64-bit bytes counter + key-size + cipher-name
	// the state counter is the number of bytes processed by the cipher
	IntegerTools::Le64ToBytes(State->Counter, State->Name, 0);
	SecureVector<byte> zero(0);
	// initialize the generator with the key and name/counter
	SymmetricKey kpm(State->MacKey, zero, State->Name);
	auth.Initialize(kpm);

	// update with the nonce
	auth.Update(Nonce, 0, Nonce.size());

	// update with the input
	auth.Update(Input, InOffset, Length);

	// 1.0c: add the total number of bytes processed by the mac, including this terminating string
	mlen = State->Counter + State->Nonce.size() + State->Associated.size() + mctr.size();
	IntegerTools::LeIncrease8(mctr, mlen);

	// 1.0c: add the associated data to the mac
	if (State->Associated.size() != 0)
	{
		auth.Update(SecureUnlock(State->Associated), 0, State->Associated.size());
		// clear the associated data, reset for each transformation, 
		// assignable with a call to SetAssociatedData before each transform call
		SecureClear(State->Associated);
	}

	// add the termination string to the mac
	auth.Update(mctr, 0, mctr.size());

	// finalize the mac code to state
	auth.Finalize(State->MacTag, 0);
}

void RCS::Generate(std::vector<byte> &Output, size_t OutOffset, size_t Length, std::vector<byte> &Counter)
{
	size_t bctr;

	bctr = 0;

	// Note: The counter length passed into LEIncrement, only processes the first 16 bytes
	// as the full counter length. This is because this cipher is not expected to encrypt
	// more that 2^128 bytes of data with a single key.

#if defined(CEX_HAS_AVX512)

	const size_t AVX512BLK = 16 * BLOCK_SIZE;

	if (Length >= AVX512BLK)
	{
		const size_t PBKALN = Length - (Length % AVX512BLK);
		std::vector<byte> tmpc(AVX512BLK);

		// stagger counters and process 8 blocks with avx512
		while (bctr != PBKALN)
		{
			MemoryTools::Copy(Counter, 0, tmpc, 0, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 32, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 64, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 96, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 128, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 160, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 192, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 224, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 256, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 288, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 320, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 352, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 384, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 416, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 448, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 480, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			Transform4096(tmpc, 0, Output, OutOffset + bctr);
			bctr += AVX512BLK;
		}
	}

#elif defined(CEX_HAS_AVX2)

	const size_t AVX2BLK = 8 * BLOCK_SIZE;

	if (Length >= AVX2BLK)
	{
		const size_t PBKALN = Length - (Length % AVX2BLK);
		std::vector<byte> tmpc(AVX2BLK);

		// stagger counters and process 8 blocks with avx2
		while (bctr != PBKALN)
		{
			MemoryTools::Copy(Counter, 0, tmpc, 0, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 32, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 64, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 96, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 128, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 160, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 192, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 224, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			Transform2048(tmpc, 0, Output, OutOffset + bctr);
			bctr += AVX2BLK;
		}
	}

#elif defined(CEX_HAS_AVX)

	const size_t AVXBLK = 4 * BLOCK_SIZE;

	if (Length >= AVXBLK)
	{
		const size_t PBKALN = Length - (Length % AVXBLK);
		std::vector<byte> tmpc(AVXBLK);

		// 4 blocks with avx
		while (bctr != PBKALN)
		{
			MemoryTools::Copy(Counter, 0, tmpc, 0, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 32, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 64, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			MemoryTools::Copy(Counter, 0, tmpc, 96, BLOCK_SIZE);
			IntegerTools::LeIncrement(Counter, 16);
			Transform1024(tmpc, 0, Output, OutOffset + bctr);
			bctr += AVXBLK;
		}
	}

#endif

	const size_t BLKALN = Length - (Length % BLOCK_SIZE);

	while (bctr != BLKALN)
	{
		Transform256(Counter, 0, Output, OutOffset + bctr);
		IntegerTools::LeIncrement(Counter, 16);
		bctr += BLOCK_SIZE;
	}

	if (bctr != Length)
	{
		std::vector<byte> otp(BLOCK_SIZE);
		Transform256(Counter, 0, otp, 0);
		IntegerTools::LeIncrement(Counter, 16);
		const size_t RMDLEN = Length % BLOCK_SIZE;
		MemoryTools::Copy(otp, 0, Output, OutOffset + (Length - RMDLEN), RMDLEN);
	}
}

CEX_OPTIMIZE_IGNORE
void RCS::PrefetchRoundKey(const SecureVector<uint> &Rkey)
{
	// timing defence: pre-load the round-key array into l1 cache
	MemoryTools::PrefetchL1(Rkey, 0, Rkey.size() * sizeof(uint));
}
CEX_OPTIMIZE_RESUME

CEX_OPTIMIZE_IGNORE
void RCS::PrefetchSbox()
{
	// timing defence: pre-load sbox into l1 cache
	MemoryTools::PrefetchL1(SBox, 0, SBox.size());
}
CEX_OPTIMIZE_RESUME

#if defined(CEX_RIJNDAEL_TABLES)
CEX_OPTIMIZE_IGNORE
void RCS::PrefetchTables()
{
	// timing defence: pre-load multiplication tables into l1 cache
	MemoryTools::PrefetchL1(T0, 0, T0.size() * sizeof(uint));
	MemoryTools::PrefetchL1(T1, 0, T1.size() * sizeof(uint));
	MemoryTools::PrefetchL1(T2, 0, T2.size() * sizeof(uint));
	MemoryTools::PrefetchL1(T3, 0, T3.size() * sizeof(uint));

}
CEX_OPTIMIZE_RESUME
#endif

void RCS::Process(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset, size_t Length)
{
	size_t i;

	const size_t PRLBLK = m_parallelProfile.ParallelBlockSize();

	if (m_parallelProfile.IsParallel() && Length >= PRLBLK)
	{
		const size_t BLKCNT = Length / PRLBLK;

		for (i = 0; i < BLKCNT; ++i)
		{
			ProcessParallel(Input, InOffset + (i * PRLBLK), Output, OutOffset + (i * PRLBLK), PRLBLK);
		}

		const size_t RMDLEN = Length - (PRLBLK * BLKCNT);

		if (RMDLEN != 0)
		{
			const size_t BLKOFT = (PRLBLK * BLKCNT);
			ProcessSequential(Input, InOffset + BLKOFT, Output, OutOffset + BLKOFT, RMDLEN);
		}
	}
	else
	{
		ProcessSequential(Input, InOffset, Output, OutOffset, Length);
	}
}

void RCS::ProcessParallel(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset, size_t Length)
{
	const size_t OUTLEN = Output.size() - OutOffset < Length ? Output.size() - OutOffset : Length;
	const size_t CNKLEN = m_parallelProfile.ParallelBlockSize() / m_parallelProfile.ParallelMaxDegree();
	const size_t CTRLEN = (CNKLEN / BLOCK_SIZE);
	std::vector<byte> tmpc(BLOCK_SIZE);

	ParallelTools::ParallelFor(0, m_parallelProfile.ParallelMaxDegree(), [this, &Input, InOffset, &Output, OutOffset, &tmpc, CNKLEN, CTRLEN](size_t i)
	{
		// thread level counter
		std::vector<byte> thdc(BLOCK_SIZE);
		// offset counter by chunk size / block size  
		IntegerTools::LeIncrease8(m_rcsState->Nonce, thdc, static_cast<uint>(CTRLEN * i));
		const size_t STMPOS = i * CNKLEN;
		// generate random at output offset
		this->Generate(Output, OutOffset + STMPOS, CNKLEN, thdc);
		// xor with input at offsets
		MemoryTools::XOR(Input, InOffset + STMPOS, Output, OutOffset + STMPOS, CNKLEN);

		// store last counter
		if (i == m_parallelProfile.ParallelMaxDegree() - 1)
		{
			MemoryTools::Copy(thdc, 0, tmpc, 0, BLOCK_SIZE);
		}
	});

	// copy last counter to class variable
	MemoryTools::Copy(tmpc, 0, m_rcsState->Nonce, 0, BLOCK_SIZE);

	// last block processing
	const size_t ALNLEN = CNKLEN * m_parallelProfile.ParallelMaxDegree();
	if (ALNLEN < OUTLEN)
	{
		const size_t FNLLEN = (Output.size() - OutOffset) % ALNLEN;
		Generate(Output, ALNLEN, FNLLEN, m_rcsState->Nonce);

		for (size_t i = ALNLEN; i < OUTLEN; i++)
		{
			Output[i] ^= Input[i];
		}
	}
}

void RCS::ProcessSequential(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset, size_t Length)
{
	// get block aligned
	const size_t ALNLEN = Length - (Length % BLOCK_SIZE);
	size_t i;

	// generate random
	Generate(Output, OutOffset, Length, m_rcsState->Nonce);

	if (ALNLEN != 0)
	{
		MemoryTools::XOR(Input, InOffset, Output, OutOffset, ALNLEN);
	}

	// get the remaining bytes
	if (ALNLEN != Length)
	{
		for (i = ALNLEN; i < Length; ++i)
		{
			Output[i + OutOffset] ^= Input[i + InOffset];
		}
	}
}

void RCS::Reset()
{
	m_rcsState->Reset();
	m_parallelProfile.Calculate(m_parallelProfile.IsParallel(), m_parallelProfile.ParallelBlockSize(), m_parallelProfile.ParallelMaxDegree());
}

SecureVector<byte> RCS::Serialize()
{
	SecureVector<byte> tmps = m_rcsState->Serialize();

	return tmps;
}

#if defined(CEX_RIJNDAEL_TABLES) // remove tables

void RCS::Transform256(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset)
{
	const size_t RNDCNT = m_rcsState->RoundKeys.size() - 8;
	size_t kctr;
	uint X0;
	uint X1;
	uint X2;
	uint X3;
	uint X4;
	uint X5;
	uint X6;
	uint X7;
	uint Y0;
	uint Y1;
	uint Y2;
	uint Y3;
	uint Y4;
	uint Y5;
	uint Y6;
	uint Y7;

	// pre-load the round key array into l1 as a timing defence
#if defined(CEX_PREFETCH_RIJNDAEL_TABLES)
	PrefetchRoundKey(m_rcsState->RoundKeys);
#endif

	// round 0
	X0 = IntegerTools::BeBytesTo32(Input, InOffset) ^ m_rcsState->RoundKeys[0];
	X1 = IntegerTools::BeBytesTo32(Input, InOffset + 4) ^ m_rcsState->RoundKeys[1];
	X2 = IntegerTools::BeBytesTo32(Input, InOffset + 8) ^ m_rcsState->RoundKeys[2];
	X3 = IntegerTools::BeBytesTo32(Input, InOffset + 12) ^ m_rcsState->RoundKeys[3];
	X4 = IntegerTools::BeBytesTo32(Input, InOffset + 16) ^ m_rcsState->RoundKeys[4];
	X5 = IntegerTools::BeBytesTo32(Input, InOffset + 20) ^ m_rcsState->RoundKeys[5];
	X6 = IntegerTools::BeBytesTo32(Input, InOffset + 24) ^ m_rcsState->RoundKeys[6];
	X7 = IntegerTools::BeBytesTo32(Input, InOffset + 28) ^ m_rcsState->RoundKeys[7];

	// pre-load the multiplication tables
#if defined(CEX_PREFETCH_RIJNDAEL_TABLES)
	PrefetchTables();
#endif

	// round 1
	Y0 = T0[static_cast<byte>(X0 >> 24)] ^ T1[static_cast<byte>(X1 >> 16)] ^ T2[static_cast<byte>(X3 >> 8)] ^ T3[static_cast<byte>(X4)] ^ m_rcsState->RoundKeys[8];
	Y1 = T0[static_cast<byte>(X1 >> 24)] ^ T1[static_cast<byte>(X2 >> 16)] ^ T2[static_cast<byte>(X4 >> 8)] ^ T3[static_cast<byte>(X5)] ^ m_rcsState->RoundKeys[9];
	Y2 = T0[static_cast<byte>(X2 >> 24)] ^ T1[static_cast<byte>(X3 >> 16)] ^ T2[static_cast<byte>(X5 >> 8)] ^ T3[static_cast<byte>(X6)] ^ m_rcsState->RoundKeys[10];
	Y3 = T0[static_cast<byte>(X3 >> 24)] ^ T1[static_cast<byte>(X4 >> 16)] ^ T2[static_cast<byte>(X6 >> 8)] ^ T3[static_cast<byte>(X7)] ^ m_rcsState->RoundKeys[11];
	Y4 = T0[static_cast<byte>(X4 >> 24)] ^ T1[static_cast<byte>(X5 >> 16)] ^ T2[static_cast<byte>(X7 >> 8)] ^ T3[static_cast<byte>(X0)] ^ m_rcsState->RoundKeys[12];
	Y5 = T0[static_cast<byte>(X5 >> 24)] ^ T1[static_cast<byte>(X6 >> 16)] ^ T2[static_cast<byte>(X0 >> 8)] ^ T3[static_cast<byte>(X1)] ^ m_rcsState->RoundKeys[13];
	Y6 = T0[static_cast<byte>(X6 >> 24)] ^ T1[static_cast<byte>(X7 >> 16)] ^ T2[static_cast<byte>(X1 >> 8)] ^ T3[static_cast<byte>(X2)] ^ m_rcsState->RoundKeys[14];
	Y7 = T0[static_cast<byte>(X7 >> 24)] ^ T1[static_cast<byte>(X0 >> 16)] ^ T2[static_cast<byte>(X2 >> 8)] ^ T3[static_cast<byte>(X3)] ^ m_rcsState->RoundKeys[15];

	kctr = 16;

	while (kctr != RNDCNT)
	{
		X0 = T0[static_cast<byte>(Y0 >> 24)] ^ T1[static_cast<byte>(Y1 >> 16)] ^ T2[static_cast<byte>(Y3 >> 8)] ^ T3[static_cast<byte>(Y4)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		X1 = T0[static_cast<byte>(Y1 >> 24)] ^ T1[static_cast<byte>(Y2 >> 16)] ^ T2[static_cast<byte>(Y4 >> 8)] ^ T3[static_cast<byte>(Y5)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		X2 = T0[static_cast<byte>(Y2 >> 24)] ^ T1[static_cast<byte>(Y3 >> 16)] ^ T2[static_cast<byte>(Y5 >> 8)] ^ T3[static_cast<byte>(Y6)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		X3 = T0[static_cast<byte>(Y3 >> 24)] ^ T1[static_cast<byte>(Y4 >> 16)] ^ T2[static_cast<byte>(Y6 >> 8)] ^ T3[static_cast<byte>(Y7)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		X4 = T0[static_cast<byte>(Y4 >> 24)] ^ T1[static_cast<byte>(Y5 >> 16)] ^ T2[static_cast<byte>(Y7 >> 8)] ^ T3[static_cast<byte>(Y0)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		X5 = T0[static_cast<byte>(Y5 >> 24)] ^ T1[static_cast<byte>(Y6 >> 16)] ^ T2[static_cast<byte>(Y0 >> 8)] ^ T3[static_cast<byte>(Y1)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		X6 = T0[static_cast<byte>(Y6 >> 24)] ^ T1[static_cast<byte>(Y7 >> 16)] ^ T2[static_cast<byte>(Y1 >> 8)] ^ T3[static_cast<byte>(Y2)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		X7 = T0[static_cast<byte>(Y7 >> 24)] ^ T1[static_cast<byte>(Y0 >> 16)] ^ T2[static_cast<byte>(Y2 >> 8)] ^ T3[static_cast<byte>(Y3)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		Y0 = T0[static_cast<byte>(X0 >> 24)] ^ T1[static_cast<byte>(X1 >> 16)] ^ T2[static_cast<byte>(X3 >> 8)] ^ T3[static_cast<byte>(X4)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		Y1 = T0[static_cast<byte>(X1 >> 24)] ^ T1[static_cast<byte>(X2 >> 16)] ^ T2[static_cast<byte>(X4 >> 8)] ^ T3[static_cast<byte>(X5)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		Y2 = T0[static_cast<byte>(X2 >> 24)] ^ T1[static_cast<byte>(X3 >> 16)] ^ T2[static_cast<byte>(X5 >> 8)] ^ T3[static_cast<byte>(X6)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		Y3 = T0[static_cast<byte>(X3 >> 24)] ^ T1[static_cast<byte>(X4 >> 16)] ^ T2[static_cast<byte>(X6 >> 8)] ^ T3[static_cast<byte>(X7)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		Y4 = T0[static_cast<byte>(X4 >> 24)] ^ T1[static_cast<byte>(X5 >> 16)] ^ T2[static_cast<byte>(X7 >> 8)] ^ T3[static_cast<byte>(X0)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		Y5 = T0[static_cast<byte>(X5 >> 24)] ^ T1[static_cast<byte>(X6 >> 16)] ^ T2[static_cast<byte>(X0 >> 8)] ^ T3[static_cast<byte>(X1)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		Y6 = T0[static_cast<byte>(X6 >> 24)] ^ T1[static_cast<byte>(X7 >> 16)] ^ T2[static_cast<byte>(X1 >> 8)] ^ T3[static_cast<byte>(X2)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
		Y7 = T0[static_cast<byte>(X7 >> 24)] ^ T1[static_cast<byte>(X0 >> 16)] ^ T2[static_cast<byte>(X2 >> 8)] ^ T3[static_cast<byte>(X3)] ^ m_rcsState->RoundKeys[kctr];
		++kctr;
	}

	// pre-load the s-box
#if defined(CEX_PREFETCH_RIJNDAEL_TABLES)
	PrefetchSbox();
#endif

	// final round
	Output[OutOffset] = static_cast<byte>(SBox[static_cast<byte>(Y0 >> 24)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 24));
	Output[OutOffset + 1] = static_cast<byte>(SBox[static_cast<byte>(Y1 >> 16)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 16));
	Output[OutOffset + 2] = static_cast<byte>(SBox[static_cast<byte>(Y3 >> 8)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 8));
	Output[OutOffset + 3] = static_cast<byte>(SBox[static_cast<byte>(Y4)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr]));
	++kctr;
	Output[OutOffset + 4] = static_cast<byte>(SBox[static_cast<byte>(Y1 >> 24)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 24));
	Output[OutOffset + 5] = static_cast<byte>(SBox[static_cast<byte>(Y2 >> 16)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 16));
	Output[OutOffset + 6] = static_cast<byte>(SBox[static_cast<byte>(Y4 >> 8)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 8));
	Output[OutOffset + 7] = static_cast<byte>(SBox[static_cast<byte>(Y5)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr]));
	++kctr;
	Output[OutOffset + 8] = static_cast<byte>(SBox[static_cast<byte>(Y2 >> 24)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 24));
	Output[OutOffset + 9] = static_cast<byte>(SBox[static_cast<byte>(Y3 >> 16)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 16));
	Output[OutOffset + 10] = static_cast<byte>(SBox[static_cast<byte>(Y5 >> 8)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 8));
	Output[OutOffset + 11] = static_cast<byte>(SBox[static_cast<byte>(Y6)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr]));
	++kctr;
	Output[OutOffset + 12] = static_cast<byte>(SBox[static_cast<byte>(Y3 >> 24)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 24));
	Output[OutOffset + 13] = static_cast<byte>(SBox[static_cast<byte>(Y4 >> 16)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 16));
	Output[OutOffset + 14] = static_cast<byte>(SBox[static_cast<byte>(Y6 >> 8)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 8));
	Output[OutOffset + 15] = static_cast<byte>(SBox[static_cast<byte>(Y7)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr]));
	++kctr;
	Output[OutOffset + 16] = static_cast<byte>(SBox[static_cast<byte>(Y4 >> 24)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 24));
	Output[OutOffset + 17] = static_cast<byte>(SBox[static_cast<byte>(Y5 >> 16)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 16));
	Output[OutOffset + 18] = static_cast<byte>(SBox[static_cast<byte>(Y7 >> 8)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 8));
	Output[OutOffset + 19] = static_cast<byte>(SBox[static_cast<byte>(Y0)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr]));
	++kctr;
	Output[OutOffset + 20] = static_cast<byte>(SBox[static_cast<byte>(Y5 >> 24)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 24));
	Output[OutOffset + 21] = static_cast<byte>(SBox[static_cast<byte>(Y6 >> 16)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 16));
	Output[OutOffset + 22] = static_cast<byte>(SBox[static_cast<byte>(Y0 >> 8)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 8));
	Output[OutOffset + 23] = static_cast<byte>(SBox[static_cast<byte>(Y1)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr]));
	++kctr;
	Output[OutOffset + 24] = static_cast<byte>(SBox[static_cast<byte>(Y6 >> 24)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 24));
	Output[OutOffset + 25] = static_cast<byte>(SBox[static_cast<byte>(Y7 >> 16)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 16));
	Output[OutOffset + 26] = static_cast<byte>(SBox[static_cast<byte>(Y1 >> 8)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 8));
	Output[OutOffset + 27] = static_cast<byte>(SBox[static_cast<byte>(Y2)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr]));
	++kctr;
	Output[OutOffset + 28] = static_cast<byte>(SBox[static_cast<byte>(Y7 >> 24)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 24));
	Output[OutOffset + 29] = static_cast<byte>(SBox[static_cast<byte>(Y0 >> 16)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 16));
	Output[OutOffset + 30] = static_cast<byte>(SBox[static_cast<byte>(Y2 >> 8)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr] >> 8));
	Output[OutOffset + 31] = static_cast<byte>(SBox[static_cast<byte>(Y3)] ^ static_cast<byte>(m_rcsState->RoundKeys[kctr]));
}

#else

void RCS::Transform256(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset)
{
	SecureVector<byte> state(BLOCK_SIZE, 0x00);
	size_t i;

	MemoryTools::Copy(Input, InOffset, state, 0, BLOCK_SIZE);
	KeyAddition(state, m_rcsState->RoundKeys, 0);

	// pre-load the s-box into L1 cache
#if defined(CEX_PREFETCH_RIJNDAEL_TABLES)
	PrefetchSbox();
#endif

	for (i = 1; i < m_rcsState->Rounds; ++i)
	{
		Substitution(state);
		ShiftRows256(state);
		MixColumns(state);
		KeyAddition(state, m_rcsState->RoundKeys, (i << 3UL));
	}

	Substitution(state);
	ShiftRows256(state);
	KeyAddition(state, m_rcsState->RoundKeys, static_cast<size_t>(m_rcsState->Rounds) << 3UL);

	MemoryTools::Copy(state, 0, Output, OutOffset, BLOCK_SIZE);
}

#endif

void RCS::Transform1024(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset)
{
	Transform256(Input, InOffset, Output, OutOffset);
	Transform256(Input, InOffset + 32, Output, OutOffset + 32);
	Transform256(Input, InOffset + 64, Output, OutOffset + 64);
	Transform256(Input, InOffset + 96, Output, OutOffset + 96);
}

void RCS::Transform2048(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset)
{
	Transform1024(Input, InOffset, Output, OutOffset);
	Transform1024(Input, InOffset + 128, Output, OutOffset + 128);
}

void RCS::Transform4096(const std::vector<byte> &Input, size_t InOffset, std::vector<byte> &Output, size_t OutOffset)
{
	Transform2048(Input, InOffset, Output, OutOffset);
	Transform2048(Input, InOffset + 256, Output, OutOffset + 256);
}

NAMESPACE_STREAMEND
