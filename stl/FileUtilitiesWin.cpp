#include "UnityPrefix.h"
#include "FileUtilities.h"
#include "Word.h"
#include "GUID.h"
#include <ShellAPI.h>
#include "File.h"
#include "PathUnicodeConversion.h"
#include "WinUtils.h"
#include "Argv.h"

#if UNITY_EDITOR

#define DEBUG_ACTUAL_FILE_NAME 0


//#include "Editor/Src/DisplayDialog.h"
#include <Psapi.h>
#include <Shlwapi.h>
#include <Shlobj.h>

HANDLE OpenFileWithPath( const string& path, File::Permission permission ); // File.cpp



bool RemoveReadOnlyA(LPCSTR path)
{
	DWORD attributes = GetFileAttributesA(path);

	if (INVALID_FILE_ATTRIBUTES != attributes)
	{
		attributes &= ~FILE_ATTRIBUTE_READONLY;
		return SetFileAttributesA(path, attributes);
	}

	return false;
}

bool RemoveReadOnlyW(LPCWSTR path)
{
	DWORD attributes = GetFileAttributesW(path);

	if (INVALID_FILE_ATTRIBUTES != attributes)
	{
		attributes &= ~FILE_ATTRIBUTE_READONLY;
		return SetFileAttributesW(path, attributes);
	}

	return false;
}



string GetProjectRelativePath (const string& path)
{
	string lowerPath = ToLower (path);
	if (lowerPath.find (ToLower (File::GetCurrentDirectory ()) + '/') != 0)
		return string ();
	int offset = File::GetCurrentDirectory ().size () + 1;
	string newPath (path.begin () + offset, path.end ());	
	return StandardizePathName (newPath);
}


static std::string GetActualPathNameViaFind( const wchar_t* path )
{
	WIN32_FIND_DATAW findData;
	HANDLE hFind;
	wchar_t searchbuffer[kDefaultPathBufferSize];
	string result;

	memset(searchbuffer, 0, sizeof(searchbuffer));
	const int pathLength = wcslen(path);

	int i = 0;
	while (i < pathLength)
	{
		// skip until path separator
		int k = i;
		while( k < pathLength && path[k] != L'\\' )
			++k;

		if ((k < pathLength) || ((k >= pathLength) && (k != i)))
		{
			wcsncpy(searchbuffer + i, path + i, k - i);

			if (k > 0 && searchbuffer[k-1] != L':')
			{
				searchbuffer[k] = L'*';
				searchbuffer[k+1] = 0;

				hFind = ::FindFirstFileW( searchbuffer, &findData );

				// FindFirstFile can match short MS-DOS names as well!
				// So here, check if the actual file name matches our buffer, and treat as "not found"
				// if it isn't.
				//
				// MSDN says that Windows 7 and later have kFindExInfoBasic flag for FindFirstFileEx
				// which makes it not look into short names, but that doesn't seem to work (it correctly
				// returns null for result short name, but still matches it).
				if (hFind != INVALID_HANDLE_VALUE)
				{
					if (_wcsnicmp(findData.cFileName, searchbuffer + i, k-i) != 0)
					{
						FindClose(hFind);
						hFind = INVALID_HANDLE_VALUE;
					}
				}

				if( hFind == INVALID_HANDLE_VALUE )
				{
#if DEBUG_ACTUAL_FILE_NAME
					char bufferUTF8[kDefaultPathBufferSize];
					ConvertWindowsPathName(searchbuffer, bufferUTF8, kDefaultPathBufferSize);
					printf_console("  FindFirstFile: not found '%s'\n", bufferUTF8);
#endif
					// File or folder does not exist.
					// Still, return the folders that were successfully found up to this.
					memcpy(&searchbuffer[k], &path[k], (pathLength-k+1)*sizeof(path[0]));
					break;
				}

				wcsncpy(searchbuffer + i, findData.cFileName, k - i);

				FindClose(hFind);
			}
			else
			{
				searchbuffer[0] = (wchar_t)ToUpper((char)searchbuffer[0]); // drive letter
			}

			searchbuffer[k] = (k < pathLength) ? L'\\' : 0;
		}

		i = k + 1;
	}

	ConvertWindowsPathName(searchbuffer, result);
	return result;
}

