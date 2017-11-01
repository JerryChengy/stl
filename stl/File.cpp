#include "UnityPrefix.h"
#include "File.h"
#include "PathNameUtility.h"
#include <string.h>
#include "PathUnicodeConversion.h"
#include "WinUtils.h"
#if UNITY_EDITOR
//#include "EditorUtility.h"
#endif
#if UNITY_WINRT
#include "PlatformDependent/MetroPlayer/FileWinHelpers.h"
#endif
#if !UNITY_WP8
#include <shlwapi.h>
#endif
#include "Thread.h"

using namespace std;

#if UNITY_EDITOR
std::string GetFormattedFileError (const std::string& operation, DWORD errorNo);
bool RemoveReadOnlyA(LPCSTR path);
bool RemoveReadOnlyW(LPCWSTR path);
#endif

static string gCurrentDirectory;

HANDLE OpenFileWithPath( const string& path, File::Permission permission )
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( path.c_str(), widePath, kDefaultPathBufferSize );
	DWORD accessMode, shareMode, createMode;
	switch( permission ) {
	case File::kReadPermission:
		accessMode = FILE_GENERIC_READ;
		shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		createMode = OPEN_EXISTING;
		break;
	case File::kWritePermission:
		accessMode = FILE_GENERIC_WRITE;
		shareMode = 0;
		createMode = CREATE_ALWAYS;
		break;
	case File::kAppendPermission:
		accessMode = FILE_GENERIC_WRITE;
		shareMode = 0;
		createMode = OPEN_ALWAYS;
		break;
	case File::kReadWritePermission:
		accessMode = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
		shareMode = 0;
		createMode = OPEN_ALWAYS;
		break;
	default:
		AssertString("Unknown permission mode");
		return INVALID_HANDLE_VALUE;
	}
	HANDLE fileHandle = CreateFileW( widePath, accessMode, shareMode, NULL, createMode, NULL, NULL );
#if UNITY_EDITOR
	if (INVALID_HANDLE_VALUE == fileHandle)
	{
		if (FILE_GENERIC_WRITE == (accessMode & FILE_GENERIC_WRITE))
		{
			DWORD lastError = GetLastError();
			if (ERROR_ACCESS_DENIED == lastError)
			{
				if (RemoveReadOnlyW(widePath))
				{
					fileHandle = CreateFileW(widePath, accessMode, shareMode, NULL, createMode, NULL, NULL);
				}
				else
					SetLastError (lastError);
			} else
				SetLastError (lastError);
		}
	}
#endif
	if (permission == File::kAppendPermission && fileHandle != INVALID_HANDLE_VALUE)
		SetFilePointer(fileHandle, 0, NULL, FILE_END);
	return fileHandle;
}

bool IsAbsoluteFilePath( const std::string& path )
{
	// this function can be called with both Unity style and Windows style path names
	if( path.empty() )
		return false;

	if( path[0] == kPlatformPathNameSeparator || path[0] == kPathNameSeparator )
		return true; // network paths are absolute
	if( path.size() >= 2 && path[1] == ':' )
		return true; // drive letter followed by colon is absolute

	return false;
}


string PathToAbsolutePath (const string& path)
{
	if( IsAbsoluteFilePath(path) )
		return path;
	else
		return AppendPathName (File::GetCurrentDirectory (), path);
}

bool ReadFromFile (const string& pathName, void *data, int fileStart, int byteLength)
{
	winutils::AutoHandle file = OpenFileWithPath( pathName, File::kReadPermission );
	if (file.handle == INVALID_HANDLE_VALUE)
		return false;

	DWORD fileSize = GetFileSize( file.handle, NULL );
	if( fileSize == INVALID_FILE_SIZE )
		return false;

	if( fileSize < byteLength )
		return false;

	DWORD moved = SetFilePointer( file.handle, fileStart, NULL, FILE_BEGIN );
	if( moved == INVALID_SET_FILE_POINTER )
	{
		DWORD error = GetLastError();
		if( error != NO_ERROR )
			return false;
	}

	DWORD readLength = 0;
	if( !ReadFile( file.handle, data, byteLength, &readLength, NULL ) )
		return false;

	if( readLength != byteLength )
		return false;

	return true;
}

