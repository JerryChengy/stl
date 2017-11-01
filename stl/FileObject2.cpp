#include "UnityPrefix.h"
#include "FileUtilities.h"
#include "PathNameUtility.h"
#include "PathUnicodeConversion.h"

string GetApplicationFolder ()
{
	return DeleteLastPathNameComponent (GetApplicationPath ());
}

string GetApplicationPath ()
{
	wchar_t buffer[kDefaultPathBufferSize];
	GetModuleFileNameW( 0, buffer, kDefaultPathBufferSize );

	std::string result;
	ConvertWindowsPathName( buffer, result );
	return result;
}
