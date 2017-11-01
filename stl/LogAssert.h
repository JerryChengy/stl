#ifndef LOGASSERT_H
#define LOGASSERT_H

class Object;
#include <string>
#include <set>
#include <stdarg.h>
#include <stdio.h>
#include "Annotations.h"
#include "FileStripped.h"
#if UNITY_EXTERNAL_TOOL
#define EXPORT_COREMODULE
#else
#include "ExportModules.h"
#endif

#if UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
#include <signal.h>
#endif

#if UNITY_OSX || UNITY_IPHONE || UNITY_LINUX
extern bool IsDebuggerPresent();
#endif

enum
{
	kError = 1 << 0,
	kAssert = 1 << 1,
	kLog = 1 << 2,
	kFatal = 1 << 4,
	kAssetImportError = 1 << 6,
	kAssetImportWarning = 1 << 7,
	kScriptingError = 1 << 8,
	kScriptingWarning = 1 << 9,
	kScriptingLog = 1 << 10,
	kScriptCompileError = 1 << 11,
	kScriptCompileWarning = 1 << 12,
	kStickyError = 1 << 13,
	kMayIgnoreLineNumber = 1 << 14,
	kReportBug = 1 << 15,
	kDisplayPreviousErrorInStatusBar = 1 << 16,
	kScriptingException = 1 << 17,
	kDontExtractStacktrace = 1 << 18,
	kGraphCompileError = 1 << 20,
};

/// The type of the log message in the delegate registered with Application.RegisterLogCallback.
///
enum LogType
{
	/// LogType used for Errors.
	LogType_Error = 0,
	/// LogType used for Asserts. (These indicate an error inside Unity itself.)
	LogType_Assert = 1,
	/// LogType used for Warnings.
	LogType_Warning = 2,
	/// LogType used for regular log messages.
	LogType_Log = 3,
	/// LogType used for Exceptions.
	LogType_Exception = 4,
	/// LogType used for Debug.
	LogType_Debug = 5,
	///
	LogType_NumLevels
};

inline const char* LogTypeToString (LogType type)
{
	switch (type)
	{
	case LogType_Assert:    return "Assert";
	case LogType_Debug:     return "Debug";
	case LogType_Exception: return "Exception";
	case LogType_Error:     return "Error";
	case LogType_Log:       return "Log";
	case LogType_Warning:   return "Warning";
	default:                return "";
	}
}

typedef void LogToConsoleImpl (const std::string& condition, int errorNum, const char* file, int line, int type, int targetObjectInstanceID, int identifier);
typedef void LogCallback (const std::string& condition, const std::string &stackTrace, int type);
typedef void PreprocessCondition (const std::string& condition, std::string &strippedStacktrace, std::string &stackTrace, int errorNum, std::string& file, int* line, int type, int targetInstanceID);
typedef void RemoveLogFunction (int identifier);

/// Callback function that is invoked before a log message is written to the log output.  Return true to
/// continue logging the message as normal or false to mark the log message as handled and prevent normal
/// processing of it.
typedef bool (*LogEntryHandler) (LogType logType, const char* log, va_list list);

void RegisterLogToConsole (LogToConsoleImpl* func);
void RegisterLogCallback (LogCallback* callback, bool threadSafe);
void RegisterLogPreprocessor (PreprocessCondition* func);
void RegisterRemoveImportErrorFromConsole (RemoveLogFunction* func);
void RegisterShowErrorWithMode (RemoveLogFunction* func);

void DebugStringToFilePostprocessedStacktrace (const char* condition, const char* strippedStacktrace, const char* stacktrace, int errorNum, const char* file, int line, int mode, int targetInstanceID = 0, int identifier = 0);
void EXPORT_COREMODULE DebugStringToFile (const char* condition, int errorNum, const char* file, int line, int mode, int targetInstanceID = 0, int identifier = 0);
template<typename alloc>
void EXPORT_COREMODULE DebugStringToFile (const std::basic_string<char, std::char_traits<char>, alloc>& condition, int errorNum, const char* file, int line, int mode, const int objectInstanceID = 0, int identifier = 0)
{
	DebugStringToFile (condition.c_str (), errorNum, file, line, mode, objectInstanceID, identifier);
}

void SetLogEntryHandler(LogEntryHandler newHandler);
void AddCleanLogEntryHandler(LogEntryHandler newHandler);
void ReleaseLogHandlers();

void DumpCallstackConsole( const char* prefix, const char* file, int line );
#define DUMP_CALLSTACK(message) DumpCallstackConsole(message, __FILE_STRIPPED__, __LINE__)

void DebugTextLineByLine(const char* text, int maxLineLen=-1);

#define ErrorIf(x)							do{ if (x) DebugStringToFile (#x, 0, __FILE_STRIPPED__, __LINE__, kError); ANALYSIS_ASSUME(!(x)); }while(0)
#define ErrorAndReturnValueIf(x, y)			do{ if (x) { DebugStringToFile (#x, 0, __FILE_STRIPPED__, __LINE__, kError); return y; ANALYSIS_ASSUME(!(x)); } else { ANALYSIS_ASSUME(!(x)); } }while(0)
#define ErrorAndReturnIf(x)					ErrorAndReturnValueIf( x, /**/ )