bool ReadStringFromFile (InputString* outData, const string& pathName)
{
	winutils::AutoHandle file = OpenFileWithPath( pathName, File::kReadPermission );
	if (file.handle == INVALID_HANDLE_VALUE)
		return false;

	DWORD fileSize = GetFileSize( file.handle, NULL );
	if( fileSize == INVALID_FILE_SIZE )
		return false;

	outData->resize( fileSize );
	DWORD readLength = 0;
	if (fileSize == 0)
	{
		outData->clear();
		return true;
	}

	if (!ReadFile( file.handle, &*outData->begin(), fileSize, &readLength, NULL ) )
	{
		outData->clear();
		return false;
	}

	if (readLength != fileSize )
	{
		outData->clear();
		return false;
	}
	return true;
}

bool WriteBytesToFile (const void *data, int byteLength, const string& pathName)
{
	File file;
	if (!file.Open(pathName, File::kWritePermission))
		return false;

	bool success = file.Write(data, byteLength);

	file.Close();

	return success;
}

int File::GetFileLength()
{
	return ::GetFileLength(m_Path);
}

int GetFileLength (const string& pathName)
{
	winutils::AutoHandle file = OpenFileWithPath( pathName, File::kReadPermission );
	if (file.handle == INVALID_HANDLE_VALUE)
		return 0;

	DWORD fileSize = GetFileSize( file.handle, NULL );
	if( fileSize == INVALID_FILE_SIZE )
		return 0;

	return fileSize;
}


File::File()
	:	m_FileHandle(INVALID_HANDLE_VALUE)
	,	m_Position(0)
{
}

File::~File()
{
	AssertIf( m_FileHandle != INVALID_HANDLE_VALUE );
}


#if UNITY_EDITOR
string GetFormattedFileError( const std::string& operation, DWORD errorNo)
{
	return "";
}
#endif

static bool HandleFileError( const char* title, const string& operation, DWORD errorNo )
{
#if UNITY_EDITOR

	/*int result = DisplayDialogComplex (title, GetFormattedFileError(operation, errorNo), "Try Again", "Force Quit", "Cancel");
	SetLastError( NO_ERROR );
	if (result == 1)
	exit(1);
	else if (result == 2)
	return false;
	else*/
		return true;

#else

	return false;

#endif
}

bool File::Open (const std::string& path, File::Permission perm, AutoBehavior behavior)
{
	Close();
	m_Path = path;

	int retryCount = 5;

	while (true)
	{
		m_FileHandle = OpenFileWithPath( path, perm );
		m_Position = 0;
		if( m_FileHandle != INVALID_HANDLE_VALUE )
		{
			if (perm == kAppendPermission && // Append opens at the end of file
				(m_Position = SetFilePointer(m_FileHandle, 0, NULL, FILE_CURRENT)) == INVALID_SET_FILE_POINTER)
			{
				Close ();
				return false;
			}
			return true;
		}
		else
		{
			DWORD lastError = GetLastError();
			if ( (behavior & kRetryOnOpenFail) && (--retryCount > 0) )
			{
				Thread::Sleep(0.2);
				continue;
			}

			if ( behavior & kSilentReturnOnOpenFail )
				return false;

#if UNITY_EDITOR
			// LastError value may be destroyed inside std::string buffer allocation
			/*int result = DisplayDialogComplex ("Opening file failed", GetFormattedFileError("Opening file "+path, lastError), "Try Again", "Force Quit", "Cancel");
			if (result == 1)
			exit(1);
			else if (result == 2)
			return false;*/
#else
			return false;
#endif
		}
	}
	return false;
}