static void SafePathCanonicalize( const wchar_t* src, wchar_t* dst, int byteLength )
{
	// leave empty paths empty
	if( src[0] == 0 )
	{
		dst[0] = 0;
		return;
	}

	/*if( PathCanonicalizeW(dst, src) )
	return;*/

	memcpy(dst, src, byteLength);
}


// Previously attempted implementations:
//
// GetShortPathName() followed by GetLongPathName(). This turns out to be about 2x slower than the Find approach,
// and probably causes mysterious failures on some funky file systems (e.g. samba shared drivers that are actually on OS X).
//
// SHGetFileInfo() approach, from suggestion in
// http://stackoverflow.com/questions/74451/getting-actual-file-name-with-proper-casing-on-windows
// This has problems of mangling the names (network names turn into human-readable ones, file extensions are stripped if they
// are turned off in Explorer, etc.).
string GetActualPathSlow (const string & path)
{
	const wchar_t kSeparator = L'\\';
	wchar_t bufferRaw[kDefaultPathBufferSize];
	ConvertUnityPathName(path.c_str(), bufferRaw, kDefaultPathBufferSize);

	// First canonicalize the path
	wchar_t buffer[kDefaultPathBufferSize];
	SafePathCanonicalize( bufferRaw, buffer, sizeof(bufferRaw) );

	std::string res = GetActualPathNameViaFind(buffer);

#if DEBUG_ACTUAL_FILE_NAME
	char bufferUTF8[kDefaultPathBufferSize];
	ConvertWindowsPathName (buffer, bufferUTF8, kDefaultPathBufferSize);
	char curDir[kDefaultPathBufferSize];
	curDir[0] = 0;
	GetCurrentDirectoryA(kDefaultPathBufferSize, curDir);
	printf_console("GetActualPathName: path='%s' canonical='%s' curdir='%s' res='%s'\n", path.c_str(), bufferUTF8, curDir, res.c_str());
#endif

	return res.empty() ? path : res;
}


string GetActualAbsolutePathSlow (const string & path)
{
	return GetActualPathSlow( PathToAbsolutePath(path) );
}

bool CreateDirectorySafe (const string& pathName)
{
	while (true)
	{
		if (CreateDirectory (pathName))
			return true;
		else
		{
			/*int result = DisplayDialogComplex ("Creating directory", "Creating directory " + pathName + " failed. Please ensure there is enough disk space and you have permissions setup correctly.", "Try Again", "Cancel", "Quit");
			if (result == 1)
				return false;
			else if (result == 2)
				exit(1);*/
		}
	}
	return false;
}

bool WriteStringToFile (const string& contents, const string& path, AtomicWriteMode atomicMode, UInt32 fileFlags)
{
	// Figure where to write the temp file
	UnityGUID guid; guid.Init();

	string tmpFilePath;
	if (atomicMode == kProjectTempFolder)
	{
		tmpFilePath = "Temp/UnityTempWriteStringToFile-" + GUIDToString(guid);
	}
	else if (atomicMode == kNotAtomic)
	{
		DeleteFile(path);
		tmpFilePath = path;
	}
	else
	{
		ErrorString("Unknown atomic mode");
	}

	// Write the file	
	{
		File tmpFile;
		if( !tmpFile.Open(tmpFilePath, File::kWritePermission) )
			return false;

		if (atomicMode != kNotAtomic)
			SetFileFlags(tmpFilePath, kAllFileFlags, kFileFlagDontIndex | kFileFlagTemporary); // this is temporary file, mark it so
		else
			SetFileFlags(tmpFilePath, kAllFileFlags, fileFlags); // set flags on final file

		const char* data = &*contents.begin ();
		if (!tmpFile.Write (data, contents.size ()))
			return false;

		if (!tmpFile.Close())
			return false;
	}

	// move-replace the file
	if (atomicMode != kNotAtomic)
	{
		bool success = MoveReplaceFile(tmpFilePath, path);
		if( success )
			SetFileFlags(path, kFileFlagDontIndex | kFileFlagTemporary, fileFlags); // set flags on final file
		return success;
	}
	else
	{
		return true;
	}
}


