#include "UnityPrefix.h"
#include "Word.h"
#include <limits.h>
#include <stdio.h>
//#include "Runtime/Math/FloatConversion.h"
#include "Annotations.h"

#if !UNITY_EXTERNAL_TOOL
#include "LinearAllocator.h"
#endif

using namespace std;

bool BeginsWithCaseInsensitive (const std::string &s, const std::string &beginsWith)
{
	// Note that we don't want to convert the whole s string to lower case, simply because
	// it might not be very efficient (imagine a string that has, e.g., 2 Mb chars), so we take
	// only a substring that we need to compare with the given prefix.
	return BeginsWith(ToLower(s.substr(0, beginsWith.length())), ToLower(beginsWith));
}

bool BeginsWith (const char* s, const char* beginsWith)
{
	return strncmp (s, beginsWith, strlen (beginsWith)) == 0;	
}


#if !WEBPLUG
bool IsStringNumber (const string& s)
{
	return IsStringNumber (s.c_str ());
}
#endif


#if UNITY_EDITOR
/* 
	Used by SemiNumericCompare:
	if c[*index] is > '9', returns INT_MAX-265+c[*index] and increases *index by 1.
	if '0' <= c[*index] <= '9' consumes all numeric characters and returns the numerical value + '0'
	if c[*index] is < '0' returns (int)c[*index] and increases *index by 1;
*/
static int GetSNOrdinal(const char* c, int& index) 
{

	if((unsigned char)c[index] < (unsigned char)'0') 
		return (unsigned char)c[index++];
	else if ((unsigned char)c[index] > (unsigned char)'9') 
		return (INT_MAX-256) + (unsigned char)ToLower(c[index++]);
	else 
	{
		int atoi=0;
		while (c[index] >= '0' && c[index] <= '9') 
		{
			atoi = atoi*10 + (c[index++] - '0'); // TODO: clamp at INT_MAX-256
		}
		return atoi+'0';
	}
}

// Human-like sorting.
// Sorts strings alphabetically, but with numbers in strings numerically, so "xx11" comes after "xx2".
int SemiNumericCompare(const char * str1, const char * str2)
{
	int i1 = 0;
	int i2 = 0;
	int o1, o2;
	
	while ((o1 = GetSNOrdinal(str1, i1)) == (o2 = GetSNOrdinal(str2, i2)))
	{
		if (!o1)
			return i2-i1; // to make strings like "xx1", "xx01" and "xx001" have a stable sorting (longest string first as in Finder)
	}

	return(o1 - o2);
}
#endif

int StrNICmp(const char * str1, const char * str2, size_t n)
{
	const char * p1 = (char *) str1;
	const char * p2 = (char *) str2;
	char c1, c2;
	size_t charsLeft=n;
	
	if( n <= 0 )
		return 0;
	
	while ((c1 = ToLower (*p1)) == (c2 = ToLower (*p2)))
	{
		++p1; ++p2; --charsLeft;
		if (!charsLeft || !c1)
			return 0;
	}

	return(c1 - c2);
}

int StrICmp(const char * str1, const char * str2)
{
	const char * p1 = (char *) str1;
	const char * p2 = (char *) str2;
	char c1, c2;
	
	while ((c1 = ToLower (*p1)) == (c2 = ToLower (*p2)))
	{
		p1++; p2++;
		if (!c1)
			return 0;
	}

	return(c1 - c2);
}

int StrCmp(const char * str1, const char * str2)
{
	const char * p1 = (char *) str1;
	const char * p2 = (char *) str2;
	char c1, c2;

	while ((c1 = (*p1)) == (c2 = (*p2)))
	{
		p1++; p2++;
		if (!c1)
			return 0;
	}

	return(c1 - c2);
}