bool File::Close ()
{
	if( m_FileHandle != INVALID_HANDLE_VALUE )
	{
		if( !CloseHandle(m_FileHandle) )
		{
			DWORD lastError = GetLastError();

#if UNITY_EDITOR
			ErrorString(GetFormattedFileError("Closing file " + m_Path, lastError));
#else
			ErrorString("Closing file " + m_Path);
#endif
		}
		m_FileHandle = INVALID_HANDLE_VALUE;
	}

	m_Path.clear();
	return true;
}

int File::Read (void* buffer, int size)
{
	if( m_FileHandle != INVALID_HANDLE_VALUE )
	{
		DWORD bytesRead = 0;
		BOOL ok = ReadFile( m_FileHandle, buffer, size, &bytesRead, NULL );
		if( ok || bytesRead == size )
		{
			m_Position += bytesRead;
			return bytesRead;
		}
		else
		{
			//m_Position = -1;
			DWORD lastError = GetLastError ();
			if (!HandleFileError("Reading file failed", "Reading from file " + m_Path, lastError))
				return false;

			// We don't know how far the file was read.
			// So we just play safe and go through the API that seeks from a specific offset
			int oldPos = m_Position;
			m_Position = -1;
			return Read( oldPos, buffer, size );
		}
	}
	else
	{
		ErrorString("Reading failed because the file was not opened");
		return 0;
	}
}

int File::Read( int position, void* buffer, int size )
{
	if( m_FileHandle != INVALID_HANDLE_VALUE )
	{
		while( true )
		{
			// Seek if necessary
			if( position != m_Position )
			{
				DWORD newPosition = 0;
				newPosition = SetFilePointer(m_FileHandle, position, NULL, FILE_BEGIN);
				DWORD lastError = GetLastError ();
				bool failed = (newPosition == INVALID_SET_FILE_POINTER && lastError != NO_ERROR);
				if( newPosition == position && !failed )
				{
					m_Position = position;
				}
				else
				{
					m_Position = -1;
					if (!HandleFileError("Reading file failed", "Seeking in file " + m_Path, lastError))
						return false;
					continue;
				}
			}

			DWORD bytesRead = 0;
			BOOL ok = ReadFile( m_FileHandle, buffer, size, &bytesRead, NULL );
			if( ok )
			{
				m_Position += bytesRead;
				return bytesRead;
			}
			else
			{
				DWORD lastError = GetLastError ();
				m_Position = -1;
				if (!HandleFileError("Reading file failed", "Reading from file " + m_Path, lastError))
					return 0;
			}
		}
	}
	else
	{
		ErrorString("Reading failed because the file was not opened");
		return 0;
	}
}


bool File::Write (const void* buffer, int size)
{
	if( m_FileHandle != INVALID_HANDLE_VALUE )
	{
		DWORD bytesWritten = 0;
		BOOL ok = WriteFile( m_FileHandle, buffer, size, &bytesWritten, NULL );
		if( ok && size == bytesWritten )
		{
			m_Position += bytesWritten;
			return true;
		}
		else
		{
			DWORD lastError = GetLastError ();
			if( !HandleFileError("Writing file failed", "Writing to file " + m_Path, lastError) )
				return false;

			// We don't know how far the file was written.
			// So we just play safe and go through the API that seeks from a specific offset.
			int oldPos = m_Position;
			m_Position = -1;
			return Write( oldPos, buffer, size );
		}
	}
	else
	{
		ErrorString("Writing failed because the file was not opened");
		return false;
	}
}

