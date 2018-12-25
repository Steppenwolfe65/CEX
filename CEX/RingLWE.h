// The GPL version 3 License (GPLv3)
// 
// Copyright (c) 2018 vtdev.com
// This file is part of the CEX Cryptographic library.
// 
// This program is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef CEX_RINGLWE_H
#define CEX_RINGLWE_H

#include "CexDomain.h"
#include "AsymmetricKey.h"
#include "AsymmetricKeyPair.h"
#include "IAsymmetricCipher.h"
#include "RLWEParameters.h"

NAMESPACE_RINGLWE

using Key::Asymmetric::AsymmetricKey;
using Key::Asymmetric::AsymmetricKeyPair;
using Enumeration::RLWEParameters;

/// <summary>
/// An implementation of the Ring Learning With Errors asymmetric cipher (RingLWE)
/// </summary> 
/// 
/// <example>
/// <description>Key generation:</description>
/// <code>
/// RingLWE acpr(RLWEParameters::RLWES1Q12289N1024);
/// IAsymmetricKeyPair* kp = acpr.Generate();
/// 
/// // serialize the public key
/// RLWEPublicKey* pubK1 = (RLWEPublicKey*)kp->PublicKey();
/// std::vector&lt;byte&gt; pk = pubK1->ToBytes();
/// </code>
///
/// <description>Encryption:</description>
/// <code>
/// create the shared secret
/// std::vector&lt;byte&gt; cpt(0);
/// std::vector&lt;byte&gt; sec(32);
///
/// // initialize the cipher
/// RingLWE acpr(RLWEParameters::RLWES1Q12289N1024);
/// cpr.Initialize(PublicKey);
/// // encrypt the secret
/// status = cpr.Encrypt(cpt, sec);
/// </code>
///
/// <description>Decryption:</description>
/// <code>
/// std::vector&lt;byte&gt; sec(32);
/// bool status;
///
/// // initialize the cipher
/// RingLWE acpr(RLWEParameters::RLWES1Q12289N1024);
/// cpr.Initialize(PrivateKey);
/// // decrypt the secret, status returns authentication outcome, false for failure
/// status = cpr.Decrypt(cpt, sec);
/// </code>
/// </example>
/// 
/// <remarks>
/// <description>Implementation Notes:</description>
/// <para>Ring learning with errors (RLWE) is the Learning With Errors problem (a generalization of the parity learning problem), specialized to polynomial rings over finite fields. \n
/// An important feature of the ring LWE problem is that the solution may be reducible to the NP-Hard Shortest Vector Problem (SVP) in a Lattice. \n
/// This makes RingLWE a strong asymmetric cipher and resistant to currently known attack methods that could use quantum computers.</para>
///
/// <para>This implementation of RingLWE uses the NewHope reconcilliation method, whereby the shared seed is generated by the receiver,
/// and encrypted using a combination of the senders public key and the receivers public key, and then shared between hosts. \n
/// That seed is compressed with a Keccak digest and used to key the GCM AEAD mode which encrypts and authenticates the message.</para>
/// 
/// <list type="bullet">
/// <item><description>The ciphers operating mode (encryption/decryption) is determined by the IAsymmetricKey key-type used to Initialize the cipher (AsymmetricKeyTypes: RLWEPublicKey, or RLWEPublicKey), Public for encryption, Private for Decryption.</description></item>
/// <item><description>The Q12289/N1024 parameter set is the default cipher configuration; an experimental Q12289/N2048 is also implemented</description></item>
/// <item><description>The primary Prng is set through the constructor, as either an prng type-name (default BCR-AES256), which instantiates the function internally, or a pointer to a perisitant external instance of a Prng</description></item>
/// <item><description>The message digest used to condition the seed bytes is set automatically; Keccak512 for standard ciphers, Keccak1024 for extended ciphers</description></item>
/// <item><description>The secondary prng used to generate the public key (BCR), is an AES128/CTR-BE construction, (changed from Shake128 in the new hope version)</description></item>
/// <item><description>The message is authenticated using GCM, and throws CryptoAuthenticationFailure on decryption authentication failure</description></item>
/// </list>
/// 
/// <description>Guiding Publications:</description>
/// <list type="number">
/// <item><description>Nist PQ Round 1 submission: <a href="https://csrc.nist.gov/Projects/Post-Quantum-Cryptography/Round-1-Submissions">New Hope</a>.</description></item>
/// <item><description>The NewHope website: <a href="https://newhopecrypto.org/">key exchange</a>.</description></item>
/// <item><description>The NewHope: <a href="https://eprint.iacr.org/2015/1092">key exchange</a> from the ring learning with errors problem.</description></item>
/// <item><description>A Simple, Provably <a href="http://eprint.iacr.org/2012/688.pdf">Secure Key Exchange</a> Scheme Based on the Learning with Errors Problem.</description></item>
/// </list>
/// </remarks>
class RingLWE final : public IAsymmetricCipher
{
private:

