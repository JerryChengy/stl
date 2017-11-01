#include "UnityPrefix.h"

#define HAS_POSIX_BACKTRACE UNITY_OSX || UNITY_ANDROID
#if HAS_POSIX_BACKTRACE && UNITY_OSX
#define POSIX_BACKTRACE_DECL extern "C"
#else
#define POSIX_BACKTRACE_DECL
#endif

#if UNITY_WIN
#include <winnt.h>
#include "StackWalker.h"
typedef USHORT (WINAPI *CaptureStackBackTraceType)(__in ULONG, __in ULONG, __out PVOID*, __out_opt PULONG);
CaptureStackBackTraceType backtrace = NULL;
// Then use 'backtrace' as if it were CaptureStackBackTrace

class MyStackWalker: public StackWalker
{
protected:
	virtual void OnSymInit(LPCSTR szSearchPath, DWORD symOptions, LPCSTR szUserName) {}
	virtual void OnLoadModule(LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCSTR symType, LPCSTR pdbName, ULONGLONG fileVersion) {}
	virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry)
	{
		if(*entry.undFullName != '\0')
			OnOutput(entry.undFullName);
		else if(*entry.undName != '\0')
			OnOutput(entry.undName);
		else if(*entry.name != '\0')
			OnOutput(entry.name);
		OnOutput("\n");
	}
	virtual void OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr) {}
};

class StringStackWalker: public MyStackWalker
{
public:
	StringStackWalker(): result(NULL) { }

	void SetOutputString(std::string &_result) {result = &_result;}
protected:
	std::string *result;

	virtual void OnOutput(LPCSTR szTest)
	{
		if(result)
			*result += szTest;
	}
};

class BufferStackWalker: public MyStackWalker
{
public:
	BufferStackWalker(): maxSize(0) { }

	void SetOutputBuffer(char *_buffer, int _maxSize)
	{
		buffer = _buffer;
		maxSize = _maxSize;
	}
protected:
	char *buffer;
	int maxSize;

	virtual void OnOutput(LPCSTR szTest)
	{
		while(maxSize > 1 && *szTest != '\0')
		{
			*(buffer++) = *(szTest++);
			maxSize--;
		}
		*buffer = '\0';
	}
};
#elif HAS_POSIX_BACKTRACE
POSIX_BACKTRACE_DECL int backtrace(void** buffer, int size);
POSIX_BACKTRACE_DECL char** backtrace_symbols(void* const* buffer, int size);
#include "Runtime/Mono/MonoIncludes.h"
#if UNITY_ANDROID
#include <corkscrew/backtrace.h>
#include "Runtime/Mono/MonoIncludes.h"
#endif
#endif

#if UNITY_WIN
StringStackWalker* g_StringStackWalker = NULL;
BufferStackWalker* g_BufferStackWalker = NULL;
#endif

void InitializeStackWalker(){
#if UNITY_WIN
	if (g_BufferStackWalker == NULL){
		g_BufferStackWalker = new BufferStackWalker();
	}
	if (g_StringStackWalker == NULL){
		g_StringStackWalker = new StringStackWalker();
	}
#endif
}

std::string GetStacktrace(bool includeMonoStackFrames)
{
	std::string trace;
#if UNITY_WIN
	InitializeStackWalker();
	g_StringStackWalker->SetOutputString(trace);
	if (includeMonoStackFrames)
		g_StringStackWalker->ShowCallstackSimple();
	else
		g_StringStackWalker->ShowCallstack();
#elif HAS_POSIX_BACKTRACE
	void* backtraceFrames[128];
	int frameCount = backtrace(&backtraceFrames[0], 128);
	char** frameStrings = backtrace_symbols(&backtraceFrames[0], frameCount);

	if(frameStrings != NULL)
	{
		for(int x = 0; x < frameCount; x++)
		{
			if(frameStrings[x] != NULL)
			{
				std::string str = frameStrings[x];
				if(str.find("???") != std::string::npos)
				{
					// If the symbol could not be resolved, try if mono knows about the address.
					unsigned int addr = 0;
					if (sscanf(str.substr(str.find("0x")).c_str(), "0x%x", &addr))
					{
						if(addr == -1){
							break;
						}
						const char *mono_frame = mono_pmip((void*)addr);
						// If mono knows the address, replace the stack trace line with
						// the information from mono.
						if (mono_frame)
							str = str.substr(0,4) + "[Mono JITed code]                  " + mono_frame;
					}
				}
				trace = trace + str + '\n';
			}
		}
		free(frameStrings);
	}
#else
	trace = "Stacktrace is not supported on this platform.";
#endif
	return trace;
}

