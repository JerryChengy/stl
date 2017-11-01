#include "UnityPrefix.h"
#include "PathNameUtility.h"
#include "Word.h"

#include <algorithm>

using namespace std;

static void AppendPathNameImpl( const string& pathName, const string& append, char separator, std::string& res )
{
	res.reserve (pathName.size () + append.size () + 1);
	if (!pathName.empty () && !append.empty ())
	{
		if (pathName[pathName.size () - 1] == separator)
		{
			if (append[0] == separator)
			{
				res += pathName;
				res.append (append.begin () + 1, append.end ());
				return;
			}
			else
			{
				res += pathName;
				res += append;
				return;
			}
		}
		else
		{
			if (append[0] == separator)
			{
				res += pathName;
				res += append;
				return;
			}
			else
			{
				res += pathName;
				res += separator;
				res += append;
				return;
			}
		}
	}
	else if (pathName.empty ())
		res = append;
	else
		res = pathName;
}

string AppendPathName (const string& pathName, const string& append)
{
	string res;
	AppendPathNameImpl( pathName, append, kPathNameSeparator, res );
	return res;
}

string PlatformAppendPathName (const string& pathName, const string& append)
{
	string res;
	AppendPathNameImpl( pathName, append, kPlatformPathNameSeparator, res );
	return res;
}


string AppendPathNameExtension (const string& pathName, const string& extension)
{
	if (extension.empty ())
		return pathName;

	string newPathName;
	newPathName.reserve (pathName.size () + 1 + extension.size ());
	newPathName.append (pathName);
	newPathName.append (".");
	newPathName.append (extension);
	return newPathName;
}

string GetPathNameExtension (const string& pathName)
{
	return GetPathNameExtension (pathName.c_str(), pathName.size());
}

const char* GetPathNameExtension (const char* path, size_t length)
{
	for (size_t i=0;i<length;i++)
	{
		if (path[length - i - 1] == kPathNameSeparator)
			return "";
		if (path[length - i - 1] == '.')
			return path + length - i;
	}
	return "";
}

string DeletePathNameExtension (const string& pathName)
{
	string::size_type slash = pathName.rfind (kPathNameSeparator);
	string::size_type dot = pathName.rfind ('.');

	if (dot != string::npos)
	{
		if (slash == string::npos || dot > slash)
			return string (&pathName[0], dot);
	}
	return pathName;
}

vector<string> FindSeparatedPathComponents (char const* constPathName, size_t size, char separator)
{
	char const* current = constPathName, *end = constPathName + size;
	vector<string> components;

	while (current != end)
	{
		char const* pos = std::find (current, end, separator);

		if (pos != current)
			components.push_back (std::string (current, pos));

		if (pos == end)
			break;

		current = pos + 1;
	}

	return components;
}

string DeleteLastPathNameComponent (const string& pathName)
{
	string::size_type p = pathName.rfind (kPathNameSeparator);
	if (p == string::npos)
		return string ();
	else
		return string (&pathName[0], p);
}

string PlatformDeleteLastPathNameComponent (const string& pathName)
{
	string::size_type p = pathName.rfind (kPlatformPathNameSeparator);
	if (p == string::npos)
		return string ();
	else
		return string (&pathName[0], p);
}

string GetLastPathNameComponent (const string& pathName)
{
	return GetLastPathNameComponent(pathName.c_str(), pathName.size());
}

const char* GetLastPathNameComponent (const char* path, size_t length)
{
	for (size_t i=0;i<length;i++)
	{
		if (path[length - i - 1] == kPathNameSeparator)
			return path + length - i;
	}
	return path;
}

string PlatformGetLastPathNameComponent(const string& pathName)
{
	string::size_type p = pathName.rfind (kPlatformPathNameSeparator);
	if (p == string::npos)
		return pathName;
	else
		return string (&pathName[1 + p], pathName.size () - 1 - p);
}

string DeleteFirstPathNameComponent (const string& pathName)
{
	string::size_type p = pathName.find (kPathNameSeparator);
	if (p == string::npos)
		return string ();
	else
		return string (&pathName[1 + p], pathName.size () - 1 - p);
}

string StandardizePathName (const string& pathName)
{
	if (pathName.empty ())
		return pathName;

	// Remove initial / if not a // in the beginning	
	if (pathName[0] == kPathNameSeparator && pathName.size () > 1 && pathName[1] != kPathNameSeparator)
		return string (&pathName[1], pathName.size () - 1);
	else
		return pathName;
}

