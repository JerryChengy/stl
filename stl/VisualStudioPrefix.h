
// configure Unity
// UNITY_RELEASE is true for all non-debug builds
// e.g. everything built by TeamCity
#ifdef _DEBUG
#define DEBUGMODE 1
#ifdef UNITY_RELEASE
#undef UNITY_RELEASE
#endif
#define UNITY_RELEASE 0
#else
#if !defined(DEBUGMODE)
#if !GAMERELEASE
#define DEBUGMODE 1
#else
#define DEBUGMODE 0
#endif
#endif
#ifdef UNITY_RELEASE
#undef UNITY_RELEASE
#endif
#define UNITY_RELEASE 1
#endif


// turn off some warnings
#pragma warning(disable:4267) // conversion from size_t to int, possible loss of data
#pragma warning(disable:4244) // conversion from int|double to float, possible loss of data
#pragma warning(disable:4800) // forcing value to bool true/false (performance warning)
#pragma warning(disable:4311) // pointer truncation to SInt32
#pragma warning(disable:4312) // conversion from SInt32 to pointer
#pragma warning(disable:4101) // unreferenced local variable
#pragma warning(disable:4018) // signed/unsigned mismatch
#pragma warning(disable:4675) // resolved overload was found by argument-dependent lookup
#pragma warning(disable:4138) // * / found outside of comment
#pragma warning(disable:4554) // check operator precedence for possible error

#pragma warning(disable:4530) // exception handler used
#pragma warning(disable:4355) //'this' : used in base member initializer list

#if UNITY_WINRT
// Include APIs from Windows 8 and up
#define NTDDI_VERSION 0x06020000
#define _WIN32_WINNT 0x0602
#define WINVER 0x0602
#else
// Include APIs from Windows XP and up
#define NTDDI_VERSION 0x05010000 // NTDDI_WINXP
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX 1

#if defined(_XBOX) || defined(XBOX)

#include <xtl.h>
#include <xgraphics.h>
#include <xaudio2.h>
#include <xmedia2.h>
#include <xmp.h>
#include <NuiApi.h>
#include <NuiHandles.h>
#include <xavatar.h>
#include <NuiAudio.h>
#include <xauth.h>
#include <xhttp.h>

// Xbox 360 master build
#ifndef MASTER_BUILD
#define MASTER_BUILD 0
#endif

#if !MASTER_BUILD
#include "xbdm.h"
#endif

#else
#define OEMRESOURCE
#include <windows.h>
#endif

#if UNITY_WINRT
#include <wrl.h>
#endif


#if defined(_MSC_VER) && _MSC_VER < 1800
#define va_copy(a,z) ((void)((a)=(z)))
#endif

#if UNITY_METRO
#define UNITY_METRO_VS2012 (_MSC_VER == 1700)
#define UNITY_METRO_VS2013 (_MSC_VER == 1800)
#else
#define UNITY_METRO_VS2012 0
#define UNITY_METRO_VS2013 0
#endif

// size_t, ptrdiff_t
#include <cstddef>
