#ifndef SWAPENDIANBYTES_H
#define SWAPENDIANBYTES_H

inline void SwapEndianBytes (char&) { }
inline void SwapEndianBytes (unsigned char&) { }
inline void SwapEndianBytes (bool&) { }
inline void SwapEndianBytes (signed char&) {  }

#if UNITY_WII
inline void SwapEndianBytes (register UInt16& i)
{
	__asm {
		lhbrx r3, 0, i
			sth r3, 0(i)
	}
}
#else
inline void SwapEndianBytes (UInt16& i) { i = static_cast<UInt16>((i << 8) | (i >> 8)); }
#endif

inline void SwapEndianBytes (SInt16& i) { SwapEndianBytes (reinterpret_cast<UInt16&> (i)); }

#if UNITY_WII
inline void SwapEndianBytes (register UInt32& i)
{
	__asm {
		lwbrx r3, 0, i
			stw r3, 0(i)
	}
}
#else
inline void SwapEndianBytes (UInt32& i)   { i = (i >> 24) | ((i >> 8) & 0x0000ff00) | ((i << 8) & 0x00ff0000) | (i << 24); }
#endif

inline void SwapEndianBytes (SInt32& i)   { SwapEndianBytes (reinterpret_cast<UInt32&> (i)); }
inline void SwapEndianBytes (float& i)    { SwapEndianBytes (reinterpret_cast<UInt32&> (i)); }

inline void SwapEndianBytes (UInt64& i)
{
#if UNITY_WII
	register unsigned int temp1, temp2;
	register UInt32* p0 = reinterpret_cast<UInt32*>( &i );
	register UInt32* p1 = reinterpret_cast<UInt32*>( &i ) + 1;
	__asm {
		lwbrx temp1, 0, p0
			lwbrx temp2, 0, p1
			stw temp1, 0(p1)
			stw temp2, 0(p0)		
	}
#else
	UInt32* p = reinterpret_cast<UInt32*>(&i);
	UInt32 u = (p[0] >> 24) | (p[0] << 24) | ((p[0] & 0x00ff0000) >> 8) | ((p[0] & 0x0000ff00) << 8);
	p[0] = (p[1] >> 24) | (p[1] << 24) | ((p[1] & 0x00ff0000) >> 8) | ((p[1] & 0x0000ff00) << 8);
	p[1] = u;
#endif
}

inline void SwapEndianBytes (SInt64& i) { SwapEndianBytes (reinterpret_cast<UInt64&> (i)); }
inline void SwapEndianBytes (double& i) { SwapEndianBytes (reinterpret_cast<UInt64&> (i)); }

#if UNITY_64 && UNITY_OSX
inline void SwapEndianBytes (size_t &i)   { SwapEndianBytes (reinterpret_cast<UInt64&> (i)); }
#endif

inline bool IsBigEndian ()
{
#if UNITY_BIG_ENDIAN
	return true;
#else
	return false;
#endif
}

inline bool IsLittleEndian ()
{
	return !IsBigEndian();
}

#if UNITY_BIG_ENDIAN
#define SwapEndianBytesBigToNative(x) 
#define SwapEndianBytesLittleToNative(x) SwapEndianBytes(x)
#define SwapEndianBytesNativeToLittle(x) SwapEndianBytes(x)
#elif UNITY_LITTLE_ENDIAN
#define SwapEndianBytesBigToNative(x) SwapEndianBytes(x)
#define SwapEndianBytesLittleToNative(x) 
#define SwapEndianBytesNativeToLittle(x)
#endif

#endif