bool IsPathNameVisible (const string& path)
{
	if (path.empty())
		return false;

	string::size_type p = 0;
	while (p < path.size ())
	{
		if (path[p] == '.')
			return false;
		p = path.find (kPathNameSeparator, p);
		if (p == string::npos)
			return true;
		p++;
	}
	return true;
}


void ConvertSeparatorsToUnity( char* pathName )
{
	while( *pathName != '\0' ) {
		if( *pathName == '\\' )
			*pathName = kPathNameSeparator;
		++pathName;
	}
}

void ConvertSeparatorsToPlatform( char* pathName )
{
	if (kPathNameSeparator != kPlatformPathNameSeparator) {
		while( *pathName != '\0' ) {
			if( *pathName == kPathNameSeparator )
				*pathName = kPlatformPathNameSeparator;
			++pathName;
		}
	}
}

void ConvertSeparatorsToPlatform( std::string& pathName )
{
	if (kPathNameSeparator != kPlatformPathNameSeparator) {
		std::string::iterator it = pathName.begin(), itEnd = pathName.end();
		while( it != itEnd ) {
			if( *it == kPathNameSeparator )
				*it = kPlatformPathNameSeparator;
			++it;
		}
	}
}

#if UNITY_EDITOR
const char* invalidFilenameChars = "/?<>\\:*|\"";

const char* GetInvalidFilenameChars ()
{
	return invalidFilenameChars;
}

bool IsValidFileNameSymbol (const char c)
{
	return std::string(invalidFilenameChars).find(c) == string::npos;
}

bool CheckValidFileName (const std::string& name)
{
	return CheckValidFileNameDetail(name) == kFileNameValid;
}


std::string MakeFileNameValid (const std::string& fileName)
{
	string output = fileName;

	// Disallow space at the beginning
	while (!output.empty() && output[0] == ' ')
		output.erase(0, 1);

	if (output.empty())
		return output;

	// Disallow space at the end
	if (output[output.size()-1] == ' ' || output[output.size()-1] == '.')
		output[output.size()-1] = '_';

	for (int i=0;i<output.size();i++)
	{
		if (!IsValidFileNameSymbol(output[i]))
			output[i] = '_';
	}


	if (CheckValidFileName(output))
		return output;
	else
		return string();
}


FileNameValid CheckValidFileNameDetail (const std::string& name)
{
	// none of these can be part of file/folder name
	if( name.find_first_of("/\\") != string::npos )
		return kFileNameInvalid;

	// anything up to first dot can't be: com1..com9, lpt1..lpt9, con, nul, prn
	size_t dotIndex = name.find('.');
	if( dotIndex == 4 )
	{
		if( name[0]=='c' && name[1]=='o' && name[2]=='m' && name[3]>='0' && name[3]<='9' )
			return kFileNameInvalid;
		if( name[0]=='l' && name[1]=='p' && name[2]=='t' && name[3]>='0' && name[3]<='9' )
			return kFileNameInvalid;
	}
	if( dotIndex == 3 )
	{
		if( name[0]=='c' && name[1]=='o' && name[2]=='n' )
			return kFileNameInvalid;
		if( name[0]=='n' && name[1]=='u' && name[2]=='l' )
			return kFileNameInvalid;
		if( name[0]=='p' && name[1]=='r' && name[2]=='n' )
			return kFileNameInvalid;
	}

	// . on OS X means invisible file
	if( dotIndex == 0 )
		return kFileNameInvalid;

	// file/folder name can't end with a dot or a space
	if( !name.empty() )
	{
		// http://support.microsoft.com/kb/115827
		char lastChar = name[name.size()-1];
		if( lastChar == ' ' || lastChar == '.' )
			return kFileNameInvalid;

		// File names starting with a space will not get preserved correctly when zipping
		// on windows. So don't allow that.
		if (name[0] == ' ')
			return kFileNameNotRecommended;
	}

	if( name.find_first_of(invalidFilenameChars) != string::npos )
		return kFileNameNotRecommended;

	return kFileNameValid;
}

std::string TrimSlash(const std::string& path)
{
	if (!path.empty())
	{
		const char lastChar = path[path.size() - 1];
		if (lastChar == '/' || lastChar == '\\')
			return path.substr(0, path.size() - 1);
	}

	return path;
}

