#ifndef BITUTILITY_H
#define BITUTILITY_H

#include <limits.h>

// index of the most significant bit in the mask

const char gHighestBitLut[] = {-1,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3};

inline int HighestBit(UInt32 mask)
{
#ifdef SN_TARGET_PS3
	return 31 - __cntlzw(mask);
#else
	int base = 0;

	if ( mask & 0xffff0000 )
	{
		base = 16;
		mask >>= 16;
	}
	if ( mask & 0x0000ff00 )
	{
		base += 8;
		mask >>= 8;
	}
	if ( mask & 0x000000f0 )
	{
		base += 4;
		mask >>= 4;
	}

	return base + gHighestBitLut[ mask ];
#endif
}


// index of the least significant bit in the mask

const char gLowestBitLut[] = {-1,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0};
inline int LowestBit(UInt32 mask)
{
	//AssertIf (mask == 0);
	
	int base = 0;

	if ( !(mask & 0xffff) )
	{
		base = 16;
		mask >>= 16;
	}
	if ( !(mask & 0x00ff) )
	{
		base += 8;
		mask >>= 8;
	}
	if ( !(mask & 0x000f) )
	{
		base += 4;
		mask >>= 4;
	}

	return base + gLowestBitLut[ mask & 15 ];
}

// can be optimized later
inline int AnyBitFromMask (UInt32 mask)
{
	return HighestBit (mask);
}

// index of the first consecutiveBitCount enabled bits
// -1 if not available
inline int LowestBitConsecutive (UInt32 bitMask, int consecutiveBitCount)
{
    UInt32 tempBitMask = bitMask;
    int i;
    for (i=1;i<consecutiveBitCount;i++)
        tempBitMask &= bitMask >> i;
    
    if (!tempBitMask)
        return -1;
    else
        return LowestBit (tempBitMask);
}
/*int LowestBitConsecutive ( u_int value, u_int consecutiveBitCount )
{
	u_int mask = (1 << consecutiveBitCount) - 1;
	u_int notValue = value ^ 0xffffffff;
	u_int workingMask = mask;
	u_int prevMask = 0;
	int match = notValue & workingMask;
	u_int shift = 1;
	while ( (match != 0) && (prevMask < workingMask) )
	{
		shift = 2*u_int(match & -match);
		prevMask = workingMask;
		workingMask = mask * shift;
		match = notValue & workingMask;
	}
	if ( prevMask < workingMask )
	{
		return LowestBit( shift );
	}
	else
	{
		return -1;
	}
}*/

// number of set bits in the 32 bit mask
inline int BitsInMask (UInt32 v)
{
	// From http://www-graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	// This variant about 30% faster on 360 than what was here before.
	v = v - ((v >> 1) & 0x55555555);
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24;
}

// number of set bits in the 64 bit mask
inline int BitsInMask64 (UInt64 v)
{
	// From http://www-graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	v = v - ((v >> 1) & (UInt64)~(UInt64)0/3);
	v = (v & (UInt64)~(UInt64)0/15*3) + ((v >> 2) & (UInt64)~(UInt64)0/15*3);
	v = (v + (v >> 4)) & (UInt64)~(UInt64)0/255*15;
	return (UInt64)(v * ((UInt64)~(UInt64)0/255)) >> (sizeof(UInt64) - 1) * CHAR_BIT;
}

// reverse bit order
inline void ReverseBits(UInt32& mask)
{
	mask = ((mask >>  1) & 0x55555555) | ((mask <<  1) & 0xaaaaaaaa);
	mask = ((mask >>  2) & 0x33333333) | ((mask <<  2) & 0xcccccccc);
	mask = ((mask >>  4) & 0x0f0f0f0f) | ((mask <<  4) & 0xf0f0f0f0);
	mask = ((mask >>  8) & 0x00ff00ff) | ((mask <<  8) & 0xff00ff00);
	mask = ((mask >> 16) & 0x0000ffff) | ((mask << 16) & 0xffff0000) ;
}

// is value a power-of-two
inline bool IsPowerOfTwo(UInt32 mask)
{
	return (mask & (mask-1)) == 0;
}

// return the next power-of-two of a 32bit number
inline UInt32 NextPowerOfTwo(UInt32 v)
{
	v -= 1;
	v |= v >> 16;
	v |= v >> 8;
	v |= v >> 4;
	v |= v >> 2;
	v |= v >> 1;
	return v + 1;
}

// return the closest power-of-two of a 32bit number
inline UInt32 ClosestPowerOfTwo(UInt32 v)
{
	UInt32 nextPower = NextPowerOfTwo (v);
	UInt32 prevPower = nextPower >> 1;
	if (v - prevPower < nextPower - v)
		return prevPower;
	else
		return nextPower;
}

inline UInt32 ToggleBit (UInt32 bitfield, int index)
{
	//AssertIf (index < 0 || index >= 32);
	return bitfield ^ (1 << index);
}

// Template argument must be a power of 2
template<int n>
struct StaticLog2
{
	static const int value = StaticLog2<n/2>::value + 1;
};

template<>
struct StaticLog2<1>
{
	static const int value = 0;
};

#endif
