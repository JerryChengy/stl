#ifndef UNITY_PREFIX_H
#define UNITY_PREFIX_H 
#pragma warning (disable:4819)
#if !defined(__MWERKS) && !defined(GEKKO) && !defined(SN_TARGET_PS3) && !defined(SN_TARGET_PS3_SPU)

// File included at top of all our source files.
// Mostly for visual studio, who is dumb enough to require
// manual inclusion for precompiled headers to work

// copied from PrefixConfigure.h
#ifndef TARGET_IPHONE_SIMULATOR
#define TARGET_IPHONE_SIMULATOR 0
#endif

// Platform dependent configuration before we do anything.
// Can't depend on unity defines because they are not there yet!
#ifdef _MSC_VER
// Visual Studio
#include "VisualStudioPrefix.h"
#elif defined(ANDROID) && i386
#include "AndroidPrefix.h"
#elif defined(linux) || defined(__linux__)
#include "LinuxPrefix.h"
#elif defined(__QNXNTO__)
#include "BB10Prefix.h"
#elif defined(__APPLE__)
#include "OSXPrefix.h"
#else
// TODO
#endif

// Configure Unity
#include "PrefixConfigure.h"
#include "UnityString.h"
#include "LogAssert.h"

#if defined(__native_client__)
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#ifndef __GLIBC__
#include <ieeefp.h>
#endif
#ifdef __GLIBC__
#include <stdint.h>
#endif
#ifdef __cplusplus
#include <memory>
#endif
#endif

#ifdef __cplusplus
#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <limits.h>
#include <functional>
#if UNITY_FLASH || UNITY_WEBGL
#include <malloc.h>
#include <cstddef>
#endif
#include <stdlib.h>
#include <cmath>

// Platform dependent bits again
#ifdef _MSC_VER
// Visual Studio
#include "VisualStudioPostPrefix.h"
#elif UNITY_IPHONE
#include "iPhonePostPrefix.h"
#endif


#endif // __cplusplus

#elif defined(GEKKO)

#include "WiiPrefix.h"

// Configure Unity
#include "Configuration/PrefixConfigure.h"

#ifdef __cplusplus

// STD
#include <algorithm>
#include <cmath>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <stack>
#include <limits>
#include <list>
#include <string>
#include <iterator>
#include <iostream>
#include <functional>
#include <cstring>

#endif // __cplusplus

#include "WiiPostPrefix.h"

// frequently used
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/Utility.h"

#elif defined(SN_TARGET_PS3_SPU)

#define GAMERELEASE 1
#define DEBUGMODE 0
#define UNITY_RELEASE 1
#define WEBPLUG	0

typedef signed int SInt32;
typedef unsigned int UInt32;
typedef signed short SInt16;
typedef unsigned short UInt16;
typedef unsigned char UInt8;
typedef signed char SInt8;
typedef unsigned long long UInt64;
typedef signed long long SInt64;

#define printf_console spu_printf
#define DebugBreak() __asm__ volatile ("stopd 0,0,1")
#define Assert(f) if(!(f)) DebugBreak()
#define CELLCALL(f) {										\
	UInt32 lResult;											\
	lResult = (f);											\
	if(CELL_OK != lResult ) {								\
	printf_console("[SPU] %s returned %p in %s(%d) : \n", #f, (void*)lResult, __FILE__, __LINE__ );	\
	DebugBreak();						\
	}														\
}
#include "Configuration/PrefixConfigure.h"

#elif defined(SN_TARGET_PS3)

#include "PS3Prefix.h"

// Configure Unity
#include "Configuration/PrefixConfigure.h"

#ifdef __cplusplus

// STD
#include <algorithm>
#include <cmath>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <stack>
#include <limits>
#include <list>
#include <string>
#include <iterator>
#include <iostream>
#include <functional>

#endif // __cplusplus

#include "PS3PostPrefix.h"

// frequently used
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Math/FloatConversion.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Utilities/Utility.h"

#endif

#endif