UInt32 GetStacktrace(void** trace, int maxFrames, int startframe)
{
#if UNITY_WIN
	if(!backtrace)
		backtrace = (CaptureStackBackTraceType)(GetProcAddress(LoadLibrary("kernel32.dll"), "RtlCaptureStackBackTrace"));
	DWORD dwHashCode = 0;
	int count = backtrace(startframe, maxFrames-1, trace, &dwHashCode);
	trace[count] = 0;
	return dwHashCode;
	//	InitializeStackWalker();
	//	g_BufferStackWalker->GetCurrentCallstack(trace,maxFrames);
#elif HAS_POSIX_BACKTRACE
	int framecount = backtrace(trace, maxFrames);
	trace[framecount] = 0;
	int index = startframe;
	UInt32 hash = 0;
	while(trace[index])
	{
		trace[index-startframe] = trace[index];
		hash ^= (UInt32)(trace[index]) ^ (hash << 7) ^ (hash >> 21);
		index++;
	}
	trace[index-startframe] = 0;
	return hash;
#endif
	return 0;
}

void GetReadableStackTrace(char* output, int bufsize, void** input, int frameCount)
{
#if UNITY_WIN
	InitializeStackWalker();
	g_BufferStackWalker->GetStringFromStacktrace(output, bufsize, input);
#elif HAS_POSIX_BACKTRACE
	char **frameStrings = backtrace_symbols(input, frameCount);
	std::string trace;
	if(frameStrings != NULL)
	{
		for(int x = 0; x < frameCount; x++)
		{
			if(frameStrings[x] != NULL)
			{
#if UNITY_ANDROID
				char line[256] = {0};
				snprintf(line, 255, " #%02d %s\n", x, frameStrings[x]);
				trace = trace + line;
#else
				trace = trace + frameStrings[x] + '\n';
#endif
			}
		}
		free(frameStrings);
	}
	strcpy(output,trace.c_str());
#endif

}

void GetStacktrace(char *trace, int maxSize, int maxFrames)
{
#if UNITY_WIN
	InitializeStackWalker();
	g_BufferStackWalker->SetOutputBuffer(trace, maxSize);
	g_BufferStackWalker->ShowCallstack(
		GetCurrentThread(),
		NULL,
		NULL,
		NULL,
		maxFrames);
#elif HAS_POSIX_BACKTRACE
	void *backtraceFrames[128];
	int frameCount = backtrace(&backtraceFrames[0], 128);
	char **frameStrings = backtrace_symbols(&backtraceFrames[0], frameCount);
	frameCount = std::min(frameCount, maxFrames);
	if(frameStrings != NULL)
	{
		for(int x = 0; x < frameCount; x++)
		{
			if(frameStrings[x] != NULL)
			{
#if UNITY_ANDROID
				int numChars = snprintf(trace, maxSize, " #%02d %s\n", x, frameStrings[x]);
#else
				int numChars = snprintf(trace, maxSize, "%s\n", frameStrings[x]);
#endif
				trace += numChars;
				maxSize -= numChars;
			}
			if(maxSize <= 0)
				break;
		}
		free(frameStrings);
	}
#else
	strncpy(trace, "Stacktrace is not supported on this platform.", maxSize);
#endif
}
