#pragma once

#include "SerializeUtility.h"

struct DateTime
{
	// We are wasting memory and serialization
	// in assetdatabase is wrong because of the alignment!
	// We do this change in 1.5 because it will force a rebuild of all assets
	UInt16              highSeconds;
	UInt16              fraction;
	UInt32              lowSeconds;
	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (DateTime)

	DateTime ();

	friend bool operator < (const DateTime& d0, const DateTime& d1);
	friend bool operator == (const DateTime& d0, const DateTime& d1);
	friend bool operator != (const DateTime& d0, const DateTime& d1) { return !(d0 == d1); }
};

template<class TransferFunction>
void DateTime::Transfer (TransferFunction& transfer)
{
	TRANSFER (highSeconds);
	TRANSFER (fraction);
	TRANSFER (lowSeconds);
}

void ByteSwapDateTime (DateTime& dateTime);
