#include "UnityPrefix.h"
#include "LogAssert.h"
#include <stdarg.h>
#include "PathNameUtility.h"
#include <sys/stat.h>
#include <list>
#include "UnityConfigure.h"
#include "AtomicOps.h"
#if !UNITY_EXTERNAL_TOOL && !UNITY_PLUGIN
#include "Thread.h"
#include "Mutex.h"
#include "ThreadSpecificValue.h"
#endif
#include "Profiler.h"

#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_TIZEN
#include <syslog.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include "Runtime/Utilities/File.h"
#endif
#if UNITY_NACL
#include "PlatformDependent/PepperPlugin/UnityInstance.h"
#endif

#if UNITY_BB10
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#endif
#if UNITY_WIN
#if !UNITY_WP8
#include <ShlObj.h>
#endif
#include "PathUnicodeConversion.h"
#include "FileUtilities.h"
#include "WinUtils.h"
#include "io.h"
#endif

#if UNITY_TIZEN
#include <osp/FBaseLog.h>
#endif
#if UNITY_EDITOR
#include "File.h"
//#include "Editor/Src/Utility/Analytics.h"
#endif

#if WEBPLUG
#include "ErrorExit.h"
#endif

//#include "ScriptingUtility.h"

#if UNITY_WII
#include "Platformdependent/wii/WiiDbgUtils.h"
#endif
#if UNITY_ANDROID
#include <android/log.h>

int CsToAndroid[LogType_NumLevels] =
{
	ANDROID_LOG_ERROR,		// LogType_Error = 0,
	ANDROID_LOG_ERROR,		// LogType_Assert = 1,
	ANDROID_LOG_WARN,		// LogType_Warning = 2,
	ANDROID_LOG_INFO, 		// LogType_Log = 3,
	ANDROID_LOG_ERROR, 		// LogType_Exception = 4,
	ANDROID_LOG_DEBUG		// LogType_Debug = 5,
};
#endif

using namespace std;

#if UNITY_WP8
	#include "PlatformDependent\MetroPlayer\MetroUtils.h"
#endif

#if WEBPLUG && !UNITY_WIN
#define CAP_LOG_OUTPUT_SIZE 1
#else
#define CAP_LOG_OUTPUT_SIZE 0
#endif

#if UNITY_WIN && DEBUGMODE && !UNITY_WP8
	extern "C" WINBASEAPI BOOL WINAPI IsDebuggerPresent( VOID );
	#define LOG_TO_WINDOWS_DEBUGGER \
		char buf[4096]; buf[4095]=0; \
		vsnprintf( buf, sizeof(buf)-1, log, list ); \
		OutputDebugStringA( buf )
#elif UNITY_WP8 && UNITY_DEVELOPER_BUILD
	#define LOG_TO_WINDOWS_DEBUGGER	\
			char buf[4096]; buf[4095]=0; \
			vsnprintf (buf, sizeof(buf) - 1, log, list); \
			auto str = ConvertUtf8ToString (buf); \
			s_WinRTBridge->WP8Utility->OutputLogMessage (str)
#else
	#define LOG_TO_WINDOWS_DEBUGGER
#endif

#if UNITY_FLASH
extern "C" void Ext_Flash_LogCallstack();
#endif

static LogEntryHandler gCurrentLogEntryHandler = NULL;
static std::list<LogEntryHandler> *gCleanLogEntryHandlers = NULL;

void ReleaseLogHandlers()
{
	if (gCleanLogEntryHandlers != NULL)
	{
		delete gCleanLogEntryHandlers;
		gCleanLogEntryHandlers = NULL;
	}
}

void SetLogEntryHandler(LogEntryHandler newHandler)
{
	gCurrentLogEntryHandler = newHandler;
}

void AddCleanLogEntryHandler(LogEntryHandler newHandler)
{
	if (gCleanLogEntryHandlers == NULL)
		gCleanLogEntryHandlers = new std::list<LogEntryHandler>();

	gCleanLogEntryHandlers->push_back(newHandler);
}

FILE* gConsoleFile = NULL;
FILE* gReproductionLogFile = NULL;

bool DefaultCleanLogHandlerv (LogType logType, const char* log, va_list list);

void InitializeCleanedLogFile (FILE* file)
{
	Assert(gReproductionLogFile == NULL);
	Assert(gCleanLogEntryHandlers == NULL || count(gCleanLogEntryHandlers->begin(), gCleanLogEntryHandlers->end(), &DefaultCleanLogHandlerv) == 0);

	gReproductionLogFile = file;

	AddCleanLogEntryHandler(&DefaultCleanLogHandlerv);
}

