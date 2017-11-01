#include "UnityPrefix.h"
#include "DateTime.h"
#include "SwapEndianBytes.h"


DateTime::DateTime ()
{
	highSeconds = 0;
	lowSeconds = 0;
	fraction = 0;
}

bool operator < (const DateTime& d0, const DateTime& d1)
{
	if (d0.highSeconds < d1.highSeconds)
		return true;
	else if (d0.highSeconds > d1.highSeconds)
		return false;

	if (d0.lowSeconds < d1.lowSeconds)
		return true;
	else if (d0.lowSeconds > d1.lowSeconds)
		return false;

	return d0.fraction < d1.fraction;
}

bool operator == (const DateTime& d0, const DateTime& d1)
{
	if (d0.highSeconds != d1.highSeconds)
		return false;

	if (d0.lowSeconds != d1.lowSeconds)
		return false;

	return d0.fraction == d1.fraction;
}


void ByteSwapDateTime (DateTime& dateTime)
{
	SwapEndianBytes(dateTime.highSeconds);
	SwapEndianBytes(dateTime.fraction);
	SwapEndianBytes(dateTime.lowSeconds);
}