#if !WEBPLUG
bool IsStringNumber (const char* s)
{
	bool success = false;
	bool hadPoint = false;
	long i = 0;
	
	while (s[i] != '\0')
	{
		switch (s[i])
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			
				success = true;
			
			break;
			
			case '.':
			case ',':
			
				// Make sure we only have one . or ,
				if (hadPoint)
					return false; 
				hadPoint = true;
			
			break;
			
			case '+':
			case '-':
			
			// + or - are only allowed at the beginning of the string
			if (i != 0)
				return false;
				break;
				
			default:
				return false;
			break;	
		}
		i++;
	}
	
	return success;
}
#endif


SInt32 StringToInt (char const* s)
{
	return atol (s);
}

template<typename T>
inline string _ToString (const char* formatString, T value)
{
	char buf[255];
	
	#ifdef WIN32
	_snprintf
	#else
	snprintf
	#endif
		(buf, sizeof(buf), formatString, value);

	return string (buf);
}

string IntToString (SInt32 i)
{
	return _ToString ("%i", i);
}

string UnsignedIntToString (UInt32 i)
{
	return _ToString ("%u", i);
}

string Int64ToString (SInt64 i)
{
	return _ToString ("%lli", i);
}

string UnsignedInt64ToString (UInt64 i)
{
	return _ToString ("%llu", i);
}

string DoubleToString (double i)
{
	return _ToString ("%f", i);
}

string FloatToString (float f, const char* precFormat)
{
	char buf[255];
	
		sprintf (buf, precFormat, f);
	return string (buf);		
}

string SHA1ToString(unsigned char hash[20])
{
	char buffer[41];
	for ( int i=0; i < 20; i++ )
		sprintf(&buffer[i*2], "%.2x", hash[i]);
	return std::string(buffer, 40);
}

string MD5ToString(unsigned char hash[16])
{
	char buffer[33];
	for ( int i=0; i < 16; i++ )
		sprintf(&buffer[i*2], "%.2x", hash[i]);
		return std::string(buffer, 32);
}

// Parses simple float: optional sign, digits, period, digits.
// No exponent or leading whitespace support.
// No locale support.
// We use it to be independent of locale, and because atof() did
// show up in the shader parsing profile.
float SimpleStringToFloat (const char* str, int* outLength)
{
	const char *p = str;

	// optional sign
	bool negative = false;
	switch (*p) {             
		case '-': negative = true; // fall through to increment pointer
		case '+': p++;
	}

	double number = 0.0;

	// scan digits
	while (IsDigit(*p))
	{
		number = number * 10.0 + (*p - '0');
		p++;
	}

	// optional decimal part
	if (*p == '.')
	{
		p++;

		// scan digits after decimal point
		double scaler = 0.1;
		while (IsDigit(*p))
		{
			number += (*p - '0') * scaler;
			scaler *= 0.1;
			p++;
		}
	}

	// apply sign
	if (negative)
		number = -number;

	// Do not check for equality with atof() - results
	// of that are dependent on locale.
	//DebugAssertIf(!CompareApproximately(number, atof(str)));

	if (outLength)
		*outLength = (int) (p - str);

	return float(number);
}


void VFormatBuffer (char* buffer, int size, const char* format, va_list ap)
{
	va_list zp;
	va_copy (zp, ap);
	vsnprintf (buffer, size, format, zp);
	va_end (zp);
}

std::string VFormat (const char* format, va_list ap)
{
	va_list zp;
	va_copy (zp, ap);
	char buffer[1024 * 10];
	vsnprintf (buffer, 1024 * 10, format, zp);
	va_end (zp);
	return buffer;
}

std::string Format (const char* format, ...)
{
	va_list va;
	va_start (va, format);
	std::string formatted = VFormat (format, va);
	va_end (va);
	return formatted;
}