static StaticString gConsolePath;
#if CAP_LOG_OUTPUT_SIZE
static int gConsoleSizeCheck = 0;
#endif
#if !UNITY_EXTERNAL_TOOL
static UNITY_TLS_VALUE(int) gRecursionLock;
#endif

enum { kMaxLogSize = 2000000 };



#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
fpos_t gStdOutPosition;
int gStdoutFd;
void ResetStdout()
{
	fflush(stdout);
	dup2(gStdoutFd, fileno(stdout));
	close(gStdoutFd);
	clearerr(stdout);
	fsetpos(stdout, &gStdOutPosition);
}
#endif

#if UNITY_XENON || UNITY_PS3
void LogOutputToSpecificFile (const char* path)
{
	if (path == NULL)
		return;

	gConsolePath = path;
	gConsoleFile = fopen(path, "w");
	if(gConsoleFile)
		fclose(gConsoleFile);

}
#elif !UNITY_PS3 && !UNITY_ANDROID && !UNITY_PEPPER && !UNITY_FLASH

void LogOutputToSpecificFile (const char* path)
{
#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN

	// Save the stdout position for later
	fgetpos(stdout, &gStdOutPosition);
	gStdoutFd = dup(fileno(stdout));

	if (path == NULL || strlen(path) == 0)
	{
		gConsoleFile = stdout;
	}
	else
	{
		gConsoleFile = fopen(path, "w");
		int fd = open(path, O_WRONLY, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
	}

#elif UNITY_WII

	// TODO: see if logging to the host machine (windows) works
	if (path == NULL)
		return;

	gConsolePath = path;
	gConsoleFile = fopen(path, "w");

#else

	// On windows just default to editor.log instead
	if (path == NULL)
		return;

	gConsolePath = path;
	gConsoleFile = fopen(path, "w");
#endif
}
#endif

#if SUPPORT_ENVIRONMENT_VARIABLES
std::string GetStringFromEnv(const char *envName)
{
	const char* env = getenv( envName );
	std::string result;
	if( env != NULL && env[0] != 0 )
		result.append(env);
	return result;
}
std::string GetCustomLogFile()
{
	return GetStringFromEnv("UNITY_LOG_FILE");
}
std::string GetCleanedLogFile()
{
	return GetStringFromEnv("UNITY_CLEANED_LOG_FILE");
}
#endif

#if UNITY_WIN

string SetLogFilePath(string const& path)
{
	Assert(gConsolePath.empty());

	gConsolePath = path;

	#if !UNITY_EDITOR && !UNITY_EXTERNAL_TOOL && !UNITY_WINRT

	if (!gConsolePath.empty())
	{
		string const customLogFile = GetCustomLogFile();

		if (!customLogFile.empty())
		{
			gConsolePath = customLogFile.c_str();
		}
	}

	#endif

	return gConsolePath.c_str();
}

namespace
{
	int gStdOutFd = -1;
	int gStdErrFd = -1;
	FILE* gStdOutFile = NULL;
	FILE* gStdErrFile = NULL;

	void CloseConsoleWin()
	{
		gConsoleFile = NULL;

		if (NULL != gStdOutFile)
		{
			int const result = fclose(gStdOutFile);
			//Assert(0 == result);

			gStdOutFile = NULL;
		}

		if (NULL != gStdErrFile)
		{
			int const result = fclose(gStdErrFile);
			//Assert(0 == result);

			gStdErrFile = NULL;
		}

		if (-1 != gStdOutFd)
		{
			int const result = _dup2(gStdOutFd, 1);
			//Assert(0 == result);

			gStdOutFd = -1;
		}

		if (-1 != gStdErrFd)
		{
			int const result = _dup2(gStdErrFd, 2);
			//Assert(0 == result);

			gStdErrFd = -1;
		}
		gConsolePath.clear();
	}

	void OpenConsoleWin()
	{
		// don't assert in this function because it might cause stack overflow
		std::wstring widePath;

		//Assert(NULL == gConsoleFile);

		// check for no log file

		if (gConsolePath.empty())
		{
			gConsoleFile = stdout;
			return;
		}

		// duplicate stdout and stderr file descriptors so they can be restored later

		gStdOutFd = _dup(1);
		//Assert(-1 != gStdOutFd);

		if (-1 == gStdOutFd)
		{
			goto error;
		}

		gStdErrFd = _dup(2);
		//Assert(-1 != gStdErrFd);

		if (-1 == gStdErrFd)
		{
			goto error;
		}

		// reassign stdout and stderr file pointers

		ConvertUnityPathName(gConsolePath, widePath);

		gStdOutFile = _wfreopen(widePath.c_str(), L"a", stdout);
		//Assert(NULL != gStdOutFile);

		if (NULL == gStdOutFile)
		{
			goto error;
		}

		gStdErrFile = _wfreopen(widePath.c_str(), L"a", stderr);
		//Assert(NULL != gStdErrFile);

		if (NULL == gStdErrFile)
		{
			goto error;
		}

		// redirect stderr to stdout

		int const error = _dup2(1, 2);
		//Assert(0 == error);

		if (0 != error)
		{
			goto error;
		}

		// disable stdout and stderr buffering

		setbuf(stdout, NULL);
		setbuf(stderr, NULL);

		// done

		gConsoleFile = stdout;
		return;

		// failed

	error:

		CloseConsoleWin();

		gConsoleFile = stdout;
		return;
	}
}

#endif

#if (UNITY_OSX || UNITY_LINUX) && !UNITY_EXTERNAL_TOOL
// Get the logfile path, relative to the user's home directory, using a system-specific subpath
static std::string GetHomedirLogfile (const std::string &relativePath)
{
	string folder = getenv ("HOME");
	if (folder.empty ())
		return "";

	// Create log file and parent folders
	folder = AppendPathName( folder, relativePath);
	CreateDirectoryRecursive (folder);

	#if UNITY_EDITOR
	std::string result = AppendPathName( folder, "Editor.log" );
	// move any existing log file into Editor-prev.log
	if( IsFileCreated(result) )
		MoveReplaceFile( result, AppendPathName(folder,"Editor-prev.log" ) );
	#else
	std::string result = GetCustomLogFile();
	if (result.empty())
		result = AppendPathName( folder, "Player.log" );
	#endif

	return result;
}

// Open the console path and redirect stdout/stderr if appropriate
static void OpenConsoleFile ()
{
	#if UNITY_EDITOR
	gConsoleFile = fopen(gConsolePath.c_str(), "w");
	#else
	gConsoleFile = fopen(gConsolePath.c_str(), "w+");
	#endif

	#if !WEBPLUG
	// Save the stdout position for later
	fgetpos(stdout, &gStdOutPosition);
	gStdoutFd = dup(fileno(stdout));

	if (gConsoleFile)
	{
		int fd = fileno (gConsoleFile);

		if (dup2 (fd, fileno (stdout)) < 0)
			fprintf (stderr, "Failed to redirect stdout to the console file %s.\n", gConsolePath.c_str ());
		if (dup2 (fd, fileno (stderr)) < 0)
			fprintf (stderr, "Failed to redirect stderr to the console file %s.\n", gConsolePath.c_str ());
	}
	#endif
}
#endif

FILE* OpenConsole ()
{
	if (gConsoleFile != NULL)
		return gConsoleFile;

	#define PLATFORM_ALWAYS_USES_STDOUT_FOR_LOG (UNITY_XENON || UNITY_IPHONE || UNITY_ANDROID || UNITY_PEPPER || UNITY_FLASH || UNITY_WII || UNITY_PS3 || UNITY_EXTERNAL_TOOL || UNITY_WEBGL || UNITY_BB10 || UNITY_TIZEN || ENABLE_GFXDEVICE_REMOTE_PROCESS_WORKER )

	#if GAMERELEASE && !UNITY_EXTERNAL_TOOL && !WEBPLUG && !UNITY_PLUGIN && !PLATFORM_ALWAYS_USES_STDOUT_FOR_LOG
	if (GetPlayerSettingsPtr() == NULL)
		return stdout;

	if (!GetPlayerSettings().GetUsePlayerLog())
	{
		gConsoleFile = stdout;
		return gConsoleFile;
	}
	#endif

	#if PLATFORM_ALWAYS_USES_STDOUT_FOR_LOG

	#if UNITY_NACL
	// in nacl, we can't write to a custom log location.
	// so if we need a clean log, use stdout for that, and use 0 for the
	// normal, non-clean log.
	if (GetUnityInstance().GetCleanLog())
		return 0;
	#endif
	
	gConsoleFile = stdout;

	#elif UNITY_OSX
	gConsolePath = GetHomedirLogfile ("Library/Logs/Unity");
	if (!gConsolePath.empty())
		OpenConsoleFile ();

	#elif UNITY_LINUX
	gConsolePath = GetHomedirLogfile (".config/unity3d");
	if (!gConsolePath.empty())
		OpenConsoleFile ();

	#elif UNITY_WIN

	OpenConsoleWin();

	#elif UNITY_PS3
		gConsoleFile = stdout;
		// When running from a read only file system, stdout is a valid file pointer, but it crashes later
		// if trying to write into it. So we check if valid _fileno exists for it.
		if( gConsoleFile && fileno(gConsoleFile) < 0 )
			gConsoleFile = NULL;

	#else

	#error "Unknown platform"

	#endif

	return gConsoleFile;
}

#if UNITY_WIN
void CloseConsoleFile()
{
	CloseConsoleWin();
}
#endif

#if UNITY_EDITOR
string GetEditorConsoleLogPath ()
{
	#if UNITY_OSX

	string home = getenv ("HOME");
	return AppendPathName (home, "Library/Logs/Unity/Editor.log");

	#elif UNITY_WIN

	return gConsolePath.c_str();

	#elif UNITY_LINUX

	string home = getenv ("HOME");
	return AppendPathName (home, ".config/Unity/Editor/Editor.log");

	#else
	#error "Unknown platform"
	#endif
}

string GetMonoDevelopLogPath ()
{
	#if UNITY_OSX

	string result = getenv ("HOME");
	return AppendPathName( result, "Library/Logs/MonoDevelop/MonoDevelop.log");

	#elif UNITY_WIN

	wchar_t widePath[MAX_PATH];
	if( SUCCEEDED(SHGetFolderPathW( NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, widePath )) )
	{
		std::string folder;
		ConvertWindowsPathName( widePath, folder );
		folder = AppendPathName( folder, "MonoDevelop-Unity" );
		return AppendPathName( folder, "log.txt" );
	}

	#elif UNITY_LINUX

	string result = getenv ("HOME");
	return AppendPathName( result, ".config/MonoDevelop-Unity/log.txt");

	#else
	#error "Unknown platform"
	#endif

	return "";
}

#if UNITY_OSX
string GetPlayerConsoleLogPath ()
{
	string home = getenv ("HOME");
	return AppendPathName (home, "Library/Logs/Unity/Player.log");
}
#endif

#if UNITY_LINUX
string GetPlayerConsoleLogPath ()
{
	string home = getenv ("HOME");
	return AppendPathName (home, ".config/unity3d/Editor/Player.log");
}
#endif
#endif

string GetConsoleLogPath ()
{
	return gConsolePath.c_str();
}

#if (UNITY_XENON || UNITY_PS3) && !MASTER_BUILD
static Mutex s_mutex;
#endif

void printf_consolev (LogType logType, const char* log, va_list alist)
{
	va_list list;
	va_copy (list, alist);

	if (gCurrentLogEntryHandler && !gCurrentLogEntryHandler (logType, log, list))
		return;

#if UNITY_FLASH
        char buffer[1024 * 10];
        vsnprintf (buffer, 1024 * 10, log, list);
        Ext_Trace(buffer);

        va_end (list);
        return;
#endif

#if UNITY_ANDROID
	if (gReproductionLogFile == NULL)		// gReproductionLogFile / 'cleanedLogFile' should remove all other logging
	{
		if (ENABLE_PROFILER /* == development player */ || logType < LogType_Debug)
			__android_log_vprint(CsToAndroid[logType], "Unity", log, list);
	}

	va_end (list);
	return;
#endif

#if UNITY_TIZEN
	if (gReproductionLogFile == NULL)
	{
        static char buffer[1024 * 10];
        memset(buffer, 0, 1024*10);
        vsnprintf (buffer, 1024 * 10, log, list);
		AppLogTagInternal("Unity", "", 0, buffer);
	}
#endif

#if UNITY_XENON
#if !MASTER_BUILD
	Mutex::AutoLock lock(s_mutex);

	char buffer[1024 * 8] = { 0 };
	vsnprintf(buffer, 1024 * 8, log, list);

	gConsoleFile = fopen(gConsolePath.c_str(), "a");
	if(gConsoleFile)
	{
		fprintf(gConsoleFile, buffer);
		fflush(gConsoleFile);
		fclose(gConsoleFile);
	}

	OutputDebugString(buffer);

#endif
	va_end (list);
	return;
#endif

#if UNITY_PS3
#if !MASTER_BUILD
	Mutex::AutoLock lock(s_mutex);

	gConsoleFile = fopen(gConsolePath.c_str(), "a");
	if(gConsoleFile)
	{
		vfprintf (gConsoleFile, log, list);
		fflush( gConsoleFile );
		fclose( gConsoleFile );
		vfprintf (stdout, log, list);
	}
	else
	{
		vfprintf(stdout, log, list);
		fflush(stdout);
	}

#endif
	va_end (list);
	return;
#endif

#if UNITY_WII
	#if !MASTER_BUILD
	vfprintf (stdout, log, list);
	fflush (stdout);
	va_end (list);
	#endif
	return;
#endif

	if (gConsoleFile == NULL)
	{
		if (OpenConsole() == NULL) {
			va_end (list);
			return;
		}
	}

	if (gConsoleFile)
	{
		vfprintf (gConsoleFile, log, list);
		fflush( gConsoleFile );

		// Clamp the size of the file to 100kb with a rolling buffer.
		// copy last 50kb to beginning of file.
		#if CAP_LOG_OUTPUT_SIZE
		gConsoleSizeCheck++;
		if (gConsoleSizeCheck > 20)
		{
			gConsoleSizeCheck = 0;
			struct stat statbuffer;
			if( ::stat(gConsolePath.c_str(), &statbuffer) == 0 && statbuffer.st_size > kMaxLogSize)
			{
				FILE* file = fopen(gConsolePath.c_str(), "r");
				if (file)
				{
					fseek(file, statbuffer.st_size - kMaxLogSize / 2, SEEK_SET);
					UInt8* buffer = new UInt8[kMaxLogSize / 2];
					if (fread(buffer, 1, kMaxLogSize / 2, file) == kMaxLogSize / 2)
					{
						fclose(file);
						fclose(gConsoleFile);
						gConsoleFile = fopen(gConsolePath.c_str(), "w");
						fwrite(buffer, 1, kMaxLogSize / 2, gConsoleFile);
					}
					else
					{
						fclose(file);
					}
					delete[] buffer;
				}
			}
		}
		#endif
	}
	else
	{
#ifndef DISABLE_TTY
		vfprintf (stdout, log, list);
		fflush( stdout );
#endif
	}

	LOG_TO_WINDOWS_DEBUGGER;
	va_end (list);
}

extern "C" void printf_console_log(const char* log, va_list list)
{
	printf_consolev(LogType_Log, log, list);
}

extern "C" void printf_console (const char* log, ...)
{
	va_list vl;
	va_start(vl, log);
	printf_consolev(LogType_Debug, log, vl);
	va_end(vl);
}

static void InternalLogConsole (const char* log, ...)
{
	va_list vl;
	va_start(vl, log);
	printf_consolev(LogType_Log, log, vl);
	va_end(vl);
}

static void InternalWarningConsole (const char* log, ...)
{
	va_list vl;
	va_start(vl, log);
	printf_consolev(LogType_Warning, log, vl);
	va_end(vl);
}

static void InternalAssertConsole (const char* log, ...)
{
	va_list vl;
	va_start(vl, log);
	printf_consolev(LogType_Assert, log, vl);
	va_end(vl);
}

static void InternalErrorConsole (const char* log, ...)
{
	va_list vl;
	va_start(vl, log);
	printf_consolev(LogType_Error, log, vl);
	va_end(vl);
}

static void InternalIgnoreConsole (const char* log, ...)
{
}

static LogToConsoleImpl* gLogToConsoleFunc = NULL;
static LogCallback* gLogCallbackFunc = NULL;
static bool gLogCallbackFuncThreadSafe = false;
static PreprocessCondition* gPreprocessor = NULL;
static RemoveLogFunction* gRemoveLog = NULL;
static RemoveLogFunction* gShowLogWithMode = NULL;
extern "C" void __msl_assertion_failed(char const *condition, char const *filename, char const *funcname, int lineno);

/*
extern "C"
{
void __eprintf(const char* log, ...)
{
printf_console (log, va_list(&log + 1));
}

void malloc_printf (const char* log, ...)
{
	printf_console (log, va_list(&log + 1));
}
}
*/
void RegisterLogToConsole (LogToConsoleImpl* func)
{
	gLogToConsoleFunc = func;
}

void RegisterLogCallback (LogCallback* callback, bool threadsafe)
{
	gLogCallbackFunc = callback;
	gLogCallbackFuncThreadSafe = threadsafe;
}

void RegisterLogPreprocessor (PreprocessCondition* func)
{
	gPreprocessor = func;
}

void RegisterRemoveImportErrorFromConsole (RemoveLogFunction* func)
{
	gRemoveLog = func;
}

void RegisterShowErrorWithMode (RemoveLogFunction* func)
{
	gShowLogWithMode = func;
}


extern "C" void __msl_assertion_failed(char const *condition, char const *filename, char const *funcname, int lineno)
{
	DebugStringToFile (condition, 0, filename, lineno, kAssert, 0);
}

inline bool ContainsNewLine (const char* c)
{
	while (*c != '\0')
	{
		if (*c == '\n')
			return true;
		c++;
	}
	return false;
}


static void CompilerErrorAnalytics (int mode, const char *condition)
{
#if UNITY_EDITOR
	if ( mode & kScriptCompileError )
	{
		std::string message = condition;

		// The compiler error message is formatted like this:
		//	 filename: error/warning number: description
		int n1 = message.find(": ");
		if ( n1 != string::npos )
		{
			int n2 = message.find(": ", n1+2);
			if ( n2 != string::npos )
			{
				string error = message.substr(n1+2, n2-n1-2);
				string description = message.substr(n2+2);
				//AnalyticsTrackEvent("Compiler", error, description, 1);
				return;
			}
		}
		//AnalyticsTrackEvent("Compiler", "Unknown", message, 1);
	}
#endif
}

bool DefaultCleanLogHandlerv (LogType logType, const char* log, va_list alist)
{
	va_list list;
	va_copy (list, alist);

#if UNITY_ANDROID	// On Android we don't use a separate clean log-file, but instead we clean up the actual logcat
#define LOG_PRINTF(x, ...) __android_log_vprint(ANDROID_LOG_INFO, "Unity", __VA_ARGS__)
#define LOG_FLUSH(x)
#else
#define LOG_PRINTF vfprintf
#define LOG_FLUSH fflush
#endif

LOG_PRINTF (gReproductionLogFile, log, list);
LOG_FLUSH( gReproductionLogFile );

#undef LOG_PRINTF
#undef LOG_FLUSH

	va_end (list);

	return true;
}

void CleanLogHandler(LogType logType, const char* log, ...)
{
	if (gCleanLogEntryHandlers != NULL)
	{
		for (std::list<LogEntryHandler>::iterator it = gCleanLogEntryHandlers->begin();
			 it != gCleanLogEntryHandlers->end();
			 it++)
		{
			va_list vl;
			va_start(vl, log);
			(**it)(logType, log, vl);
		}
	}
}


typedef void PrintConsole (const char* log, ...);

// convert the log mode to the cs LogType enum
// LogType.Error = 0, LogType.Assert = 1, LogType.Warning = 2, LogType.Log = 3, LogType.Exception = 4
inline LogType LogModeToLogType(int mode)
{
	LogType logType;
	if ( mode & (kScriptingException) ) logType = LogType_Exception;
	else if ( mode & (kError | kFatal | kScriptingError | kScriptCompileError | kStickyError | kAssetImportError | kGraphCompileError) ) logType = LogType_Error;
	else if ( mode & kAssert ) logType = LogType_Assert;
	else if ( mode & (kScriptingWarning | kScriptCompileWarning | kAssetImportWarning) ) logType = LogType_Warning;
	else logType = LogType_Log;

	return logType;
}

void DebugStringToFilePostprocessedStacktrace (const char* condition, const char* strippedStacktrace, const char* stacktrace, int errorNum, const char* file, int line, int mode, int objectInstanceID, int identifier)
{
	LogType logType = LogModeToLogType(mode);

	#if !UNITY_EXTERNAL_TOOL
	int depth = gRecursionLock;
	if (depth == 1)
		return;
	gRecursionLock = 1;

	if ( gLogCallbackFunc)
	{
#if SUPPORT_THREADS
		if (gLogCallbackFuncThreadSafe || Thread::CurrentThreadIsMainThread())
#endif
			gLogCallbackFunc (condition, strippedStacktrace, (int)logType);
	}

	#endif
#if UNITY_WII
	if (mode & kFatal)
	{
		wii::DbgFatalError ("%s\n", condition);
	}
	else if (mode & (kAssert | kError))
	{
		wii::DbgOutput ("%s\n", condition);
	}
#elif UNITY_XENON && MASTER_BUILD
	return;
#endif
	CompilerErrorAnalytics (mode, condition);

	string conditionAndStacktrace = condition;
	if ( stacktrace )
	{
		conditionAndStacktrace += "\n";
		conditionAndStacktrace += stacktrace;
	}

	string conditionAndStrippedStacktrace = condition;
	if ( stacktrace )
	{
		conditionAndStrippedStacktrace += "\n";
		conditionAndStrippedStacktrace += strippedStacktrace;
	}

	if (errorNum)
		CleanLogHandler (logType, "%s (Error: %d)\n\n", condition, errorNum);
	else
		CleanLogHandler (logType, "%s\n\n", condition);


	PrintConsole* printConsole = InternalIgnoreConsole;
	// Logs
	if (mode & (kLog | kScriptingLog))
		printConsole = InternalLogConsole;
	else if (mode & (kScriptingWarning | kAssetImportWarning))
		printConsole = InternalWarningConsole;
	else if (mode & kAssert)
		printConsole = InternalAssertConsole;
	// Real errors ---- YOU WANT TO BREAKPOINT THIS LINE!
	else
		printConsole = InternalErrorConsole;


	if (errorNum)
	{
		if (ContainsNewLine (conditionAndStacktrace.c_str()))
			printConsole ("%s \n(Error: %li Filename: %s Line: %li)\n\n", conditionAndStacktrace.c_str(), errorNum, file, line);
		else
			printConsole ("%s (Error: %li Filename: %s Line: %li)\n", conditionAndStacktrace.c_str(), errorNum, file, line);
	}
	else
	{
		if (ContainsNewLine (conditionAndStacktrace.c_str()))
			printConsole ("%s \n(Filename: %s Line: %li)\n\n", conditionAndStacktrace.c_str(), file, line);
		else
			printConsole ("%s (Filename: %s Line: %li)\n", conditionAndStacktrace.c_str(), file, line);
	}

	if (gLogToConsoleFunc)
		gLogToConsoleFunc (conditionAndStrippedStacktrace, errorNum, file, line, mode, objectInstanceID, identifier);

	#if WEBPLUG
	#if DEBUGMODE && 0
	if (mode & kAssert)
	{
		DebugBreak();
	}
	#endif
	if (mode & kFatal)
	{
		ExitWithErrorCode(kErrorFatalException);
	}
	#elif UNITY_WINRT
	if (mode & kAssert)
	{
		// Used to set a breakpoint
		int s = 5;
	}
	if (mode & kFatal)
		__debugbreak();
	#endif

	#if !UNITY_EXTERNAL_TOOL
	gRecursionLock = 0;
	#endif
}

//PROFILER_INFORMATION (gProfilerLogString, "LogStringToConsole",	kProfilerOther);

void DebugStringToFile (const char* condition, int errorNum, const char* file, int line, int mode, int objectInstanceID, int identifier)
{
	//PROFILER_AUTO(gProfilerLogString, NULL);
	SET_ALLOC_OWNER(NULL);
#if UNITY_ANDROID && i386
	printf_console("%s: %d at %s:%d (%d, %d, %d)\n", condition, errorNum, file, line, mode, objectInstanceID, identifier);
#endif

#if UNITY_FLASH || UNITY_WEBGL
	printf_console(condition);
	if (!(mode&(kScriptingWarning | kScriptingLog ))){
#if UNITY_FLASH
		Ext_Flash_LogCallstack();//Let's only switch this on for internal debugging, it sucks having to go through callstacks because of a log.
#elif UNITY_WEBGL
	//	__asm __volatile__("console.log(new Error().stack)");
#endif
	}else{
		return;
	}
#endif

	string stackTrace;
	string strippedStackTrace;
	string preprocessedFile;
	if (gPreprocessor)
	{
		preprocessedFile = file;
		string conditionStr = condition;

		gPreprocessor (conditionStr, strippedStackTrace, stackTrace, errorNum, preprocessedFile, &line, mode, objectInstanceID);

		file = preprocessedFile.c_str ();
	}

	DebugStringToFilePostprocessedStacktrace (condition, strippedStackTrace.c_str(), stackTrace.c_str(), errorNum, file, line, mode, objectInstanceID, identifier);

}

void RemoveErrorWithIdentifierFromConsole (int identifier)
{
	if (gRemoveLog)
		gRemoveLog (identifier);
}

void ShowErrorWithMode (int identifier)
{
	if (gShowLogWithMode)
		gShowLogWithMode (identifier);
}


void DebugTextLineByLine(const char* text, int maxLineLen)
{
	if(maxLineLen == -1)
		maxLineLen = 1023;

	// on android we can output maximum 1023 chars (1024 including \0)
#if UNITY_ANDROID
 	if(maxLineLen > 1023)
 		maxLineLen = 1023;
#endif

	#define SKIP_CRLF(ptr)										\
	do{															\
		while( (*ptr == '\r' || *ptr == '\n') && *ptr != 0 )	\
			++ptr;												\
	} while(0)

	#define SKIP_UNTIL_CRLF(ptr)							\
	do{														\
		while( *ptr != '\r' && *ptr != '\n' && *ptr != 0 )	\
			++ptr;											\
	} while(0)


	const char* lineStart 	= text;
	SKIP_CRLF(lineStart);

	std::string out;
	while(*lineStart != 0)
	{
		const char* lineEnd = lineStart;
		SKIP_UNTIL_CRLF(lineEnd);

		if(lineEnd - lineStart > maxLineLen)
			lineEnd = lineStart + maxLineLen;

		bool needSkipCRLF = *lineEnd == '\r' || *lineEnd == '\n';

		out.assign(lineStart, lineEnd-lineStart);
        #if UNITY_ANDROID
		    __android_log_print( ANDROID_LOG_DEBUG, "Unity", "%s", out.c_str());
        #else
			printf_console("%s\n", out.c_str());
        #endif

		lineStart = lineEnd;
		if(needSkipCRLF)
			SKIP_CRLF(lineStart);
	}

	#undef SKIP_UNTIL_CRLF
	#undef SKIP_CRLF
}


#if UNITY_IPHONE || UNITY_ANDROID

#if UNITY_IPHONE
#include <execinfo.h>
#elif UNITY_ANDROID
#include "PlatformDependent/AndroidPlayer/utils/backtrace_impl.h"
#endif

void DumpCallstackConsole( const char* prefix, const char* file, int line )
{
	const size_t	kMaxDepth = 100;

	size_t			stackDepth;
	void*			stackAddr[kMaxDepth];
	char**			stackSymbol;

	stackDepth  = backtrace(stackAddr, kMaxDepth);
	stackSymbol = backtrace_symbols(stackAddr, stackDepth);

	printf_console("%s%s:%d\n", prefix, file, line);

	// TODO: demangle? use unwind to get more info?
	// start from 1 to bypass self
	for( unsigned stackI = 1 ; stackI < stackDepth ; ++stackI )
	{
	#if UNITY_IPHONE
		printf_console(" #%02d %s\n", stackI-1, stackSymbol[stackI]);
	#elif UNITY_ANDROID // just for now
		__android_log_print( ANDROID_LOG_DEBUG, "DEBUG", " #%02d %s\n", stackI-1, stackSymbol[stackI]);
	#endif
	}

	::free(stackSymbol);
}

#else

void DumpCallstackConsole( const char* /*prefix*/, const char* /*file*/, int /*line*/ )
{
}

#endif

#if UNITY_OSX || UNITY_IPHONE
#include <sys/sysctl.h>
// taken from apple technical note QA1361
bool EXPORT_COREMODULE IsDebuggerPresent()
{
	static bool debuggerPresent = false;
	static bool inited = false;

	if(!inited)
	{
		kinfo_proc info;
		::memset(&info, 0x00, sizeof(info));

		int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, ::getpid()};

		size_t size = sizeof(info);
		sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);

		debuggerPresent = (info.kp_proc.p_flag & P_TRACED) != 0;
		inited = true;
	}

	return debuggerPresent;
}
#endif

