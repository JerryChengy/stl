#ifndef UNITYCONFIGURE_H
#define UNITYCONFIGURE_H

#include "GlobalCppDefines.h"
#include "PerPlatformCppDefines.h"

/// Increase this number by one if you want all users 
/// running a project with a different version to rebuild the library
#define UNITY_REBUILD_LIBRARY_VERSION 9

// Increase this number if you want to prevent users from downgrading to an older version.
// (It will ask the user to quit and warn him about potential loss of data)
#define UNITY_FORWARD_COMPATIBLE_VERSION 39

// Increase this number of you want to tell people that now your project folder won't open
// in old Unity versions anymore
#define UNITY_ASK_FOR_UPGRADE_VERSION 39

/// Increase this number if you want the user the user to upgrade to the latest standard assets
#define UNITY_STANDARD_ASSETS_VERSION 0

#ifndef UNITY_EXTERNAL_TOOL
#define UNITY_EXTERNAL_TOOL 0
#endif

#if UNITY_EXTERNAL_TOOL
// Threaded loading is always off in external tools (e.g. BinaryToText)
#define THREADED_LOADING 0
#define ENABLE_THREAD_CHECK_IN_ALLOCS 0
#else
#define THREADED_LOADING (!UNITY_FLASH && !UNITY_WII && !UNITY_WEBGL)
#define ENABLE_THREAD_CHECK_IN_ALLOCS 0
#endif

#define THREADED_LOADING_DEBUG 0

#define USE_PREBAKED_COLLISIONMESH 0
#define SUPPORT_RUNTIME_COLLISION_MESHBAKING 1
#define SUPPORT_ENVIRONMENT_VARIABLES (!UNITY_XENON && !UNITY_PS3 && !UNITY_IPHONE && !UNITY_WINRT)

//#ifndef SUPPORT_THREADS //temp hack
//#define SUPPORT_THREADS (!UNITY_FLASH && !UNITY_WII && !UNITY_WEBGL)
//#endif

#ifndef ENABLE_MULTITHREADED_CODE //temp hack
#define ENABLE_MULTITHREADED_CODE (!UNITY_FLASH && !UNITY_WII && !UNITY_WEBGL)
#endif

#if UNITY_FLASH && !FLASH_WANTS_PHYSX
#undef ENABLE_PHYSICS
#define ENABLE_PHYSICS 0
#endif

#if UNITY_NACL && !INCLUDE_PHYSX
#undef ENABLE_PHYSICS
#define ENABLE_PHYSICS 0
#undef ENABLE_CLOTH
#define ENABLE_CLOTH 0
#endif

#define MONO_2_10 0
#define MONO_2_12 (INCLUDE_MONO_2_12 && (UNITY_WIN || UNITY_OSX))
#define ENABLE_SCRIPTING (!UNITY_WEBGL && !UNITY_EXTERNAL_TOOL || UNITY_WINRT)
#define ENABLE_UNITYGUI !UNITY_WEBGL
#define ENABLE_PNG_JPG (!UNITY_WII && !UNITY_WEBGL)
#define ENABLE_TEXTURE_PADDED_SCALED_IMAGES (!UNITY_WII)
#define ENABLE_WEBPLAYER_SECURITY ((UNITY_EDITOR || WEBPLUG) && ENABLE_MONO && !UNITY_PEPPER && !UNITY_WEBGL)
#define ENABLE_MEMORY_MANAGER (!UNITY_EXTERNAL_TOOL && !UNITY_WII && !ENABLE_GFXDEVICE_REMOTE_PROCESS_WORKER)
#define ENABLE_ASSET_STORE (UNITY_EDITOR && !UNITY_64 && !UNITY_LINUX) // not in 64 bit yet and not on Linux
#define ENABLE_ASSET_SERVER (UNITY_EDITOR && !UNITY_64 && !UNITY_LINUX) // not in 64 bit yet and not on Linux
#define ENABLE_LIGHTMAPPER (UNITY_EDITOR && !UNITY_LINUX)