#if UNITY_EDITOR || UNITY_FBX_IMPORTER
bool AsciiToUTF8 (std::string& name)
{
	if (name.empty())
		return true;

	#if UNITY_OSX
	CFStringRef str = CFStringCreateWithCString (NULL, name.c_str(), kCFStringEncodingASCII);
	if (str != NULL)
	{
		bool res = false;
		
		char* tempName;
		ALLOC_TEMP(tempName, char, name.size() * 2);
		if (CFStringGetCString(str, tempName, name.size() * 2, kCFStringEncodingUTF8))
		{
			name = tempName;
			res = true;
		}
		CFRelease(str);
		return res;			
	}

	return false;

	#elif UNITY_WIN

	bool result = false;
	int bufferSize = (int) name.size()*4+1;
	wchar_t* wideBuffer;
	ALLOC_TEMP(wideBuffer, wchar_t, bufferSize);
	if( MultiByteToWideChar( CP_ACP, 0, name.c_str(), -1, wideBuffer, bufferSize ) )
	{
		char* buffer;
		ALLOC_TEMP(buffer, char, bufferSize);
		if( WideCharToMultiByte( CP_UTF8, 0, wideBuffer, -1, buffer, bufferSize, NULL, NULL ) )
		{
			name = buffer;
			result = true;
		}
	}
	return result;

	#elif UNITY_LINUX

	return true; // do nothing for now. Tests should show how much is this
	             // function needed.

	#else
	#error Unknown platform
	#endif
}

std::string StripInvalidIdentifierCharacters (std::string str)
{
	for (unsigned int i=0;i<str.size();i++)
	{
		char c = str[i];
		if (c == '~' || c == '&' || c == '%' || c == '|' || c == '$' || c == '<' || c == '>' || c == '/' || c == '\\')
			str[i] = '_';
	}
	return str;
}
#endif

void HexStringToBytes (char* str, size_t bytes, void *data)
{
	for (size_t i=0; i<bytes; i++)
	{
		UInt8 b;
		char ch = str[2*i+0];
		if (ch <= '9')
			b = (ch - '0') << 4;
		else
			b = (ch - 'a' + 10) << 4;

		ch = str[2*i+1];
		if (ch <= '9')
			b |= (ch - '0');
		else
			b |= (ch - 'a' + 10);
			
		((UInt8*)data)[i] = b;
	}
}