bool File::Write (int position, const void* buffer, int size)
{
	if( m_FileHandle != INVALID_HANDLE_VALUE )
	{
		while (true)
		{
			// Seek if necessary
			if (position != m_Position)
			{
				DWORD newPosition = 0;
				newPosition = SetFilePointer(m_FileHandle, position, NULL, FILE_BEGIN);
				DWORD lastError = GetLastError ();
				bool failed = (newPosition == INVALID_SET_FILE_POINTER && lastError != NO_ERROR);
				if( newPosition == position && !failed )
				{
					m_Position = position;
				}
				else
				{
					m_Position = -1;
					if (!HandleFileError("Writing file failed", "Seeking in file " + m_Path, lastError))
						return false;
					continue;
				}
			}

			DWORD bytesWritten = 0;
			BOOL ok = WriteFile( m_FileHandle, buffer, size, &bytesWritten, NULL );
			DWORD lastError = GetLastError ();
			if( ok && size == bytesWritten )
			{
				m_Position += bytesWritten;
				return true;
			}
			else
			{
				m_Position = -1;
				if( !HandleFileError("Writing file failed", "Writing to file " + m_Path, lastError))
					return false;
			}
		}
	}
	else
	{
		ErrorString("Writing failed because the file was not opened");
		return false;
	}
}

bool File::SetFileLength (int size)
{
	if (m_FileHandle != INVALID_HANDLE_VALUE)
		return ::SetFileLength(m_FileHandle, m_Path, size);
	else
	{
		ErrorString("Resizing file failed because the file was not opened");
		return false;
	}
}


void File::SetCurrentDirectory (const string& path)
{
	gCurrentDirectory = path;
	ConvertSeparatorsToUnity( gCurrentDirectory );
}

const string& File::GetCurrentDirectory ()
{
	return gCurrentDirectory;
}

void File::CleanupClass()
{
	gCurrentDirectory = string();
}

bool SetFileLength (const std::string& path, int size)
{
	while (true)
	{
		for ( int attempts = 0; attempts < 2; attempts++ )
		{
			wchar_t widePath[kDefaultPathBufferSize];
			ConvertUnityPathName( path.c_str(), widePath, kDefaultPathBufferSize );
			HANDLE hfile = CreateFileW( widePath, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );
			if( hfile != INVALID_HANDLE_VALUE )
			{
				//LONG sizehigh = (size >> (sizeof(LONG) * 8));
				LONG sizehigh = 0;
				LONG sizelow = (size & 0xffffffff);
				LARGE_INTEGER largeSize;
				largeSize.QuadPart = size;
				if( SetFilePointerEx( hfile, largeSize, NULL, FILE_BEGIN ) )
				{
					if( SetEndOfFile( hfile ) )
					{
						CloseHandle( hfile );
						return true;
					}
				}
				CloseHandle( hfile );
			}
			//Sometimes CreateFileW will return INVALID_HANDLE_VALUE, it might because antivirus has opened the target file, so try again... to do the same operation
		}

#if UNITY_EDITOR
		DWORD lastError = ::GetLastError ();
		/*int result = DisplayDialogComplex ("Writing file error", GetFormattedFileError("Resizing file " + path, lastError), "Try Again", "Cancel", "Quit");
		if (result == 1)
		return false;
		else if (result == 2)
		exit(1);*/
#else
		return false;
#endif
	}

	return true;
}

bool SetFileLength (const HANDLE hFile, const std::string& path, int size)
{
	// sets file length without closing/reopening file
	while (true)
	{
		if( hFile != INVALID_HANDLE_VALUE )
		{
			//LONG sizehigh = (size >> (sizeof(LONG) * 8));
			LONG sizehigh = 0;
			LONG sizelow = (size & 0xffffffff);
			LARGE_INTEGER largeSize;
			largeSize.QuadPart = size;
			if( ::SetFilePointerEx( hFile, largeSize, NULL, FILE_BEGIN ) )
			{
				if( ::SetEndOfFile( hFile ) )
				{
					::SetFilePointer( hFile, 0, 0, FILE_BEGIN );
					return true;
				}
			}
		}

#if UNITY_EDITOR
		DWORD lastError = ::GetLastError ();
		/*int result = DisplayDialogComplex ("Writing file error", GetFormattedFileError("Resizing file " + path, lastError), "Try Again", "Cancel", "Quit");
		if (result == 1)
		return false;
		else if (result == 2)
		exit(1);*/
#else
		return false;
#endif
	}

	return true;
}

static bool IsFileCreatedAtAbsolutePath (const string & path)
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName(path.c_str(), widePath, _countof(widePath));
	return true;
}