#if UNITY_LINUX || UNITY_TIZEN
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <errno.h>

bool IsDebuggerPresent ()
{
	int status = 0,
	    pid = -1,
	    result = 0;

#ifdef PR_SET_PTRACER
// Guard ancient versions (&*^$%@*&#^%$ build agents)
	// Enable tracing by self and children
	if (0 != prctl (PR_SET_PTRACER, getpid (), 0, 0, 0))
	{
		ErrorString (Format ("Unable to enable tracing: %s", strerror (errno)));
		return false;
	}
#endif

	pid = fork ();
	if (0 > pid)
	{
		ErrorString ("Error creating child process");
		return false;
	}

	if (0 == pid)
	{
		// Child
		int parent = getppid();

		// Attempt to attach to parent
		if (ptrace (PTRACE_ATTACH, parent, NULL, NULL) == 0)
		{
			// Debugger is not attached; continue parent once it stops
			waitpid (parent, NULL, WUNTRACED | WCONTINUED);
			ptrace (PTRACE_DETACH, getppid (), NULL, NULL);
			result = 0;
		}
		else
		{
			// Debugger is already tracing parent
			result = 1;
		}
		exit(result);
	}

	// Parent
	waitpid (pid, &status, 0);
	result = WEXITSTATUS (status);
#ifdef PR_SET_PTRACER
	// Clear tracing
	prctl (PR_SET_PTRACER, 0, 0, 0, 0);
#endif

	return (0 != result);
}
#endif