void BytesToHexString (const void *data, size_t bytes, char* str)
{
	static const char kHexToLiteral[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	for (size_t i=0; i<bytes;i++)
	{
		UInt8 b = ((UInt8*)data)[i];
		str[2*i+0] = kHexToLiteral[ b >> 4 ];
		str[2*i+1] = kHexToLiteral[ b & 0xf ];
	}
}

std::string BytesToHexString (const void* data, size_t numBytes)
{
	std::string result;
	result.resize (numBytes * 2);
	BytesToHexString (data, numBytes, &result[0]);
	return result;
}

string FormatBytes(SInt64 b)
{
	AssertIf(sizeof(b) != 8);
	
	if(b < 0)
		return "Unknown";
	if (b < 512)
#if UNITY_64 && UNITY_LINUX
		return Format("%ld B",b);
#else
		return Format("%lld B",b);
#endif
	if (b < 512*1024)
		return Format("%01.1f KB",b / 1024.0);
	
	b /= 1024;
	if (b < 512*1024)
		return Format("%01.1f MB", b / 1024.0);

	b /= 1024;
	return Format("%01.2f GB",b / 1024.0);
}

std::string Append (char const* a, std::string const& b)
{
	std::string r;
	size_t asz = strlen (a);
	r.reserve (asz + b.size ());
	r.assign (a, asz);
	r.append (b);
	return r;
}

std::string Append (char const* a, char const* b)
{
	std::string r;
	size_t asz = strlen (a);
	size_t bsz = strlen (b);
	r.reserve (asz + bsz);
	r.assign (a, asz);
	r.append (b, bsz);
	return r;
}

std::string Append (std::string const& a, char const* b)
{
	std::string r;
	size_t bsz = strlen(b);
	r.reserve(a.size() + bsz);
	r.assign(a);
	r.append(b, bsz);
	return r;
}

#if UNITY_OSX || UNITY_IPHONE
CFStringRef StringToCFString (const std::string &str)
{
	return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
}

std::string CFStringToString (CFStringRef str)
{
	std::string output;
	if (str)
	{
		int bufferSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + 1;
		char *buf;
		MALLOC_TEMP( buf, bufferSize );
		if (CFStringGetCString(str, buf, bufferSize, kCFStringEncodingUTF8))
			output = buf;
	}
	return output;
}
#endif

std::string Trim(const std::string &input, const std::string &ws)
{
	size_t startpos = input.find_first_not_of(ws); // Find the first character position after excluding leading blank spaces
	size_t endpos = input.find_last_not_of(ws); // Find the first character position from reverse af

	// if all spaces or empty return an empty string
	if(( string::npos == startpos ) || ( string::npos == endpos))
	{
		return std::string(); // empty string
	}
	else
	{
		return input.substr( startpos, endpos-startpos+1 );
	}
}


/*
 Convert version string of form XX.X.XcXXX to UInt32 in Apple 'vers' representation
 as described in Tech Note TN1132 (http://developer.apple.com/technotes/tn/pdf/tn1132.pdf)
 eg.
 1.2.0
 1.2.1
 1.2.1a1
 1.2.1b1
 1.2.1r12
 */
int GetNumericVersion (char const* versionCString)
{
	if( (*versionCString)=='\0' )
		return 0;
	
	int major=0,minor=0,fix=0,type='r',release=0;
	const char *spos=versionCString;
	major = *(spos++) - '0';
	if (*spos>='0' && *spos < '9')
		major = major*10 + *(spos++)-'0';
	
	if (*spos)
	{
		if (*spos=='.')
		{
			spos++;
			if(*spos)
				minor=*(spos++)-'0';
		}
		if (*spos)
		{
			if (*spos=='.')
			{
				spos++;
				if (*spos)
					fix=*(spos++)-'0';
			}
			if (*spos)
			{
				type = *(spos++);
				if (*spos)
				{
					release = *(spos++)-'0';
					if (*spos)
					{
						release = release*10+*(spos++)-'0';
						if (*spos)
						{
							release=release*10+*(spos++)-'0';
						}
					}
				}
			}
		}
	}
	
	unsigned int version = 0;
	version |= ((major/10)%10)<<28;
	version |= (major%10)<<24;
	version |= (minor%10)<<20;
	version |= (fix%10)<<16;
	switch( type )
	{
		case 'D':
		case 'd': version |= 0x2<<12;  break;
		case 'A':
		case 'a': version |= 0x4<<12;  break;
		case 'B':
		case 'b': version |= 0x6<<12;  break;
		case 'F':
		case 'R':
		case 'f':
		case 'r': version |= 0x8<<12;  break;
	}
	version |= ((release / 100)%10)<<8;
	version |= ((release / 10) %10)<<4;
	version |=   release % 10;
	
	AssertIf (((int)version) < 0);
	
	return version;
}

void Split (const std::string s, char splitChar, std::vector<std::string> &parts)
{
	int n = 0, n1 = 0;
	while ( 1 )
	{
		n1 = s.find (splitChar, n);
		std::string p = s.substr (n, n1-n);
		if ( p.length () ) 
		{
			parts.push_back (p);
		}	
		if ( n1 == std::string::npos )
			break;
		
		n = n1 + 1;
	}
}

void Split (const std::string s, const char* splitChars, std::vector<std::string> &parts)
{
	int n = 0, n1 = 0;
	while ( 1 )
	{
		n1 = s.find_first_of (splitChars, n);
		std::string p = s.substr (n, n1-n);
		if ( p.length () ) 
		{
			parts.push_back (p);
		}	
		if ( n1 == std::string::npos )
			break;
		
		n = n1 + 1;
	}
}