bool IsFileCreated (const string& path)
{
	return IsFileCreatedAtAbsolutePath (PathToAbsolutePath (path));
}

bool IsFileOrDirectoryInUse ( const string& path )
{
	if ( IsDirectoryCreated( path ) )
	{
		set<string> paths;
		if ( GetFolderContentsAtPath( path, paths ) )
		{
			for (set<string>::iterator i = paths.begin(); i != paths.end(); i++)
			{
				if ( IsFileOrDirectoryInUse( *i ) )
					return true;
			}
		}
	}
	else if ( IsFileCreated( path ) )
	{
		HANDLE openTest = OpenFileWithPath ( path, File::kReadWritePermission );
		if ( openTest != INVALID_HANDLE_VALUE )
			CloseHandle(openTest);
		else
			return true;
	}

	return false;
}

static bool IsDirectoryCreatedAtAbsolutePath (const string & path)
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName(path.c_str(), widePath, _countof(widePath));
	return true;
}

bool IsDirectoryCreated (const string& path)
{
	return IsDirectoryCreatedAtAbsolutePath (PathToAbsolutePath (path));
}

static bool IsPathCreatedAtAbsolutePath (const string & path)
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName(path.c_str(), widePath, _countof(widePath));
	return true;
}

bool IsPathCreated (const string& path)
{
	return IsPathCreatedAtAbsolutePath (PathToAbsolutePath (path));
}

bool CreateDirectory (const string& pathName)
{
	if (IsFileCreated (pathName))
		return false;
	else if (IsDirectoryCreated (pathName))
		return true;

	string absolutePath = PathToAbsolutePath (pathName);

#if UNITY_EDITOR
	ConvertSeparatorsToUnity(absolutePath);

	std::string fileNamePart = GetLastPathNameComponent(absolutePath);
	if( CheckValidFileNameDetail(fileNamePart) == kFileNameInvalid) {
		//ErrorStringMsg ("%s is not a valid directory name. Please make sure there are no unallowed characters in the name.", fileNamePart.c_str());
		return false;
	}
#endif

	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( pathName.c_str(), widePath, kDefaultPathBufferSize );
	if( CreateDirectoryW( widePath, NULL ) )
		return true;
	else
	{
		DWORD err = GetLastError();
		//printf_console("CreateDirectory '%s' failed: %s\n", absolutePath.c_str(), winutils::ErrorCodeToMsg(err).c_str() );
		return false;
	}
}

bool ShouldIgnoreFile( const WIN32_FIND_DATAW& data )
{
	// ignore hidden files and folders
	if( data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN )
		return true;

	// ignore names starting with '.' (hidden on *nix)
	if( data.cFileName[0] == L'.' )
		return true;

	// ignore CVS files
	if( _wcsicmp(data.cFileName,L"cvs")==0 )
		return true;

	int length = wcslen(data.cFileName);
	if(length < 1)
		return true;

	// ignore temporary or backup files
	if( data.cFileName[length-1] == L'~' || length >= 4 && _wcsicmp(data.cFileName+length-4,L".tmp")==0 )
		return true;

	return false;
}


