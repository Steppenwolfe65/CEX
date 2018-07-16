﻿#ifndef CEXTEST_RIJNDAELTEST_H
#define CEXTEST_RIJNDAELTEST_H

#include "ITest.h"

namespace Test
{
    /// <summary>
	/// Rijndael implementation vector comparison tests.
    /// <para>est vectors derived from Bouncy Castle RijndaelTest.cs and the Nessie unverified vectors:
    /// <see href="https://www.cosic.esat.kuleuven.be/nessie/testvectors/bc/rijndael/Rijndael-256-256.unverified.test-vectors"/>
    /// Tests supported block sizes of 16 and 32 bytes.</para>
    /// </summary>
    class RijndaelTest final : public ITest
    {
	private:

		static const std::string DESCRIPTION;
		static const std::string FAILURE;
		static const std::string SUCCESS;

        std::vector<std::vector<byte>> m_cipherText;
        std::vector<std::vector<byte>> m_keys;
        std::vector<std::vector<byte>> m_plainText;
		TestEventHandler m_progressEvent;

    public:

		/// <summary>
		/// Compares known answer Rijndael vectors for equality
		/// </summary>
		RijndaelTest();

		/// <summary>
		/// Destructor
		/// </summary>
		~RijndaelTest();

		/// <summary>
		/// Get: The test description
		/// </summary>
		const std::string Description() override;

		/// <summary>
		/// Progress return event callback
		/// </summary>
		TestEventHandler &Progress() override;

		/// <summary>
		/// Start the tests
		/// </summary>
		std::string Run() override;
        
    private:

		void CompareOutput(std::vector<byte> &Key, std::vector<byte> &Input, std::vector<byte> &Output);
		void Initialize();
		void OnProgress(std::string Data);
    };
}

#endif

