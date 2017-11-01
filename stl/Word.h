#ifndef WORD_H
#define WORD_H

#include <string>
#include <vector>
#include <stdarg.h>			// va_list
#include <vector>
#include "Annotations.h"
#include "MemoryMacros.h"
#include "ExportModules.h"

#if UNITY_OSX || UNITY_IPHONE
#include "CoreFoundation/CoreFoundation.h"
#endif

bool BeginsWithCaseInsensitive (const std::string &s, const std::string &beginsWith);

bool BeginsWith (const char* s, const char* beginsWith);

template<typename StringType>
bool BeginsWith (const StringType &s, const StringType &beginsWith)
{
	return BeginsWith (s.c_str(), beginsWith.c_str());
}
template<typename StringType>
bool BeginsWith (const StringType &s, const char *beginsWith)
{
	return BeginsWith (s.c_str(), beginsWith);
}

inline bool EndsWith (const char* str, int strLen, const char* sub, int subLen){
	return (strLen >= subLen) && (strncmp (str+strLen-subLen, sub, subLen)==0);
}

template<typename StringType>
bool EndsWith (const StringType& str, const StringType& sub)
{
	return EndsWith(str.c_str(), str.size(), sub.c_str(), sub.size());
}

template<typename StringType>
bool EndsWith (const StringType& str, const char* endsWith)
{
	return EndsWith(str.c_str(), str.size(), endsWith, strlen(endsWith));
}

inline bool EndsWith (const char* s, const char* endsWith){
	return EndsWith(s, strlen(s), endsWith, strlen(endsWith));
}

void ConcatCString( char* root, const char* append );
#if !WEBPLUG
bool	IsStringNumber (const char* s);
bool 	IsStringNumber (const std::string& s);
#endif

SInt32 StringToInt (char const* s);

template <typename StringType>
SInt32 StringToInt (const StringType& s)
{
	return StringToInt(s.c_str());
}

/// Replacement for atof is not dependent on locale settings for what to use as the decimal separator.
/// Limited support but fast. It does'nt work for infinity, nan, but
/// This function is lossy. Converting a string back and forth does not result in the same binary exact float representation.
/// See FloatStringConversion.h for binary exact string<->float conversion functions.
float SimpleStringToFloat (const char* str, int* outLength = NULL);

std::string IntToString (SInt32 i);
std::string UnsignedIntToString (UInt32 i);
std::string Int64ToString (SInt64 i);
std::string UnsignedInt64ToString (UInt64 i);
std::string DoubleToString (double i);
std::string EXPORT_COREMODULE FloatToString (float f, const char* precFormat = "%f");

std::string SHA1ToString(unsigned char hash[20]);
std::string MD5ToString(unsigned char hash[16]);

int StrNICmp (const char* a, const char* b, size_t n);
int StrICmp (const char* a, const char* b);
inline int StrICmp(const UnityStr& str1, const UnityStr& str2) { return StrICmp (str1.c_str (), str2.c_str ()); }
int StrCmp (const char* a, const char* b);
inline int StrCmp(const std::string& str1, const std::string& str2) { return StrCmp (str1.c_str (), str2.c_str ()); }
inline int StrICmp(const std::string& str1, const std::string& str2) { return StrICmp (str1.c_str (), str2.c_str ()); }

#if UNITY_EDITOR
int SemiNumericCompare(const char * str1, const char * str2);
inline int SemiNumericCompare(const std::string& str1, const std::string& str2) { return SemiNumericCompare (str1.c_str (), str2.c_str ()); }
#endif

inline char ToLower (char v)
{
	if (v >= 'A' && v <= 'Z')
		return static_cast<char>(v | 0x20);
	else
		return v;
}

inline char ToUpper (char v)
{
	if (v >= 'a' && v <= 'z')
		return static_cast<char>(v & 0xdf);
	else
		return v;
}