static bool GetFolderContentsAtPathImpl( const std::wstring& pathNameWide, std::set<std::string>& paths, bool deep )
{
	if( pathNameWide.empty() )
		return false;

	// base path
	std::wstring basePath = pathNameWide;
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
			// ignore '.' and '..'
			if( wcscmp(findData.cFileName,L".")==0 || wcscmp(findData.cFileName,L"..")==0 )
				continue;

			if( ShouldIgnoreFile(findData) )
				continue;

			std::wstring filePath = basePath + findData.cFileName;
			if( deep && (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
			{
				// deep results requested - recurse
				if( !GetFolderContentsAtPathImpl( filePath, paths, deep ) )
					hadFailures = true;
			}

			// add to results
			std::string bufUtf8;
			ConvertWindowsPathName( filePath, bufUtf8 );
			paths.insert( bufUtf8 );
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

	return !hadFailures;
}

bool GetFolderContentsAtPath( const string& pathName, std::set<std::string>& paths )
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( pathName.c_str(), widePath, kDefaultPathBufferSize );
	return GetFolderContentsAtPathImpl( widePath, paths, false );
}

bool GetDeepFolderContentsAtPath( const string& pathName, std::set<string>& paths )
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( pathName.c_str(), widePath, kDefaultPathBufferSize );
	return GetFolderContentsAtPathImpl( widePath, paths, true );
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

bool DeleteFileOrDirectory (const string& path)
{
	if( IsDirectoryCreated(path) )
		return RemoveDirectoryRecursive( path );
	else
	{
		wchar_t widePath[kDefaultPathBufferSize];
		ConvertUnityPathName(path.c_str(), widePath, kDefaultPathBufferSize);

		if (DeleteFileW(widePath))
		{
			return true;
		}

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
}

#if UNITY_EDITOR

bool MoveReplaceFile (const string& fromPath, const string& toPath)
{
	if( IsDirectoryCreated(fromPath) )
	{
		//ErrorString( Format("Path %s is a directory", fromPath.c_str()) );
		return false;
	}
	while (true)
	{
		wchar_t wideFrom[kDefaultPathBufferSize], wideTo[kDefaultPathBufferSize];
		ConvertUnityPathName( PathToAbsolutePath(fromPath).c_str(), wideFrom, kDefaultPathBufferSize );
		ConvertUnityPathName( PathToAbsolutePath(toPath).c_str(), wideTo, kDefaultPathBufferSize );
		if( MoveFileExW( wideFrom, wideTo, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED ) )
			return true;

#if UNITY_EDITOR

		if (ERROR_ACCESS_DENIED == GetLastError())
		{
			if (RemoveReadOnlyW(wideTo))
			{
				if (MoveFileExW(wideFrom, wideTo, (MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)))
				{
					return true;
				}
			}
		}

		DWORD lastError = ::GetLastError ();
		/*int result = DisplayDialogComplex ("Moving file failed", GetFormattedFileError("Moving "+fromPath+" to "+toPath, lastError), "Try Again", "Force Quit", "Cancel");
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

#endif

static inline DWORD FileFlagsToWindowsAttributes( UInt32 flags )
{
	DWORD v = 0;
	if( flags & kFileFlagTemporary ) v |= FILE_ATTRIBUTE_TEMPORARY;
	if( flags & kFileFlagDontIndex ) v |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
	if( flags & kFileFlagHidden ) v |= FILE_ATTRIBUTE_HIDDEN;
	return v;
}

bool SetFileFlags (const std::string& path, UInt32 attributeMask, UInt32 attributeValue )
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( PathToAbsolutePath(path).c_str(), widePath, kDefaultPathBufferSize );

	const DWORD attr = GetFileAttributesW(widePath);
	if( attr == INVALID_FILE_ATTRIBUTES )
		return false;

	DWORD winMask = FileFlagsToWindowsAttributes(attributeMask);
	DWORD winValue = FileFlagsToWindowsAttributes(attributeValue);

	// only files and not directories can be temporary
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
	{
		winMask &= ~FILE_ATTRIBUTE_TEMPORARY;
		winValue &= ~FILE_ATTRIBUTE_TEMPORARY;
	}

	BOOL ok = SetFileAttributesW( widePath, (attr & (~winMask)) | winValue );
	Assert(ok);
	return ok ? true : false;
}

bool isHiddenFile (const std::string& path)
{
	wchar_t widePath[kDefaultPathBufferSize];
	ConvertUnityPathName( PathToAbsolutePath(path).c_str(), widePath, kDefaultPathBufferSize );

	const DWORD attr = GetFileAttributesW(widePath);
	if( attr == INVALID_FILE_ATTRIBUTES )
		return false;

	// Check if the file is hidden.
	if (attr & FILE_ATTRIBUTE_HIDDEN)
		return true;

	return false;
}
