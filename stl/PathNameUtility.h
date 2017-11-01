#ifndef PATHNAMEUTILITY_H
#define PATHNAMEUTILITY_H

#include <string>
#include <vector>
#include <functional>


// Path names in Unity always use forward slash as a separator; and convert to other one (if needed)
// only in low-level file functions.
const char kPathNameSeparator = '/';

// If absolutely required, this defines a platform specific path separator.
#if UNITY_WIN || UNITY_XENON
const char kPlatformPathNameSeparator = '\\';
#elif UNITY_OSX || UNITY_WII || UNITY_PS3 || UNITY_IPHONE || UNITY_ANDROID || UNITY_PEPPER || UNITY_LINUX || UNITY_FLASH || UNITY_WEBGL || UNITY_BB10 || UNITY_TIZEN
const char kPlatformPathNameSeparator = '/';
#else
#error "Unknown platform"
#endif



std::string AppendPathName (const std::string& pathName, const std::string& append);
std::string AppendPathNameExtension (const std::string& pathName, const std::string& extension);

std::string GetPathNameExtension (const std::string& pathName);
const char* GetPathNameExtension (const char* path, size_t cachedStringLength);
std::string GetLastPathNameComponent (const std::string& pathName);
const char* GetLastPathNameComponent (const char* path, size_t length);


// Returns true if path is a child folder of beginsWith or the path itself. Case insensitive.
bool StartsWithPath (const std::string &path, const std::string& beginsWith);

std::vector<std::string> FindSeparatedPathComponents (char const* pathName, size_t size, char separator);
inline std::vector<std::string> FindSeparatedPathComponents (std::string const& pathName, char separator) {
	return FindSeparatedPathComponents (pathName.c_str (), pathName.size (), separator);
}

std::string DeletePathNameExtension (const std::string& pathName);
std::string StandardizePathName (const std::string& pathName);

std::string DeleteLastPathNameComponent (const std::string& pathName);
std::string DeleteFirstPathNameComponent (const std::string& pathName);

inline std::string GetFileNameWithoutExtension(const std::string& pathName) { return DeletePathNameExtension(GetLastPathNameComponent(pathName)); }

bool IsPathNameVisible (const std::string& path);


void ConvertSeparatorsToUnity( char* pathName );
template<typename alloc>
void ConvertSeparatorsToUnity( std::basic_string<char, std::char_traits<char>, alloc>& pathName )
{
	typename std::basic_string<char, std::char_traits<char>, alloc>::iterator it = pathName.begin(), itEnd = pathName.end();
	while( it != itEnd ) {
		if( *it == '\\' )
			*it = kPathNameSeparator;
		++it;
	}
}
void ConvertSeparatorsToPlatform( char* pathName );
void ConvertSeparatorsToPlatform( std::string& pathName );

int StrICmp (const char* a, const char* b);

// These functions operate on platform dependent path separators

std::string PlatformAppendPathName (const std::string& pathName, const std::string& append);
std::string PlatformGetLastPathNameComponent (const std::string& pathName);
std::string PlatformDeleteLastPathNameComponent (const std::string& pathName);

std::string EncodePath(const std::string& s);
std::string DecodePath(const std::string& s);

#if UNITY_EDITOR
bool IsValidFileNameSymbol (const char c);

bool CheckValidFileName (const std::string& name);

const char* GetInvalidFilenameChars ();

enum FileNameValid { kFileNameValid = 0, kFileNameInvalid = 1, kFileNameNotRecommended = 2 };
FileNameValid CheckValidFileNameDetail (const std::string& name);

std::string MakeFileNameValid (const std::string& fileName);

// Removes last / or \ if it exists in path
std::string TrimSlash(const std::string& path);

std::string& GetGoodPathNameForGUID (const std::string& pathname);
std::string& GetGoodPathName (const std::string& pathname);

std::string NormalizeUnicode (const std::string& utf8, bool decompose);

inline bool RequiresNormalization (const char* utf8)
{
	const unsigned char* c = (const unsigned char*)utf8;
	while (*c)
	{
		if (*c < 32 || *c > 127)
			return true;

		c++;
	}

	return false;
}

inline bool RequiresNormalization (const std::string& utf8)
{
	return RequiresNormalization (utf8.c_str());
}

// Use this functor as a comparison function when using paths as keys in std::map.
struct PathCompareFunctor : std::binary_function<std::string, std::string, bool>
{ 
	bool operator() ( const std::string& path_a, const std::string& path_b ) const 
	{
#if UNITY_OSX
		bool ascii = !RequiresNormalization (path_a) && !RequiresNormalization (path_b);
		if (ascii)
			return StrICmp(path_a.c_str(), path_b.c_str()) < 0;
		else
			return StrICmp(NormalizeUnicode(path_a, true).c_str(), NormalizeUnicode(path_b, true).c_str()) < 0;		
#else
		return StrICmp(path_a.c_str(), path_b.c_str()) < 0;
#endif
	}
};

struct PathEqualityFunctor : std::binary_function<std::string, std::string, bool>
{ 
	bool operator() ( const std::string& path_a, const std::string& path_b ) const 
	{
#if UNITY_OSX
		bool ascii = !RequiresNormalization (path_a) && !RequiresNormalization (path_b);
		if (ascii)
			return StrICmp(path_a.c_str(), path_b.c_str()) == 0;
		else
			return StrICmp(NormalizeUnicode(path_a, true).c_str(), NormalizeUnicode(path_b, true).c_str()) == 0;		
#else
		return StrICmp(path_a.c_str(), path_b.c_str()) == 0;
#endif
	}
};


#endif

#endif

