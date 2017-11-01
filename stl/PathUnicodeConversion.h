#pragma once

#include "WinUnicode.h"

#include <algorithm>

#if !UNITY_EXTERNAL_TOOL

// These functions are only used in Editor/Standalone, so they do not need to be
// built for crashhandler apps.

#if UNITY_EDITOR || UNITY_STANDALONE
bool GetWideShortPathName(const std::wstring& wideUnityPath, std::wstring& wideShortPath);

std::string ToShortPathName( const std::string& unityPath );
#endif

#if UNITY_EDITOR
void ConvertUnityPathNameToShort( const std::string& utf8, std::string& outBuffer );
#endif

#endif // !UNITY_EXTERNAL_TOOL

inline void ConvertSeparatorsToWindows(std::string& path)
{
	std::replace(path.begin(), path.end(), '/', '\\');
}


// Implementation details for ConvertWindowsPathName
namespace detail {

	template<typename WideString>
	inline void ConvertWindowsPathNameImpl(const WideString& widePath, std::string& outPath)
	{
		ConvertWideToUTF8String( widePath, outPath );
		std::replace(outPath.begin(), outPath.end(), '\\', '/'); // Convert separators to unity
	}

} // namespace detail

inline void ConvertWindowsPathName( const std::wstring& widePath, std::string& outPath )
{
	return detail::ConvertWindowsPathNameImpl( widePath, outPath );
}

// This overload is a minor optimization, i.e., we don't copy wide buffer to a std::wstring, 
// and simply go straight to conversion routines
inline void ConvertWindowsPathName( const wchar_t* widePath, std::string& outPath )
{
	return detail::ConvertWindowsPathNameImpl( widePath, outPath );
}


// Implementation details for ConvertUnityPathName
namespace detail {

	template<typename NarrowString>
	inline void ConvertUnityPathNameImpl(const NarrowString& utf8, std::wstring& widePath)
	{
		ConvertUTF8ToWideString( utf8, widePath );
		std::replace(widePath.begin(), widePath.end(), L'/', L'\\'); // Convert separators to Windows
	}

} // namespace detail

inline void ConvertUnityPathName( const std::string& utf8, std::wstring& widePath )
{
	return detail::ConvertUnityPathNameImpl(utf8, widePath);
}

// This overload is a minor optimization, i.e., we don't copy utf8 buffer to a std::string, 
// and simply go straight to conversion routines
inline void ConvertUnityPathName( const char* utf8, std::wstring& widePath )
{
	return detail::ConvertUnityPathNameImpl(utf8, widePath);
}

// Old and unsafe API
const int kDefaultPathBufferSize = MAX_PATH*4;
void ConvertUnityPathName( const char* utf8, wchar_t* outBuffer, int outBufferSize );
void ConvertWindowsPathName( const wchar_t* widePath, char* outBuffer, int outBufferSize );