template<typename StringType>
StringType ToUpper (const StringType& input)
{
	StringType s = input;
	for (typename StringType::iterator i= s.begin (); i != s.end ();i++)
		*i = ToUpper (*i);
	return s;
}
template<typename StringType>
StringType ToLower (const StringType& input)
{
	StringType s = input;
	for (typename StringType::iterator i= s.begin (); i != s.end ();i++)
		*i = ToLower (*i);
	return s;
}
template<typename StringType>
void ToUpperInplace (StringType& input)
{
	for (typename StringType::iterator i= input.begin (); i != input.end ();i++)
		*i = ToUpper (*i);
}
template<typename StringType>
void ToLowerInplace (StringType& input)
{
	for (typename StringType::iterator i= input.begin (); i != input.end ();i++)
		*i = ToLower (*i);
}

TAKES_PRINTF_ARGS(1,2) std::string EXPORT_COREMODULE Format (const char* format, ...);
std::string VFormat (const char* format, va_list ap);

void VFormatBuffer (char* buffer, int size, const char* format, va_list ap);
template<typename StringType>
inline TAKES_PRINTF_ARGS(1,2) StringType FormatString(const char* format, ...)
{
	char buffer[10*1024];
	va_list va;
	va_start( va, format );
	VFormatBuffer (buffer, 10*1024, format, va);
	return StringType(buffer);
}

std::string Append (char const* a, std::string const& b);
EXPORT_COREMODULE std::string Append (char const* a, char const* b);
std::string Append (std::string const& a, char const* b);

inline bool IsDigit (char c) { return c >= '0' && c <= '9'; }
inline bool IsAlpha (char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool IsSpace (char c) { return c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' || c == ' '; }
inline bool IsAlphaNumeric (char c) { return IsDigit (c) || IsAlpha (c); }

template<typename alloc>
void replace_string (std::basic_string<char, std::char_traits<char>, alloc>& target,
	const std::basic_string<char, std::char_traits<char>, alloc>& search,
	const std::basic_string<char, std::char_traits<char>, alloc>& replace, size_t startPos = 0)
{
	if (search.empty())
		return;

	typename std::basic_string<char, std::char_traits<char>, alloc>::size_type p = startPos;
	while ((p = target.find (search, p)) != std::basic_string<char, std::char_traits<char>, alloc>::npos)
	{
		target.replace (p, search.size (), replace);
		p += replace.size ();
	}
}

template<typename StringType>
void replace_string (StringType& target, const char* search, const StringType& replace, size_t startPos = 0)
{
	replace_string(target,StringType(search),replace, startPos);
}
template<typename StringType>
void replace_string (StringType& target, const StringType& search, const char* replace, size_t startPos = 0)
{
	replace_string(target,search,StringType(replace), startPos);
}
template<typename StringType>
void replace_string (StringType& target, const char* search, const char* replace, size_t startPos = 0)
{
	replace_string(target,StringType(search),StringType(replace), startPos);
}

#if UNITY_EDITOR || UNITY_FBX_IMPORTER
/// Converts name to UTF8. Returns whether conversion was successful.
/// If not successful name will not be touched
bool AsciiToUTF8 (std::string& name);
std::string StripInvalidIdentifierCharacters (std::string str);
#endif

std::string FormatBytes(SInt64 b);

#if UNITY_OSX || UNITY_IPHONE
CFStringRef StringToCFString (const std::string &str);
std::string CFStringToString (CFStringRef str);
#endif

std::string Trim(const std::string &input, const std::string &ws=" \t");

void HexStringToBytes (char* str, size_t numBytes, void *data);
void BytesToHexString (const void *data, size_t numBytes, char* str);
std::string BytesToHexString (const void* data, size_t numBytes);

int GetNumericVersion (char const* versionCString);
inline int GetNumericVersion (const std::string& versionString) { return GetNumericVersion (versionString.c_str()); }

/// Split a string delimited by splitChar or any character in splitChars into parts.
/// Parts is appended, not cleared.
/// Empty parts are discarded.
void Split (const std::string s, char splitChar, std::vector<std::string> &parts);
void Split (const std::string s, const char* splitChars, std::vector<std::string> &parts);

inline std::string QuoteString( const std::string& str )
{
	return '"' + str + '"';
}

#endif