// More information can be found here - http://msdn.microsoft.com/en-us/library/windows/apps/bg182880.aspx#five
// Disable for now, as it seems if you create swapchain with DXGI_FORMAT_B8G8R8A8_UNORM, you only see black screen
#define ENABLE_DX11_FRAME_LATENCY_WAITABLE_OBJECT (UNITY_METRO && UNITY_METRO_VS2013)

//we cannot move these to PerPlatformDefines.pm, because these are enabled for native code, but by default disabled for managed code.
#define UNITY_PS3_API (UNITY_EDITOR || UNITY_PS3)
#define UNITY_XENON_API 0
#define UNITY_METRO_API (UNITY_EDITOR || UNITY_METRO)
#define UNITY_WP8_API (UNITY_EDITOR || UNITY_WP8)
#define UNITY_WINRT_API (UNITY_METRO_API || UNITY_WP8_API)

#if UNITY_PS3 || UNITY_XENON || UNITY_WINRT
#define UNITY_DYNAMIC_TLS 0
#else
#define UNITY_DYNAMIC_TLS 1
#endif

#ifndef SUPPORT_MONO_THREADS
#define SUPPORT_MONO_THREADS (ENABLE_MONO && SUPPORT_THREADS)
#endif

#define UNITY_EMULATE_PERSISTENT_DATAPATH ((UNITY_WIN && !UNITY_WINRT) || UNITY_OSX || UNITY_LINUX || UNITY_XENON)

// TODO: pass UNITY_DEVELOPER_BUILD instead of ENABLE_PROFILER from projects
// Defines whether this is a development player build (user checks "Development Player" in the build settings)
// It determines whether debugging symbols are built/used, profiler can be enabled, etc.
#if !defined(UNITY_DEVELOPER_BUILD)
#if defined(ENABLE_PROFILER)
#define UNITY_DEVELOPER_BUILD ENABLE_PROFILER
#else
#define UNITY_DEVELOPER_BUILD 1
#endif
#endif

#define UNITY_ISBLEEDINGEDGE_BUILD 0

#define PLATFORM_SUPPORTS_PROFILER (UNITY_PS3 || UNITY_XENON || UNITY_OSX || UNITY_WIN || UNITY_LINUX || UNITY_IPHONE || UNITY_ANDROID || UNITY_PEPPER || UNITY_BB10 || UNITY_WINRT || UNITY_TIZEN)

#ifndef ENABLE_PROFILER
#define ENABLE_PROFILER ((UNITY_EDITOR || (UNITY_DEVELOPER_BUILD && PLATFORM_SUPPORTS_PROFILER)) && !UNITY_EXTERNAL_TOOL)
#endif


#define ENABLE_PROFILER_INTERNAL_CALLS 0

#define ENABLE_SHARK_PROFILE UNITY_OSX && ENABLE_PROFILER
#define ENABLE_MONO_MEMORY_PROFILER (ENABLE_PROFILER && ENABLE_MONO)
#define ENABLE_ETW (ENABLE_PROFILER && UNITY_WP8)

// Cannot enable on Android, as it overrides callbacks in Android_Profiler.cpp
//Be VERY careful when turning on heapshot profiling. we disabled it on all platforms for now, because it was causing flipflop deadlocks in windows gfx tests only in multithreaded mode.
//usually around test 72. if you enable this, make sure to do many runs of wingfxtests and ensure you didn't introdue a flipflopping deadlock.
#define ENABLE_MONO_HEAPSHOT 0
#define ENABLE_MONO_HEAPSHOT_GUI 0

#define PLATFORM_SUPPORTS_PLAYERCONNECTION (!UNITY_WII && !UNITY_PEPPER && !UNITY_WEBGL)
#define PLATFORM_SUPPORTS_PLAYERCONNECTION_LISTENING (PLATFORM_SUPPORTS_PLAYERCONNECTION && !UNITY_FLASH)
#ifdef ENABLE_PLAYERCONNECTION
#undef ENABLE_PLAYERCONNECTION
#endif
#define ENABLE_PLAYERCONNECTION (UNITY_DEVELOPER_BUILD && PLATFORM_SUPPORTS_PLAYERCONNECTION || UNITY_FLASH)

