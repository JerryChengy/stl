#pragma once

#include "Allocator.h"
#if UNITY_OSX || UNITY_IPHONE || UNITY_ANDROID || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
#include "fcntl.h"
#endif
#include "NonCopyable.h"
#include "PathNameUtility.h"
#include <set>
#include "EnumFlags.h"

#if UNITY_WII
#include <rvlaux/file-io.h>
#endif

#if UNITY_WINRT
// Enable only one 
#define ENABLE_CRT_IO 0
#define ENABLE_WIN_IO 1
#endif

class File : public NonCopyable
{
	int          m_Position;
	std::string  m_Path;
#if UNITY_WII
	bool		 m_Open;
	WiiFileInfo  m_File;
#elif UNITY_WINRT
	//Microsoft::WRL::ComPtr<IInspectable>	m_Stream;
#	if ENABLE_CRT_IO
	FILE*		m_File;
#	elif ENABLE_WIN_IO	
	HANDLE		m_FileHandle;
#	else
	std::vector<unsigned char> m_Data;
#	endif
#elif UNITY_WIN || UNITY_XENON
	HANDLE		m_FileHandle;
#elif UNITY_PS3
	int			m_FileDescriptor;
#else
	FILE*		m_File; 
#endif

public:

	File ();
	~File ();

	enum Permission { kReadPermission = 0, kWritePermission = 1, kReadWritePermission = 2, kAppendPermission = 3 };
	enum AutoBehavior { kNormalBehavior = 0, kSilentReturnOnOpenFail = 1<<0, kRetryOnOpenFail = 1<<1}; 

	bool Open (const std::string& path, Permission perm, AutoBehavior behavior = kNormalBehavior);
	bool Close ();

	int Read (void* buffer, int size);
	int Read (int position, void* buffer, int size);

	bool Write (const void* buffer, int size);
	bool Write (int pos, const void* buffer, int size);
	bool SetFileLength (int size);
	int GetFileLength ();
	int GetPosition () const { return m_Position; }

	static void SetCurrentDirectory (const std::string& path);
	static const std::string& GetCurrentDirectory ();
	static void CleanupClass();

#if UNITY_OSX || UNITY_IPHONE || UNITY_ANDROID || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
	enum LockMode {
		kNone = LOCK_UN,
		kShared = LOCK_SH,
		kExclusive = LOCK_EX
	};
	bool Lock(File::LockMode mode, bool block);
#endif
};

ENUM_FLAGS(File::AutoBehavior);

/// Returns the file length of the file at pathName. If the file doesn't exist returns -1.
int GetFileLength (const std::string& pathName);
size_t GetFileSize (FILE* file);


std::string PathToAbsolutePath (const std::string& path);
// Path is absolute in platform dependent way!
bool IsAbsoluteFilePath( const std::string& path );

/// Reads the contents of the file at pathName into data.
/// byteStart is the first byte read in the file, byteLength is the number of bytes that should be read
bool ReadFromFile (const std::string& pathName, void *data, int fileStart, int byteLength);

bool WriteBytesToFile (const void *data, int byteLength, const std::string& pathName);

#if UNITY_WII
typedef std::basic_string<char, std::char_traits<char>, STL_ALLOCATOR_ALIGNED(kMemIOTemp, char, 32)> InputString;
#else
typedef TEMP_STRING InputString;
#endif
bool ReadStringFromFile (InputString* outData, const std::string& pathName);

bool IsFileCreated (const std::string& path);
bool IsFileOrDirectoryInUse( const std::string& path );
bool CreateDirectory (const std::string& pathName);

/// Returns a set of files and folders that are children of the folder at pathname.
/// Ignores invisible files
bool GetFolderContentsAtPath (const std::string& pathName, std::set<std::string>& paths);

/// Deletes a single file. Returns true if the file could be deleted.
/// Returns false if the file couldnt be deleted or the file was a folder.
bool DeleteFile (const std::string& file);
bool DeleteFileOrDirectory (const std::string& path);

bool SetFileLength (const std::string& path, int size);
#if UNITY_WIN
bool SetFileLength (const HANDLE hFile, const std::string& path, int size);
bool GetFolderContentsAtPathImpl( const std::wstring& pathNameWide, std::set<std::string>& paths, bool deep );
#endif
bool IsDirectoryCreated (const std::string& path);

#if UNITY_EDITOR
bool MoveReplaceFile (const std::string& fromPath, const std::string& toPath);
#if UNITY_OSX || UNITY_LINUX
std::string GetFormattedFileError (int error, const std::string& operation);
#endif
#endif

#if !UNITY_EXTERNAL_TOOL
inline bool CreateDirectoryRecursive (const std::string& pathName)
{
	if (IsDirectoryCreated(DeleteLastPathNameComponent(pathName)))
	{
		if (IsDirectoryCreated (pathName))
			return true;
		else if (IsFileCreated (pathName))
			return false;
		else
			return CreateDirectory (pathName);
	}

	std::string::size_type pos = pathName.rfind ("/");
	if (pos == std::string::npos)
		return false;

	std::string superPath;
	superPath.assign (pathName.begin (), pathName.begin () + pos);

	if (!CreateDirectoryRecursive (superPath))
		return false;

	return CreateDirectoryRecursive (pathName);
}
#endif

// On Vista, its search indexer can wreak havoc on files that are written then renamed. Full story here:
// http://stackoverflow.com/questions/153257/random-movefileex-failures-on-vista-access-denied-looks-like-caused-by-search-i
// but in short, whenever a file is temporary or should be excluded from search indexing, we have to set those flags.
enum FileFlags {
	kFileFlagTemporary = (1<<0), // File is meant to be temporary.
	kFileFlagDontIndex = (1<<1), // Exclude file from search indexing (Spotlight, Vista Search, ...)
	kFileFlagHidden	   = (1<<2), // Set file as hidden file.

	kAllFileFlags = kFileFlagTemporary | kFileFlagDontIndex
};

#if UNITY_WIN
bool SetFileFlags (const std::string& path, UInt32 attributeMask, UInt32 attributeValue);

bool isHiddenFile (const std::string& path);

#elif UNITY_EDITOR
bool SetFileFlags (const std::string& path, UInt32 attributeMask, UInt32 attributeValue);

bool isHiddenFile (const std::string& path);

#else
inline bool SetFileFlags (const std::string& path, UInt32 attributeMask, UInt32 attributeValue)
{
	return true;
}

inline bool isHiddenFile (const std::string& path)
{
	return false;
}
#endif

#if UNITY_OSX
void SetupFileLimits ();
#endif

