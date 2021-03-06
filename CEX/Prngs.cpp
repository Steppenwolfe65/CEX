#include "Prngs.h"

NAMESPACE_ENUMERATION

std::string PrngConvert::ToName(Prngs Enumeral)
{
	std::string name("");

	switch (Enumeral)
	{
		case Prngs::BCR:
			name = std::string("BCR");
			break;
		case Prngs::CSR:
			name = std::string("CSR");
			break;
		case Prngs::CSRC512:
			name = std::string("CSRC512");
			break;
		case Prngs::CSRC1024:
			name = std::string("CSRC1024");
			break;
		case Prngs::HCR:
			name = std::string("HCR");
			break;
		case Prngs::HCRS512:
			name = std::string("HCRS512");
			break;
		default:
			name = std::string("None");
			break;
	}

	return name;
}

Prngs PrngConvert::FromName(std::string &Name)
{
	Prngs tname;

	if (Name == std::string("BCR"))
	{
		tname = Prngs::BCR;
	}
	else if (Name == std::string("CSR"))
	{
		tname = Prngs::CSR;
	}
	else if (Name == std::string("CSRC512"))
	{
		tname = Prngs::CSRC512;
	}
	else if (Name == std::string("CSRC1024"))
	{
		tname = Prngs::CSRC1024;
	}
	else if (Name == std::string("HCR"))
	{
		tname = Prngs::HCR;
	}
	else if (Name == std::string("HCRS512"))
	{
		tname = Prngs::HCRS512;
	}
	else
	{
		tname = Prngs::None;
	}

	return tname;
}

NAMESPACE_ENUMERATIONEND