string NormalizeUnicode (const string& utf8, bool decompose)
{
#if UNITY_OSX
	// OS X stores Unicode file names in decomposed form. Ie ?(U-Umlaut), becomes represented as two characters
	// -U and ?(Unicode U+0308), instead of a single ?(Unicode U+00DC). For proper display and consistency of 
	// names, they need to be converted into precomposed form.
	CFStringRef cfLowercased = StringToCFString(utf8);
	if (!cfLowercased)
		ErrorStringMsg("Failed to convert string '%s' to CFString", utf8.c_str());
	CFMutableStringRef cf = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, cfLowercased);
	CFStringNormalize(cf, decompose?kCFStringNormalizationFormD:kCFStringNormalizationFormC);
	string result = CFStringToString(cf);
	if (cf != NULL)
		CFRelease(cf);
	if (cfLowercased != NULL)
		CFRelease(cfLowercased);
	return result;
#else
	return utf8;
#endif
}

// Serialization does weird shit without case mangling - even if filesystem paths shouldn't
// be mangled on Linux, we apparently need them mangled in some serialization stuff anyway...
string& GetGoodPathNameForGUID (const string& pathname)
{
	static string lowercased;

	lowercased = GetGoodPathName(pathname);

#if UNITY_LINUX
	ToLowerInplace(lowercased);
#endif

	return lowercased;
}

string& GetGoodPathName (const string& pathname)
{
	static string lowercased;
	if (lowercased.capacity () < 4096)
		lowercased.reserve (4096);

	if (!pathname.empty () && pathname[0] == kPathNameSeparator)
	{
		lowercased.clear();
		return lowercased;
	}

	lowercased = pathname;

#if UNITY_WIN
	ConvertSeparatorsToUnity( lowercased );
#endif

#if UNITY_OSX
	if (RequiresNormalization(lowercased))
		lowercased = NormalizeUnicode (lowercased, false);
#endif

#if !UNITY_LINUX
	ToLowerInplace(lowercased);
#endif

	return lowercased;
}

#endif


#if !UNITY_PLUGIN
bool StartsWithPath (const std::string &s, const std::string& beginsWith)
{
	size_t beginsWithSize = beginsWith.size ();
	if (s.size () < beginsWithSize)
		return false;

	for (size_t i=0;i<beginsWithSize;i++)
	{
		if (ToLower(s[i]) != ToLower(beginsWith[i]))
			return false;
	}

	// Exact match or empty beginsWith
	if (s.size () == beginsWithSize || beginsWith.empty())
		return true;
	// Next unmatched character or last matched character is
	// path separator.
	else if (s[beginsWithSize] == kPathNameSeparator ||
		beginsWith[beginsWithSize - 1] == kPathNameSeparator)
		return true;
	else
		return false;
}

// Converts a hex digit to the actual value. Invalid hex characters are treated as '0'
static inline unsigned char Unhex(char c) {
	if( c >= '0' && c <= '9')
		return c-'0';
	if( c >= 'a' && c <= 'f')
		return c-'a'+10;
	if( c >= 'A' && c <= 'F')
		return c-'A'+10;
	return 0;
}

// Takes a string containing path characters and URL escapes and returns a string that can be used as a file name
string EncodePath(const string& s)
{
	string result;
	size_t i=0;
	while( i < s.size() )
	{
		size_t j=s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ", i);
		if (j == string::npos )
		{
			result += ToLower(s.substr(i));
			break;
		}

		if( j>i ) result += ToLower(s.substr(i,j-i));

		if ( s[j] == '%' && j+2 < s.size() )
		{
			// decode %xx URL escapes 
			unsigned char decoded = ToLower((char)(Unhex(s[j+1]) << 4 | Unhex(s[j+2])));
			if( (decoded >= 'a' && decoded <= 'z') || (decoded >= '0' && decoded <= '9') || decoded == ' ' )
				result += decoded;
			else if (decoded == '/')
				result += '-';
			else
				result += Format("_%02x",(int)decoded);
			i=j+3;

		}
		else if (s[j] == '?')
		{
			// Don't include URL parameters
			break;
		}
		else if (s[j] == '/')
		{
			// Convert slashes to dashes
			while ( j+1 < s.size() && s[j+1] == '/' ) j++; 	// Collapse multiple /es into one
			result += '-';
			i=j+1;
		}
		else
		{
			// Encode anything "dangerous" as _XX hex encoding
			result += Format("_%02x",(int)s[j]);
			i=j+1;
		}
	}

	return result;
}

string DecodePath(const string& s)
{
	string result;
	size_t i=0;
	while( i < s.size() )
	{
		size_t j=s.find("_", i);
		if (j == string::npos )
		{
			result += s.substr(i);
			break;
		}

		if( j>i ) result += s.substr (i,j-i);

		string escape = s.substr (j, 3);
		unsigned int ch = ' ';
		sscanf(escape.c_str(), "_%x", &ch);

		result += (unsigned char)ch;
		i = j+3;
	}

	return result;
}

#endif
