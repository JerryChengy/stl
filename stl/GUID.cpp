#include "UnityPrefix.h"
#include "GUID.h"
#include "LogAssert.h"
#include "StaticAssert.h"
#include <string>

#if UNITY_IPHONE
#include <CoreFoundation/CFUUID.h>
#endif

#if UNITY_WIN
#include <ObjBase.h>
#endif

#if UNITY_XENON
#include <comdecl.h>
#include <time.h>
#endif

using namespace std;

#if  UNITY_ANDROID
#include <fcntl.h>
#include <unistd.h>
#elif UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
#include <fcntl.h>
#include <sys/time.h>
#endif

#if UNITY_PS3
#include <sys/random_number.h>
#endif

#if UNITY_HAVE_GUID_INIT
void UnityGUID::Init ()
{
#if UNITY_OSX || UNITY_IPHONE
	CompileTimeAssert (sizeof (CFUUIDBytes) == 16, "GIUD shoud be 16 bytes");
#elif UNITY_WIN || UNITY_XENON
	CompileTimeAssert (sizeof (GUID) == 16, "GIUD shoud be 16 bytes");
#endif


#if UNITY_OSX || UNITY_IPHONE

	CFUUIDRef myUUID;
	myUUID = CFUUIDCreate (kCFAllocatorDefault);
	CFUUIDBytes bytes = CFUUIDGetUUIDBytes (myUUID);
	memcpy (data, &bytes, sizeof (CFUUIDBytes));
	CFRelease (myUUID);

#elif UNITY_ANDROID || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN

#pragma message("Android GUID/UUID creation NOT tested!")
	{
		unsigned char bytes[16];

		struct timeval tv ;
		gettimeofday(&tv, 0) ;
		int fd = open("/dev/urandom", O_RDONLY);
		if (fd == -1)
			fd = open("/dev/random", O_RDONLY | O_NONBLOCK) ;

		srand((getpid() << 16) ^ getuid() ^ tv.tv_sec ^ tv.tv_usec) ;

		// Crank the random number generator a few times
		gettimeofday(&tv, 0) ;
		for (int i = (tv.tv_sec ^ tv.tv_usec) & 0x1F; i > 0; i--) rand() ;

		int n = 16 ;
		int lose_counter = 0 ;
		unsigned char *cp = (unsigned char *) bytes ;
		while (n > 0 && fd != -1)
		{
			int i = read(fd, cp, n) ;
			if (i <= 0)
			{
				if (lose_counter++ > 16) break ;
				continue ;
			}
			n -= i ;
			cp += i ;
			lose_counter = 0 ;
		}

		if (fd != -1)
			close(fd);

		// We do this all the time, but this is the only source of
		// randomness if /dev/random/urandom is out to lunch.
		for (unsigned int i = 0; i<16; ++i)
			bytes[i] ^= (rand() >> 7) & 0xFF ;

		// clock_seq is bytes #8 and #9
		uint16_t clock_seq = (uint16_t(bytes[8]) << 8) + bytes[9] ;
		clock_seq = (clock_seq & 0x3FFF) | 0x8000 ;
		bytes[8] = clock_seq >> 8 ;
		bytes[9] = clock_seq & 0xFF ;

		// time_hi_and_version is bytes #6 and #7
		uint16_t time_hi_and_version = (uint16_t(bytes[6]) << 8) + bytes[7] ;
		time_hi_and_version = (time_hi_and_version & 0x0FFF) | 0x4000 ;
		bytes[6] = time_hi_and_version >> 8 ;
		bytes[7] = time_hi_and_version & 0xFF ;

		memcpy (data, &bytes, sizeof (data));
	}
#elif UNITY_WIN

	GUID guid;
	::CoCreateGuid( &guid );
	memcpy( data, &guid, sizeof(guid) );

#elif UNITY_PS3

	sys_get_random_number( &data, sizeof(UnityGUID) );

#elif UNITY_XENON

	// RFC 4122, section 4.4 (Algorithms for Creating a UUID from Truly Random or Pseudo-Random Numbers)
	// http://www.ietf.org/rfc/rfc4122.txt
	GUID guid;
	unsigned char* bytes = (unsigned char*)&guid;

	srand( time(NULL) );
	for (unsigned int i = 0; i<16; ++i)
		bytes[i] ^= (rand() >> 7) & 0xFF;

	// V4
	guid.Data3 &= 0x0fff;
	guid.Data3 |= (4 << 12);
	guid.Data4[0] &= 0x3f;
	guid.Data4[0] |= 0x80;

	memcpy( data, &guid, sizeof(guid) );

#else

	ErrorString("GUID::Init() not implemented.");

#endif
}
#endif

bool CompareGUIDStringLess (const UnityGUID& lhsGUID, const UnityGUID& rhsGUID)
{
	char lhs[32];
	char rhs[32];
	GUIDToString(lhsGUID, lhs);
	GUIDToString(rhsGUID, rhs);

	for (int i=0;i<32;i++)
	{
		if (lhs[i] != rhs[i])
			return lhs[i] < rhs[i];
	}

	return false;
}

const char kHexToLiteral[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

string GUIDToString(const UnityGUID& guid)
{
	char name[kGUIDStringLength+1];
	GUIDToString (guid, name);
	name[kGUIDStringLength] = '\0';
	return name;
}

void GUIDToString(const UnityGUID& guid, char* name)
{
	for (int i=0;i<4;i++)
	{
		for (int j=8;j--;)
		{
			UInt32 cur = guid.data[i];
			cur >>= (j * 4);
			cur &= 0xF;
			name[i * 8 + j] = kHexToLiteral[cur];
		}
	}
}

UnityGUID StringToGUID (const std::string& guidString)
{
	return StringToGUID(guidString.c_str(), guidString.size());
}

UnityGUID StringToGUID (const char* guidString, size_t length)
{
	if (length != kGUIDStringLength)
		return UnityGUID ();

	// Initialize hex char to int table
	static char s_LiteralToHex[255];
	static bool s_IsInitialized = false;
	if (!s_IsInitialized)
	{
		for (int i=0;i<255;i++)
			s_LiteralToHex[i] = -1;
		s_LiteralToHex['0'] = 0;
		s_LiteralToHex['1'] = 1;
		s_LiteralToHex['2'] = 2;
		s_LiteralToHex['3'] = 3;
		s_LiteralToHex['4'] = 4;
		s_LiteralToHex['5'] = 5;
		s_LiteralToHex['6'] = 6;
		s_LiteralToHex['7'] = 7;
		s_LiteralToHex['8'] = 8;
		s_LiteralToHex['9'] = 9;
		s_LiteralToHex['a'] = 10;
		s_LiteralToHex['b'] = 11;
		s_LiteralToHex['c'] = 12;
		s_LiteralToHex['d'] = 13;
		s_LiteralToHex['e'] = 14;
		s_LiteralToHex['f'] = 15;
		s_IsInitialized = true;
	}

	// Convert every hex char into an int [0...16]
	int hex[kGUIDStringLength];
	for (int i=0;i<kGUIDStringLength;i++)
		hex[i] = s_LiteralToHex[guidString[i]];

	UnityGUID guid;
	for (int i=0;i<4;i++)
	{
		UInt32 cur = 0;
		for (int j=8;j--;)
		{
			int curHex = hex[i * 8 + j];
			if (curHex == -1)
				return UnityGUID ();

			cur |= (curHex << (j * 4));
		}
		guid.data[i] = cur;
	}
	return guid;
}