bool MoveToTrash (const std::string& inPath)
{
	if (!IsFileCreated (inPath) && !IsDirectoryCreated (inPath))
		return false;

	string path = PathToAbsolutePath(inPath);

	wchar_t widePath[kDefaultPathBufferSize+1];
	memset( widePath, 0, sizeof(widePath) );
	ConvertUnityPathName( path.c_str(), widePath, kDefaultPathBufferSize );

	SHFILEOPSTRUCTW op;
	op.hwnd = NULL;
	op.wFunc = FO_DELETE;
	op.pFrom = widePath;
	op.pTo = NULL;
	op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_ALLOWUNDO;
	op.hNameMappings = NULL;
	op.lpszProgressTitle = NULL;
	SHFileOperationW( &op );

	// The above doesn't work when performed over the network.
	// In that case we just kill it immediately.
	if (IsFileCreated (inPath) || IsDirectoryCreated (inPath))
	{
		DeleteFileOrDirectory(inPath);
	}

	return !IsFileCreated (inPath) && !IsDirectoryCreated (inPath);
}

string DeleteLastPathNameComponentWithAnySlash (const string& pathName)
{
	string::size_type p = pathName.rfind ('\\');
	string::size_type p2 = pathName.rfind ('/');

	if (p == string::npos && p2 == string::npos)
		return string ();

	if (p == string::npos || p2 == string::npos)
		return string (&pathName[0], p == string::npos ? p2 : p);

	return string (&pathName[0], std::max(p, p2));
}

bool CopyFileOrDirectory (const string& from, const string& to)
{
	wchar_t wideFrom[kDefaultPathBufferSize], wideTo[kDefaultPathBufferSize];
	ConvertUnityPathName( PathToAbsolutePath(from).c_str(), wideFrom, kDefaultPathBufferSize );
	ConvertUnityPathName( PathToAbsolutePath(to).c_str(), wideTo, kDefaultPathBufferSize );

	while (true)
	{
		// Windows Vista and 7 will recursively create the full path which we
		// don't want for compatibility with OSX.
		string parentDirectory = DeleteLastPathNameComponentWithAnySlash(to);
		if(!IsDirectoryCreated(parentDirectory))
			return false;

		if(IsDirectoryCreated(from) && IsDirectoryCreated(to))
		{
			// If we're copying from a directory and the destination directory
			// already exists, we should fail.
			return false;
		}

		SHFILEOPSTRUCTW fileOp;
		ZeroMemory(&fileOp, sizeof(fileOp));

		wideFrom[wcslen(wideFrom)+1] = 0; // double null terminated strings
		wideTo[wcslen(wideTo)+1] = 0;    

		fileOp.wFunc = FO_COPY;
		fileOp.pFrom = wideFrom;
		fileOp.pTo = wideTo;
		fileOp.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOCONFIRMMKDIR;

		int err = SHFileOperationW(&fileOp);

		if(err == 0)
			return true;

		if(IsBatchmode())
			return false;

		/*int result = DisplayDialogComplex (Format("Copying file failed (%d)", err), "Copying "+from+" to "+to, "Try Again", "Force Quit", "Cancel");
		if (result == 1)
			exit(1);
		else if (result == 2)
			return false;*/
	}
}

bool CopyFileOrDirectoryCheckPermissions (const string& from, const string& to, bool &accessdenied)
{
	wchar_t wideFrom[kDefaultPathBufferSize], wideTo[kDefaultPathBufferSize];
	ConvertUnityPathName( PathToAbsolutePath(from).c_str(), wideFrom, kDefaultPathBufferSize );
	ConvertUnityPathName( PathToAbsolutePath(to).c_str(), wideTo, kDefaultPathBufferSize );

	int result = 0;

	while (true)
	{
		SHFILEOPSTRUCTW fileOp;
		ZeroMemory(&fileOp, sizeof(fileOp));

		wideFrom[wcslen(wideFrom)+1] = 0; // double null terminated strings
		wideTo[wcslen(wideTo)+1] = 0;

		fileOp.wFunc = FO_COPY;
		fileOp.pFrom = wideFrom;
		fileOp.pTo = wideTo;
		fileOp.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOCONFIRMMKDIR;

		result = SHFileOperationW(&fileOp);

		accessdenied = result == ERROR_ACCESS_DENIED;

		if (result == 0)
			return true;

#if UNITY_EDITOR
		if (!accessdenied)
		{
			/*result = DisplayDialogComplex ("Copying file failed", "Copying "+from+" to "+to, "Try Again", "Force Quit", "Cancel");
			if (result == 1)
			exit(1);
			else 
			if (result == 2)
			return false;*/
		}
		else
		{
			return false;
		}
#else
		return false;
#endif
	}
}