	static const std::string CLASS_NAME;

	bool m_destroyEngine;
	std::vector<byte> m_domainKey;
	bool m_isDestroyed;
	bool m_isEncryption;
	bool m_isInitialized;
	std::unique_ptr<AsymmetricKey> m_privateKey;
	std::unique_ptr<AsymmetricKey> m_publicKey;
	RLWEParameters m_rlweParameters;
	std::unique_ptr<IPrng> m_rndGenerator;

public:

	//~~~Constructor~~~//

	/// <summary>
	/// Copy constructor: copy is restricted, this function has been deleted
	/// </summary>
	RingLWE(const RingLWE&) = delete;

	/// <summary>
	/// Copy operator: copy is restricted, this function has been deleted
	/// </summary>
	RingLWE& operator=(const RingLWE&) = delete;

	/// <summary>
	/// Instantiate the cipher with auto-initialized prng and digest functions
	/// </summary>
	///
	/// <param name="Parameters">The parameter set enumeration name</param>
	/// <param name="PrngType">The seed prng function type; the default is the BCR generator</param>
	/// 
	/// <exception cref="Exception::CryptoAsymmetricException">Thrown if an invalid prng type, or parameter set is specified</exception>
	RingLWE(RLWEParameters Parameters = RLWEParameters::RLWES1Q12289N1024, Prngs PrngType = Prngs::BCR);

	/// <summary>
	/// Constructor: instantiate this class using an external Prng instance
	/// </summary>
	///
	/// <param name="Parameters">The parameter set enumeration name</param>
	/// <param name="Prng">A pointer to the seed Prng function</param>
	/// 
	/// <exception cref="Exception::CryptoAsymmetricException">Thrown if an invalid prng, or parameter set is specified</exception>
	RingLWE(RLWEParameters Parameters, IPrng* Prng);

	/// <summary>
	/// Destructor: finalize this class
	/// </summary>
	~RingLWE() override;

	//~~~Accessors~~~//

	/// <summary>
	/// Read/Write: Reads or Sets the Domain Key used as a customization string by cSHAKE to generate the shared secret.
	/// <para>Changing this code will create a unique distribution of the cipher.
	/// The domain key can be used as a secondary secret shared between hosts in an authenticated domain.
	/// The key is used as a customization string to pre-initialize a custom SHAKE function, that conditions the SharedSecret in Encapsulation/Decapsulation.
	/// For best security, the key should be random, secret, and shared only between hosts within a secure domain.
	/// This property is used by the Shared Trust Model secure communications protocol.</para>
	/// </summary>
	std::vector<byte> &DomainKey();

	/// <summary>
	/// Read Only: The cipher type-name
	/// </summary>
	const AsymmetricEngines Enumeral() override;

	/// <summary>
	/// Read Only: The cipher is initialized for encryption
	/// </summary>
	const bool IsEncryption() override;

	/// <summary>
	/// Read Only: The cipher has been initialized with a key
	/// </summary>
	const bool IsInitialized() override;

	/// <summary>
	/// Read Only: The cipher and parameter-set formal names
	/// </summary>
	const std::string Name() override;

	/// <summary>
	/// Read Only: The ciphers parameters enumeration name
	/// </summary>
	const RLWEParameters Parameters();

	//~~~Public Functions~~~//

	/// <summary>
	/// Decrypt a ciphertext and return the shared secret
	/// </summary>
	/// 
	/// <param name="CipherText">The input cipher-text</param>
	/// <param name="SharedSecret">The shared secret key</param>
	/// 
	/// <returns>Returns true if decryption is sucesssful</returns>
	bool Decapsulate(const std::vector<byte> &CipherText, std::vector<byte> &SharedSecret) override;

	/// <summary>
	/// Generate a shared secret and ciphertext
	/// </summary>
	/// 
	/// <param name="CipherText">The output cipher-text</param>
	/// <param name="SharedSecret">The shared secret key</param>
	void Encapsulate(std::vector<byte> &CipherText, std::vector<byte> &SharedSecret) override;

	/// <summary>
	/// Generate a public/private key-pair
	/// </summary>
	/// 
	/// <returns>A public/private key pair</returns>
	/// 
	/// <exception cref="Exception::CryptoAsymmetricException">Thrown if the key generation call fails</exception>
	AsymmetricKeyPair* Generate() override;

	/// <summary>
	/// Initialize the cipher
	/// </summary>
	/// 
	/// <param name="Key">The asymmetric public or private key</param>
	/// 
	/// <exception cref="Exception::CryptoAsymmetricException">Fails on invalid key or configuration error</exception>
	void Initialize(AsymmetricKey* Key) override;

private:

	int32_t Verify(const std::vector<byte> &A, const std::vector<byte> &B, size_t Length);
};

NAMESPACE_RINGLWEEND
#endif
