#include "PathUnicodeConversion.h"

#if !UNITY_EXTERNAL_TOOL

#if UNITY_EDITOR || UNITY_STANDALONE

// Implementation details for ConvertUnityPathNameToShort/ToShortPathName
bool GetWideShortPathName(const std::wstring& wideUnityPath, std::wstring& wideShortPath)
{
#if UNITY_WINRT

	wideShortPath = wideUnityPath; // !?
	return true;

#else
	wideShortPath.resize( MAX_PATH );
	DWORD const path_len = GetShortPathNameW(wideUnityPath.c_str(), &wideShortPath.front(), wideShortPath.size());
	if( 0u == path_len ) // The function has failed
	{
		return false;
	}
	else
	{
		wideShortPath.resize(path_len); // Typically this will properly truncate the string

		if ( path_len > MAX_PATH )
		{
			// Absolutely unlikely, but possible; in such a case where the previous buffer was not enough
			// to hold the short path, we simply increase the size of the buffer
			// and try again to fetch the directory name.
			DWORD const path_len2 = GetShortPathNameW(wideUnityPath.c_str(), &wideShortPath.front(), wideShortPath.size());
			return path_len2 == wideShortPath.size();
		}

		return true;
	}
#endif // UNITY_WINRT
}

std::string ToShortPathName( const std::string& unityPath )
{
	std::wstring wideUnityPath;
	ConvertUTF8ToWideString(unityPath, wideUnityPath);

	std::wstring wideShortPath;
	if( GetWideShortPathName(wideUnityPath, wideShortPath) )
	{
		std::string convertedPath;
		ConvertWindowsPathName(wideShortPath, convertedPath);
		return convertedPath;
	}

	return unityPath;
}

#endif // UNITY_EDITOR || UNITY_STANDALONE

#if UNITY_EDITOR
#include "WinUtils.h"
void ConvertUnityPathNameToShort( const std::string& utf8, std::string& outBuffer )
{
	std::wstring widePath;
	ConvertUnityPathName( utf8, widePath );

	std::wstring shortPath;
	if( GetWideShortPathName(widePath, shortPath) )
	{
		ConvertToDefaultAnsi( shortPath, outBuffer );
	}
	else
	{
		outBuffer.clear();
		//ErrorString(Format("Failed to get short form of path '%s': %s", utf8.c_str(), winutils::ErrorCodeToMsg(GetLastError()).c_str()));
	}
}
#endif // UNITY_EDITOR

#endif // !UNITY_EXTERNAL_TOOL

namespace detail {

	static void ConvertSeparatorsToUnity( char* pathName )
	{
		while( *pathName != '\0' ) {
			if( *pathName == '\\' )
				*pathName = '/';
			++pathName;
		}
	}

	static void ConvertSeparatorsToWindows( wchar_t* pathName )
	{
		while( *pathName != L'\0' ) {
			if( *pathName == L'/' )
				*pathName = L'\\';
			++pathName;
		}
	}

} // namespace detail

// Old and unsafe API
void ConvertWindowsPathName( const wchar_t* widePath, char* outBuffer, int outBufferSize )
{
	::WideCharToMultiByte( CP_UTF8, 0, widePath, -1, outBuffer, outBufferSize, NULL, NULL );
	detail::ConvertSeparatorsToUnity( outBuffer );
}

void ConvertUnityPathName( const char* utf8, wchar_t* outBuffer, int outBufferSize )
{
	UTF8ToWide( utf8, outBuffer, outBufferSize );
	detail::ConvertSeparatorsToWindows( outBuffer );
}