bool DeleteFile (const string& path)
{
	string absolutePath = PathToAbsolutePath (path);
	if (IsFileCreated (absolutePath))
	{
		wchar_t widePath[kDefaultPathBufferSize];
		ConvertUnityPathName( absolutePath.c_str(), widePath, kDefaultPathBufferSize );

		if (!DeleteFileW( widePath ))
		{
#if UNITY_EDITOR
			if (ERROR_ACCESS_DENIED == GetLastError())
			{
				if (RemoveReadOnlyW(widePath))
				{
					return (FALSE != DeleteFileW(widePath));
				}
			}
#endif

			return false;
		}

		return true;
	}
	else
		return false;
}

static bool RemoveDirectoryRecursiveWide( const std::wstring& path )
{
	if( path.empty() )
		return false;

	// base path
	std::wstring basePath = path;
	if( basePath[basePath.size()-1] != L'\\' )
		basePath += L'\\';

	// search pattern: anything inside the directory
	std::wstring searchPat = basePath + L'*';

	// find the first file
	WIN32_FIND_DATAW findData;
	HANDLE hFind = ::FindFirstFileW( searchPat.c_str(), &findData );
	if( hFind == INVALID_HANDLE_VALUE )
		return false;

	bool hadFailures = false;

	bool bSearch = true;
	while( bSearch )
	{
		if( ::FindNextFileW( hFind, &findData ) )
		{
			if( wcscmp(findData.cFileName,L".")==0 || wcscmp(findData.cFileName,L"..")==0 )
				continue;

			std::wstring filePath = basePath + findData.cFileName;
			if( (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
			{
				// we have found a directory, recurse
				if( !RemoveDirectoryRecursiveWide( filePath ) ) {
					hadFailures = true;
				} else {
					::RemoveDirectoryW( filePath.c_str() ); // remove the empty directory
				}
			}
			else
			{
				if( findData.dwFileAttributes & FILE_ATTRIBUTE_READONLY )
					::SetFileAttributesW( filePath.c_str(), FILE_ATTRIBUTE_NORMAL );
				if( !::DeleteFileW( filePath.c_str() ) ) {
					hadFailures = true;
				}
			}
		}
		else
		{
			if( ::GetLastError() == ERROR_NO_MORE_FILES )
			{
				bSearch = false; // no more files there
			}
			else
			{
				// some error occurred
				::FindClose( hFind );
				return false;
			}
		}
	}
	::FindClose( hFind );

	if( !RemoveDirectoryW( path.c_str() ) ) { // remove the empty directory
		hadFailures = true;
	}

	return !hadFailures;
}

static bool RemoveDirectoryRecursive( const std::string& pathUtf8 )
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( pathUtf8.c_str(), widePath, kDefaultPathBufferSize );
	return RemoveDirectoryRecursiveWide( widePath );
}

bool CopyReplaceFile (string from, string to)
{
	if (PathToAbsolutePath (from) == PathToAbsolutePath (to))
		return true;

	if (!IsFileCreated (from))
		return false;

	DeleteFile (to);
	return CopyFileOrDirectory (from, to);
}

string ResolveSymlinks (const string& pathName)
{
	// leave empty paths empty
	if (pathName.empty())
		return pathName;

	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName(pathName.c_str(), widePath, kDefaultPathBufferSize);

	wchar_t canonicalPath[kDefaultPathBufferSize];
	/*if( PathCanonicalizeW(canonicalPath, widePath) )
	{
	std::string res;
	ConvertWindowsPathName( canonicalPath, res );
	DebugAssertIf(res.find("..") != std::string::npos);
	return res;
	}*/

	DebugAssertIf(pathName.find("..") != std::string::npos);
	return pathName;
}

bool IsSymlink (const string& pathName)
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName(pathName.c_str(), widePath, kDefaultPathBufferSize);

	DWORD const attributes = GetFileAttributesW(widePath);

	// We had a discussion with Ignas whether it is necessary to do an assert here.
	// There was no definite conclusion, because on one hand one would expect this
	// function to return true then and only then when a given path is a symlink;
	// in all other cases one would expect to be returned false sans much ado.
	// On the other hand, this assert catches the cases when, e.g., tests try to
	// refer to non-existent files, which would be an error in program logic.
	//Assert(INVALID_FILE_ATTRIBUTES != attributes);

	if (INVALID_FILE_ATTRIBUTES == attributes)
	{
		return false;
	}

	return (0 != (attributes & FILE_ATTRIBUTE_REPARSE_POINT));
}