#define ErrorString(x)						do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kError); }while(0)
#define ErrorStringWithoutStacktrace(x)		do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kDontExtractStacktrace | kError); }while(0)
#define ErrorStringMsg(...)					do{ DebugStringToFile (Format(__VA_ARGS__), 0, __FILE_STRIPPED__, __LINE__, kError); }while(0)
#define WarningString(x)					do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kScriptingWarning); }while(0)
#define WarningStringWithoutStacktrace(x)	do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kDontExtractStacktrace | kScriptingWarning); }while(0)
#define WarningStringMsg(...)				do{ DebugStringToFile (Format(__VA_ARGS__), 0, __FILE_STRIPPED__, __LINE__, kScriptingWarning); }while(0)


#define ErrorOSErr(x)						do{ int XERRORRESULT = x; if (XERRORRESULT)	DebugStringToFile (#x, XERRORRESULT, __FILE_STRIPPED__, __LINE__, kError); }while(0)

/// These errors pass an Object* as the place in which object the error occurred
#define ErrorIfObject(x,o)					do{ if (x) DebugStringToFile (#x, 0, __FILE_STRIPPED__, __LINE__, kError, (o) ? (o)->GetInstanceID() : 0); }while(0)
#define ErrorStringObject(x,o)				do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kError, (o) ? (o)->GetInstanceID() : 0); }while(0)
#define WarningStringObject(x,o)			do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kScriptingWarning, (o) ? (o)->GetInstanceID() : 0); }while(0)

#define LogStringObject(x,o)				do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kLog, (o) ? (o)->GetInstanceID() : 0); }while(0)
#define LogString(x)						do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kLog); }while(0)
#define LogStringMsg(...)					do{ DebugStringToFile (Format(__VA_ARGS__), 0, __FILE_STRIPPED__, __LINE__, kLog); }while(0)

#define FatalErrorOSErr(x)					do{ int XERRORRESULT = x; if (XERRORRESULT)	DebugStringToFile (#x, XERRORRESULT, __FILE_STRIPPED__, __LINE__, kError | kFatal | kReportBug); }while(0)
#define FatalErrorString(x)					do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kError | kFatal | kReportBug); }while(0)
#define FatalErrorIf(x)						do{ if (x) DebugStringToFile (#x, 0, __FILE_STRIPPED__, __LINE__, kError | kFatal | kReportBug); }while(0)
#define FatalErrorStringDontReport(x)		do{ DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kError | kFatal); }while(0)
#define FatalErrorMsg(...)					do{ DebugStringToFile (Format(__VA_ARGS__), 0, __FILE_STRIPPED__, __LINE__, kError | kFatal); }while(0)


#define ErrorFiniteParameter(x) if (!IsFinite(x)) { ErrorString("Invalid parameter because it was infinity or nan.");  return; }
#define ErrorFiniteParameterReturnFalse(x) if (!IsFinite(x)) { ErrorString("Invalid parameter because it was infinity or nan.");  return false; }


/***** MAKE SURE THAT AssertIf's only compare code to compare,	****/
/***** Which can safely be not called in non-debug mode			****/


// TODO: should have ASSERT_ENABLED define, but difficult to add now

#if defined(__ppc__)
#define DEBUG_BREAK		__asm__("li r0, 20\nsc\nnop\nli r0, 37\nli r4, 2\nsc\nnop\n" : : : "memory","r0","r3","r4" )
#elif UNITY_OSX
#define DEBUG_BREAK		if(IsDebuggerPresent()) __asm { int 3 }
#elif UNITY_WIN
#define DEBUG_BREAK		if (IsDebuggerPresent()) __debugbreak()
#elif UNITY_XENON || UNITY_METRO
#define DEBUG_BREAK		__debugbreak()
#elif UNITY_PS3
#define DEBUG_BREAK		__asm__ volatile ("tw 31,1,1 ")
#elif (UNITY_IPHONE && !TARGET_IPHONE_SIMULATOR)
#define DEBUG_BREAK		__asm__ __volatile__ ( "bkpt #0\n\t  bx lr\n\t" : : : )
#elif UNITY_ANDROID
#define DEBUG_BREAK		__builtin_trap()
#elif UNITY_LINUX
#define DEBUG_BREAK		if (IsDebuggerPresent ()) raise(SIGTRAP)
#else
#define DEBUG_BREAK
#endif

#ifndef ASSERT_SHOULD_BREAK
// On Metro breaking without debugger attached stops the program... So don't do it !
#define ASSERT_SHOULD_BREAK DEBUGMODE && !UNITY_RELEASE && !UNITY_METRO
#endif

#if ASSERT_SHOULD_BREAK
#define ASSERT_BREAK DEBUG_BREAK
#else
#define ASSERT_BREAK
#endif


#if DEBUGMODE