#define ENABLE_SOCKETS (ENABLE_NETWORK || ENABLE_PLAYERCONNECTION)

// MONO_QUALITY_ERRORS are only for the editor!
#define MONO_QUALITY_ERRORS UNITY_EDITOR

// ENABLE_MONO_API_THREAD_CHECK are only for the editor and development/debug builds!
#define ENABLE_MONO_API_THREAD_CHECK UNITY_EDITOR || ( (UNITY_DEVELOPER_BUILD || DEBUGMODE) && SUPPORT_THREADS )

// New Unload Unused Assets implementation doesn't work on WinRT, that's why we use old implementation
#define ENABLE_OLD_GARBAGE_COLLECT_SHARED_ASSETS UNITY_WINRT

#ifndef SUPPORT_REPRODUCE_LOG
#define SUPPORT_REPRODUCE_LOG 0
#endif

#define SUPPORT_MOUSE (!UNITY_PS3 && !UNITY_XENON)


/// Instruments OpenGL GfxDevice to get a better overview with regressions caused by graphics
/// (This should be ported to it's own GfxDevice probably)
#ifndef SUPPORT_REPRODUCE_LOG_GFX_TRACE
#define SUPPORT_REPRODUCE_LOG_GFX_TRACE 0
#endif

/// Adds printing of AwakeFromLoad and Update functions with name and class type
/// Setting the value to 2 will also print the instanceID
/// This is useful for detecting changes in execution order.
#ifndef SUPPORT_LOG_ORDER_TRACE
#define SUPPORT_LOG_ORDER_TRACE 0
#endif


#define SUPPORT_ERROR_EXIT WEBPLUG 
#define SUPPORT_DIRECT_FILE_ACCESS (!UNITY_PEPPER)
#define HAS_DXT_DECOMPRESSOR !UNITY_IPHONE
#define HAS_PVRTC_DECOMPRESSOR UNITY_EDITOR || UNITY_ANDROID || UNITY_IPHONE || UNITY_BB10

#define HAS_FLASH_ATF_DECOMPRESSOR 0

/// Should we output errors when writing serialized files with wrong alignment on a low level.
/// (ProxyTransfer should find all alignment issues in theory)
#define CHECK_SERIALIZE_ALIGNMENT 0 /////@TODO: WHY IS THIS NOT ENABLED?????

#define SUPPORT_SERIALIZE_WRITE UNITY_EDITOR
#define SUPPORT_TEXT_SERIALIZATION UNITY_EDITOR

#define SUPPORT_SERIALIZED_TYPETREES (UNITY_EDITOR || WEBPLUG || UNITY_EXTERNAL_TOOL || UNITY_OSX || (UNITY_WIN && !UNITY_WINRT) || UNITY_LINUX) && !ENABLE_SERIALIZATION_BY_CODEGENERATION

#ifndef UNITY_IPHONE_AUTHORING
#define UNITY_IPHONE_AUTHORING UNITY_EDITOR && UNITY_OSX
#endif

#ifndef UNITY_ANDROID_AUTHORING
#define UNITY_ANDROID_AUTHORING UNITY_EDITOR
#endif

#define USE_MONO_AOT ((UNITY_IPHONE && !TARGET_IPHONE_SIMULATOR) || UNITY_XENON || UNITY_PS3)
#define INTERNAL_CALL_STRIPPING UNITY_IPHONE
#define SUPPORTS_NATIVE_CODE_STRIPPING (UNITY_IPHONE || UNITY_XENON || UNITY_PS3)

#ifndef USE_ANCIENT_MONO
#define USE_ANCIENT_MONO 0
#endif

// Leaving AOT targets disabled for now - debugging on AOT will _at least_ require additional AOT compilation arguments
// On Wii, though we don't have unity profiler capabilities, this is still useful because it loads mono debugging info which let's us resolve IL offsets during crashes
// On Xbox we use this to load debugging info for resolving stack traces
#define USE_MONO_DEBUGGER (ENABLE_PROFILER && (!USE_MONO_AOT || UNITY_IPHONE || UNITY_PEPPER)) || (UNITY_WII && !MASTER_BUILD) || (UNITY_XENON && !MASTER_BUILD)

