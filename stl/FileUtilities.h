#ifndef FILEUTILITIES_H
#define FILEUTILITIES_H

#include <list>
#include <string>
#include <set>
#include "PathNameUtility.h"
#include "DateTime.h"
#include "File.h"

using std::string;

#if UNITY_EDITOR

/// Returns the real pathname of path
/// (eg. if pathname is all lowercase, the returned string will match the actual pathname)
/// If there is no file at path returns path.
std::string GetActualPathSlow (const std::string & input);
std::string GetActualAbsolutePathSlow (const std::string & path);

// Copies a file or folder. Resolves all symlinks in from before copying.
bool CopyFileOrDirectory (const string& from, const string& to);
bool MoveToTrash (const string& path);
bool CopyReplaceFile (string from, string to);
bool MoveFileOrDirectory (const string& fromPath, const string& toPath);

string GetProjectRelativePath (const string& path);

bool CreateDirectory (const string& pathName);

/// Returns a set of files and folders that are deep children of the folder at pathname.
/// Ignores invisible files
bool GetDeepFolderContentsAtPath (const string& pathName, std::set<string>& paths);

/// Returns the content modification date for the file at path or zero timestamp if file couldn't be found
DateTime GetContentModificationDate (const string& path);

/// Sets the content modification date (write time) for the file at path
bool SetContentModificationDateToCurrentTime (const string& path);

bool CreateDirectorySafe (const string& pathName);

std::string GenerateUniquePath (std::string inPath);
std::string GenerateUniquePathSafe (const std::string& pathName);
std::string GetUniqueTempPathInProject ();
std::string GetUniqueTempPath (const std::string& basePath);

bool IsPathCreated (const std::string& path);

#if UNITY_OSX || UNITY_LINUX
inline void UnixTimeToUnityTime (time_t time, DateTime &dateTime )
{
	// Seconds from 1904 to 2001: 3061152000 (kCFAbsoluteTimeIntervalSince1904)
	// Seconds from 1970 to 2001: 978307200 (kCFAbsoluteTimeIntervalSince1970)
	// Thus seconds from 1904 to 1970: 2082844800
	UInt64 seconds = time - 2082844800;
	dateTime.fraction = 0;
	dateTime.lowSeconds = seconds & 0xFFFFFFFF;
	dateTime.highSeconds = seconds >> 32;
}
#endif

#if UNITY_OSX
bool PathToFSSpec (const std::string& path, FSSpec* spec);
#endif

#if UNITY_WIN
void FileTimeToUnityTime( const FILETIME& ft, DateTime& dateTime );
bool CopyFileOrDirectoryCheckPermissions (const string& from, const string& to, bool &accessdenied);
#endif

enum AtomicWriteMode {
	kProjectTempFolder = 0,
#if UNITY_OSX
	kSystemTempFolder = 1,
#endif
	kNotAtomic = 2,
};
bool WriteStringToFile (const string& inData, const string& pathName, AtomicWriteMode mode, UInt32 fileFlags);

bool IsSymlink (const std::string& pathName);

// Resolve symlinks & canonicalize the path
std::string ResolveSymlinks (const std::string& pathName);

std::string GetUserAppDataFolder ();
std::string GetUserAppCacheFolder ();

#endif // UNITY_EDITOR

string GetApplicationPath ();
string GetApplicationContentsPath ();
string GetApplicationFolder ();
string GetApplicationLocalFolder();
string GetApplicationManagedPath();
string GetApplicationTemporaryFolder();

#if UNITY_STANDALONE || UNITY_EDITOR
string GetApplicationNativeLibsPath();
#endif

bool CreateFile (const string& path, int creator = '?', int fileType = '?');

#if UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
void SetApplicationPath( std::string path );
bool IsSymlink(const std::string &pathName);
std::string ResolveSymlink(const std::string& pathName);
#if !UNITY_BB10 && !UNITY_TIZEN
std::string GetUserConfigFolder ();
#endif
#endif

#endif