std::string GenerateUniquePath (string inPath)
{
	if (!IsDirectoryCreated(DeleteLastPathNameComponent (inPath)))
		return "";

	if (!IsFileCreated (inPath) && !IsDirectoryCreated (inPath))
		return inPath;

	string pathName = inPath;
	string pathNoExtension = DeletePathNameExtension (pathName);
	string extension = GetPathNameExtension (pathName);

	int i = 1;

	if (!pathNoExtension.empty ())
	{
		string::size_type pos = pathNoExtension.size ();
		while (IsDigit (*(pathNoExtension.begin () + pos - 1)))
			pos--;

		if (pos == pathNoExtension.size ())
			pathNoExtension = pathNoExtension + " ";
		else
		{
			i = StringToInt (string (pathNoExtension.begin () + pos, pathNoExtension.end ()));
			pathNoExtension = string (pathNoExtension.begin (), pathNoExtension.begin () + pos);
		}		
	}
	else
		pathNoExtension = pathNoExtension + " ";

	int timeout = 0;	
	while (i < 10000)
	{
		string uniquePath = pathNoExtension + IntToString (i);
		if (!extension.empty ())
			uniquePath = AppendPathNameExtension (uniquePath, extension);

		if (!IsFileCreated (uniquePath) && !IsDirectoryCreated (uniquePath))
			return uniquePath;

		timeout++;
		i++;		
	}

	return string();
}

string GenerateUniquePathSafe (const string& pathName)
{
	while (true)
	{
		string uniquePath = GenerateUniquePath (pathName);
		if (!uniquePath.empty())
			return uniquePath;
		else
		{
			/*int result = DisplayDialogComplex ("Creating unique file", "Creating file " + pathName + " failed. Please ensure there is enough disk space and you have permissions setup correctly.", "Try Again", "Cancel", "Quit");
			if (result == 1)
			return "";
			else if (result == 2)
			exit(1);*/
		}
	}
	return "";
}

DateTime GetContentModificationDate (const string& pathName)
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( pathName.c_str(), widePath, kDefaultPathBufferSize );

	WIN32_FILE_ATTRIBUTE_DATA data;
	if( GetFileAttributesExW( widePath, GetFileExInfoStandard, &data ) )
	{
		DateTime out;
		FileTimeToUnityTime( data.ftLastWriteTime, out );
		return out;
	}

	return DateTime();
}

bool SetContentModificationDateToCurrentTime (const string& path)
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName(path.c_str(), widePath, kDefaultPathBufferSize);

	HANDLE fileHandle = CreateFileW(widePath, GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_WRITE_ATTRIBUTES, 0);
	if (fileHandle == INVALID_HANDLE_VALUE)
		return false;

	SYSTEMTIME systemTime;
	GetSystemTime(&systemTime);

	FILETIME fileTime;
	SystemTimeToFileTime(&systemTime, &fileTime);

	BOOL ret = SetFileTime(fileHandle, NULL, NULL, &fileTime);
	CloseHandle(fileHandle);

	return (ret != 0);
}

void FileTimeToUnityTime( const FILETIME& ft, DateTime& dateTime )
{
	// File timestamps on OS X are seconds + fractional seconds since 1904 (UTCDateTime).
	// Windows FILETIME values are 100-nanoseconds since 1601.
	//
	// Seconds from 1904 to 2001: 3061152000 (kCFAbsoluteTimeIntervalSince1904)
	// Seconds from 1970 to 2001: 978307200 (kCFAbsoluteTimeIntervalSince1970)
	// Thus seconds from 1904 to 1970: 2082844800
	// Seconds from 1601 to 1970: 11644473600 (MSDN KBKB167296)
	//
	// So to get from FILETIME to OS X time, we need to subtract 9561628800
	// from seconds.

	ULARGE_INTEGER timeValue;
	timeValue.LowPart = ft.dwLowDateTime;
	timeValue.HighPart = ft.dwHighDateTime;

	ULONGLONG seconds = timeValue.QuadPart / 10000000;
	seconds -= 9561628800L;
	dateTime.fraction = timeValue.QuadPart % 10000000;
	dateTime.lowSeconds = seconds & 0xFFFFFFFF;
	dateTime.highSeconds = seconds >> 32;
}