#ifndef PLAY_QUICKTIME_DIRECTLY
#define PLAY_QUICKTIME_DIRECTLY 0
#endif


// Do we have iPhone remote support (which is only on the host PC and only in the editor)
#define SUPPORT_IPHONE_REMOTE (UNITY_IPHONE_AUTHORING || UNITY_ANDROID_AUTHORING)

#define LOCAL_IDENTIFIER_IN_FILE_SIZE 32

#define ENABLE_SECURITY WEBPLUG || UNITY_EDITOR

#define ALLOW_CLASS_ID_STRIPPING !UNITY_EDITOR

#ifndef UNITY_ASSEMBLER

#if LOCAL_IDENTIFIER_IN_FILE_SIZE > 32
--- Must change kCurrentSerializeVersion in SerializedFile.cpp
	typedef UInt64 LocalIdentifierInFileType;
#else
typedef SInt32 LocalIdentifierInFileType;
#endif

#endif // UNITY_ASSEMBLER

#ifndef ENABLE_MEMORY_TRACKING 
// ENABLE_MEMORY_TRACKING is not enabled by default, because it makes debug build very slow
// we enable it for certain builds from a command line
//#define ENABLE_MEMORY_TRACKING UNITY_WIN && !UNITY_RELEASE && SUPPORT_REPRODUCE_LOG
#define ENABLE_MEMORY_TRACKING 0
#endif

// For now C++ tests are only run / supported in the editor
#ifndef ENABLE_UNIT_TESTS
#define ENABLE_UNIT_TESTS (UNITY_EDITOR || (UNITY_XENON && _DEBUG) || (UNITY_PS3 && _DEBUG))
#endif

#define SUPPORT_RESOURCE_IMAGE_LOADING (!UNITY_EDITOR && !WEBPLUG && !UNITY_FLASH && !UNITY_WEBGL)
#define SUPPORT_SERIALIZATION_FROM_DISK (!UNITY_FLASH)

// ios and android draw splash by themselves
#define UNITY_CAN_SHOW_SPLASH_SCREEN (!UNITY_EDITOR && !WEBPLUG && !UNITY_FLASH && !UNITY_WEBGL && !UNITY_ANDROID && !UNITY_IPHONE && !UNITY_WP8)

#define ENABLE_CUSTOM_ALLOCATORS_FOR_STDMAP (!UNITY_FLASH && !UNITY_WEBGL && !UNITY_LINUX && !(UNITY_OSX && UNITY_64))

#ifndef DOXYGEN
#define DOXYGEN 0 
#endif

#ifndef UNITY_FBX_IMPORTER
#define UNITY_FBX_IMPORTER 0
#endif 

#ifndef JAM_BUILD
#define JAM_BUILD 0
#endif

// File name limit for any file output as a result of Unity's build process
// Current limit: 40 characters (Xbox 360 STFS)
#define UNITY_RESOURCE_FILE_NAME_MAX_LEN 40

#define UNITY_PLUGINS_AVAILABLE (!UNITY_WII && !UNITY_IPHONE && !UNITY_PS3 && !WEBPLUG && !UNITY_FLASH && !UNITY_WEBGL)

#define ENABLE_HARDWARE_INFO_REPORTER (WEBPLUG && !UNITY_PEPPER) || (UNITY_STANDALONE) || UNITY_ANDROID || UNITY_IPHONE || UNITY_WINRT || UNITY_BB10 || UNITY_TIZEN

#if UNITY_FLASH
#define SCRIPT_BINDINGS_EXPORT_DECL extern "C"
#define SCRIPT_CALL_CONVENTION
#elif UNITY_WINRT
#define SCRIPT_BINDINGS_EXPORT_DECL
#if ENABLE_WINRT_PINVOKE
#	define SCRIPT_CALL_CONVENTION
#else
#	define SCRIPT_CALL_CONVENTION WINAPI
#endif
#else
#define SCRIPT_BINDINGS_EXPORT_DECL
#define SCRIPT_CALL_CONVENTION
#endif

#endif