#define AssertIf(x)												\
	do {															\
	if(x)														\
{															\
	DebugStringToFile (#x, 0, __FILE_STRIPPED__, __LINE__, kAssert);	\
	ASSERT_BREAK;											\
	ANALYSIS_ASSUME(!(x));									\
}															\
	} while(0)

#define AssertIfObject(x,o)																	\
	do {																						\
	if(x)																					\
{																						\
	DebugStringToFile (#x, 0, __FILE_STRIPPED__, __LINE__, kAssert, (o)?(o)->GetInstanceID():0);	\
	ASSERT_BREAK;																		\
	ANALYSIS_ASSUME(!(x));																\
}																						\
	} while(0)



#define Assert(x)												\
	do {															\
	if(!(x))													\
{															\
	DebugStringToFile (#x, 0, __FILE_STRIPPED__, __LINE__, kAssert);	\
	ASSERT_BREAK;											\
	ANALYSIS_ASSUME(x);										\
}															\
	} while(0)

#define AssertMsg(x,...)															\
	do {																				\
	if(!(x))																		\
{																				\
	DebugStringToFile (Format(__VA_ARGS__), 0, __FILE_STRIPPED__, __LINE__, kAssert);	\
	ASSERT_BREAK;																\
	ANALYSIS_ASSUME(x);															\
}																				\
	} while(0)

#define AssertMsgObject(x,o,...)													\
	do {																				\
	if(!(x))																		\
{																				\
	DebugStringToFile (Format(__VA_ARGS__), 0, __FILE_STRIPPED__, __LINE__, kAssert, (o) ? (o)->GetInstanceID() : 0);	\
	ASSERT_BREAK;																\
	ANALYSIS_ASSUME(x);															\
}																				\
	} while(0)


#else

#define AssertIf(x) 				do { (void)sizeof(x); } while(0)
#define AssertIfObject(x,o)			do { (void)sizeof(x); (void)sizeof(o); } while(0)
#define Assert(x) 					do { (void)sizeof(x); } while(0)
#define AssertMsg(x,...)			do { (void)sizeof(x); } while(0)
#define AssertMsgObject(x,o,...)	do { (void)sizeof(x); } while(0)

#endif


#if DEBUGMODE

#define AssertString(x)       { DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kAssert); }
#define AssertFiniteParameter(x) if (!IsFinite(x)) { AssertString("Invalid parameter because it was infinity or nan.");  }

/// These errors pass an Object* as the place in which object the error occurred
#define AssertStringObject(x,o)      { DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kAssert, (o) ? (o)->GetInstanceID() : 0); }
#define ScriptWarning(x,o)  { DebugStringToFile (x, 0, __FILE_STRIPPED__, __LINE__, kScriptingWarning); }
#else

#define AssertString(x) 	{  }
#define AssertFiniteParameter(x) {}

#define AssertStringObject(x,o) {  }
#define ScriptWarning(x,o) {  }
#endif


#if UNITY_RELEASE
#define DebugAssertIf(x)	do { (void)sizeof(x); } while(0)
#define DebugAssert(x) 		do { (void)sizeof(x); } while(0)
#define DebugAssertMsg(x, ...) {  }
#define AssertBreak(x) Assert(x)
#else
#define DebugAssertIf(x) AssertIf(x)
#define DebugAssert(x) Assert(x)
#define DebugAssertMsg(x, ...) AssertMsg(x, __VA_ARGS__)

#define AssertBreak(x)											\
	do {															\
	if(!(x))													\
{															\
	DEBUG_BREAK;											\
	Assert(x);												\
}															\
	} while(0)
#endif


#ifdef __cplusplus
extern "C"
{
#endif
	EXPORT_COREMODULE TAKES_PRINTF_ARGS(1,2) void printf_console (const char* string, ...);
#ifdef __cplusplus
}
#endif
void printf_consolev (LogType logType, const char* log, va_list list);

#if MASTER_BUILD
#define UNITY_TRACE(...)
#define UNITY_TRACEIF(condition, ...)
#else
#define UNITY_TRACE(...)				printf_console(__VA_ARGS__)
#define UNITY_TRACEIF(condition, ...)	if (condition) printf_console(__VA_ARGS__)
#endif

/// When logging an error it can be passed an identifier. The identifier can be used to remove errors from the console later on.
/// Eg. when reimporting an asset all errors generated for that asset should be removed before importing!
void RemoveErrorWithIdentifierFromConsole (int identifier);
void ShowErrorWithMode (int mode);

#if UNITY_OSX
void ResetStdout();
#endif

// Route input to a custom outputfile out instead of Player.log
void LogOutputToSpecificFile (const char* path);
void InitializeCleanedLogFile (FILE* file);
std::string GetCleanedLogFile();
std::string GetConsoleLogPath();

#if UNITY_EDITOR
std::string GetEditorConsoleLogPath ();
std::string GetPlayerConsoleLogPath ();
std::string GetMonoDevelopLogPath ();
#endif

#if UNITY_WIN
FILE* OpenConsole ();
std::string SetLogFilePath(std::string const& path);
#endif

#if defined(_MSC_VER)
#define PRINTF_SIZET_FORMAT "Iu"
#else
#define PRINTF_SIZET_FORMAT "zu"
#endif

#endif