string GetApplicationContentsPath()
{
	return AppendPathName(GetApplicationFolder(), "Data");
}

string GetApplicationManagedPath()
{
	return AppendPathName(GetApplicationContentsPath(), "Managed");
}

bool MoveFileOrDirectory (const string& fromPath, const string& toPath)
{
	bool destinationFileCreated = IsFileCreated (toPath);
	bool destinationDirectoryCreated = IsDirectoryCreated (toPath);
	bool namesIdentical = ToLower(fromPath) == ToLower(toPath);

	// Cannot rename file or folder into something that already exists
	if ((destinationFileCreated || destinationDirectoryCreated) && !namesIdentical)
		return false;

	// OSX compatibility. Fail if parent folder is not created
	if (!IsDirectoryCreated(DeleteLastPathNameComponent(toPath)))
		return false;

	// Folder renaming has to be handled differently otherwise Windows thinks we are going to move into a subfolder of itself which is impossible
	bool folderRenaming = false;
	if(destinationDirectoryCreated && namesIdentical)
		folderRenaming = true;

	while (true)
	{
		// strings must end with double zeros for SHFileOperation
		wchar_t wideFrom[kDefaultPathBufferSize+1], wideTo[kDefaultPathBufferSize+1];
		memset(wideFrom, 0, sizeof(wideFrom));
		memset(wideTo, 0, sizeof(wideTo));
		ConvertUnityPathName( PathToAbsolutePath(fromPath).c_str(), wideFrom, kDefaultPathBufferSize );
		ConvertUnityPathName( PathToAbsolutePath(toPath).c_str(), wideTo, kDefaultPathBufferSize );

		SHFILEOPSTRUCTW op;
		op.hwnd = NULL;
		op.wFunc = folderRenaming ? FO_RENAME : FO_MOVE;
		op.pFrom = wideFrom;
		op.pTo = wideTo;
		op.fFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION;
		op.hNameMappings = NULL;
		op.lpszProgressTitle = NULL;
		int errorCode = SHFileOperationW( &op );
		bool success = (errorCode==0 || errorCode==0x71); // 0x71 = DE_SAMEFILE
		if( op.fAnyOperationsAborted )
			success = false;

		if( success )
			return true;

#if UNITY_EDITOR
		/*int result = DisplayDialogComplex ("Moving file failed", "Moving "+fromPath+" to "+toPath, "Try Again", "Force Quit", "Cancel");
		if (result == 1)
		exit(1);
		else if (result == 2)
		return false;*/
#else
		return false;
#endif
	}
	return false;
}

string GetUserAppDataFolder ()
{
	wchar_t widePath[MAX_PATH];
	if( SUCCEEDED(SHGetFolderPathW( NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, widePath )) )
	{
		std::string path;
		ConvertWindowsPathName( widePath, path );
		path = AppendPathName( path, "Unity" );
		CreateDirectory( path );
		return path;
	}
	return string();
}

string GetUserAppCacheFolder ()
{
	std::string path = GetUserAppDataFolder();
	if ( path.empty() )
		return path;
	path = AppendPathName( path, "Caches" );
	CreateDirectory( path );
	return path;
}	

#endif

bool CreateFile( const string& path, int creator, int fileType )
{
	string parentPath = DeleteLastPathNameComponent(path);
	if( !IsDirectoryCreated(parentPath) )
		return false;

	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( path.c_str(), widePath, kDefaultPathBufferSize );
#if UNITY_EDITOR
	RemoveReadOnlyW(widePath);
#endif
	HANDLE hfile = CreateFileW( widePath, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0 );
	if( hfile != INVALID_HANDLE_VALUE )
	{
		CloseHandle( hfile );
		return true;
	}
	return false;
}



// -------------------------------------------------------------------


#if ENABLE_UNIT_TESTS


static void BenchmarkGetActualPathSlow()
{
	double GetTimeSinceStartup();

	std::set<std::string> paths;
	std::set<std::string> paths2;
	GetDeepFolderContentsAtPath(GetApplicationContentsPath(), paths);
	std::set<std::string>::iterator it;
	for (it = paths.begin(); it != paths.end(); ++it)
		paths2.insert( ToUpper(*it) );

	const int kIterations = 10;

	size_t sum = 0;
	double t0 = GetTimeSinceStartup();
	for (int i = 0; i < kIterations; ++i)
	{
		for (it = paths2.begin(); it != paths2.end(); ++it)
		{
			sum += GetActualPathSlow(*it).size();
		}
	}
	double t1 = GetTimeSinceStartup();
	printf_console("GetActualPathName: %.2fs for %i paths (%.4f ms per path) sum=%i\n", t1-t0, paths2.size()*kIterations, (t1-t0) / (paths2.size()*kIterations) * 1000.0, sum);
	// Win 7 64 bit, Core i7 2600K, SSD disk:
	// GetActualPathName: 3.08s for 20480 paths (0.1506 ms per path) sum=1994800
}


static void CheckFN(const std::string& aaa, const std::string& bbb)
{
	std::string res = GetActualPathSlow(aaa);
	Assert(res == bbb);
}

void TestGetActualPathWindows()
{
	// Current directory is the root of the project folder at this point

	char curdir[1000];
	GetCurrentDirectoryA(1000, curdir);
	std::string curDirUnity = curdir;
	ConvertSeparatorsToUnity(curDirUnity);
	if (curdir[1] == ':')
	{
		std::string drive; drive += curdir[0];
		std::string drivel; drivel += ToLower(curdir[0]);
		CheckFN(drivel+":", drive+":/");
		CheckFN(drive+":", drive+":/");
		CheckFN(drive+":/", drive+":/");
		CheckFN(drive+":\\", drive+":/");
	}
	CheckFN (curdir, curDirUnity);

	CheckFN ("", "");
	CheckFN ("assets", "Assets");
	CheckFN ("AsseTS", "Assets");
	CheckFN ("assets/", "Assets/");
	CheckFN ("temp", "Temp");
	CheckFN ("_NonExistingFile_tp2hKw7THL163Re", "_NonExistingFile_tp2hKw7THL163Re");

	CreateDirectory("Temp/GetActualPathSlowTest");
	CheckFN ("temp/getactualpathslowtest", "Temp/GetActualPathSlowTest");
	CheckFN ("temp/_NonExistingFile_tp2hKw7THL163Re", "Temp/_NonExistingFile_tp2hKw7THL163Re");

	WriteStringToFile("", "Temp/GetActualPathSlowTest/SomeFile.txt", kNotAtomic, kFileFlagTemporary|kFileFlagDontIndex);
	CheckFN ("temp/getactualpathslowt/somefile.txt", "Temp/GetActualPathSlowT/somefile.txt");
	CheckFN ("temp/getactualpathslowtest/some", "Temp/GetActualPathSlowTest/Some");
	CheckFN ("temp/getactualpathslowtest/somefile", "Temp/GetActualPathSlowTest/SomeFile");
	CheckFN ("temp/getactualpathslowtest/somefile.txt", "Temp/GetActualPathSlowTest/SomeFile.txt");
	CheckFN ("temp/getactualpathslowtest/somefile.TXT", "Temp/GetActualPathSlowTest/SomeFile.txt");
	CheckFN ("temp/getactualpathslowtest/somefile.txt/", "Temp/GetActualPathSlowTest/SomeFile.txt/");
	CheckFN ("temp/getactualpathslowtest/somefile.tx", "Temp/GetActualPathSlowTest/SomeFile.tx");
	CheckFN ("temp/getactualpathslowtest/somefile.txta", "Temp/GetActualPathSlowTest/somefile.txta");

	WriteStringToFile("", "Temp/GetActualPathSlowTest/With Spaces.txt", kNotAtomic, kFileFlagTemporary|kFileFlagDontIndex);
	CheckFN ("temp/getactualpathslowtest/with spaces.txt", "Temp/GetActualPathSlowTest/With Spaces.txt");

	// There was a bug where search for "withs" would find an existing file with name like "With Spaces",
	// trim result to original length and return "With ". All because DOS shortname for "With Spaces"
	// (something like "withs~1") actually matches "withs" prefix.
	CheckFN ("temp/getactualpathslowtest/withs", "Temp/GetActualPathSlowTest/withs");

	//BenchmarkGetActualPathSlow();
}


#endif // #if ENABLE_UNIT_TESTS
