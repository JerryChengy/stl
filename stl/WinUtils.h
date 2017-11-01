#ifndef __WIN_UTILS_H
#define __WIN_UTILS_H

#include <string>
#include "Annotations.h"

namespace winutils
{
	EXTERN_C IMAGE_DOS_HEADER __ImageBase;

	inline HINSTANCE GetCurrentModuleHandle()
	{
		return reinterpret_cast<HINSTANCE>(&__ImageBase);
	}

	enum WindowsVersion {
		kWindows2000 = 50,	// 5.0
		kWindowsXP = 51,	// 5.1
		kWindows2003 = 52,	// 5.2
		kWindowsVista = 60,	// 6.0
		kWindows7 = 61,		// 6.1
		kWindows8 = 62,		// 6.2
	};

	int GetWindowsVersion();

	void SetDontDisplayDialogs(bool dontDisplayDialogs);
	bool GetDontDisplayDialogs();

#if UNITY_EDITOR
	void RedirectStdoutToConsole();
	bool IsApplicationRunning( std::string appPath );
#endif

	void AddErrorMessage( const char* fmt, ... );
	const std::string& GetErrorMessages();
	DOES_NOT_RETURN void DisplayErrorMessagesAndQuit( const char* error );

	std::string ErrorCodeToMsg( DWORD code );
	void RegisterLogToDebugOutput();

	void EnableWindowsKey();
	void DisableWindowsKey();

	// In web player, we should use our DLL instance handle, not the containing
	// application's one. So make function to set it up.
	HINSTANCE GetInstanceHandle();
	void SetInstanceHandle( HINSTANCE instance );

#if !UNITY_WINRT
	ATOM RegisterWindowClass( const wchar_t* className, WNDPROC windowProc, unsigned int style );
	void UnregisterWindowClass( const wchar_t* className );
#endif

	HWND GetWindowTopmostParent( HWND wnd );

#if DEBUGMODE
	std::string GetWindowsMessageInfo( UINT message, WPARAM wparam, LPARAM lparam );
#endif
	void DebugMessageBox(LPCSTR format, ...);

	void CenterWindowOnParent( HWND window );

	struct AutoHandle
	{
		explicit AutoHandle() : handle(INVALID_HANDLE_VALUE) { }
		AutoHandle( HANDLE h ) : handle(h) { }
		~AutoHandle() { Close(); }

		void Close()
		{
			if(handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(handle);
				handle = INVALID_HANDLE_VALUE;
			}
		}
		HANDLE	handle;
	};


	// There are two types of crash in Unity - one which comes from C++ code and the second one which comes from mono (see MonoManager.cpp)
	// The crashes from C++ are catched, but the crashes from mono are processed by int __cdecl HandleSignal( EXCEPTION_POINTERS* ep )
	// So we need to call this function from there
	int ProcessInternalCrash(PEXCEPTION_POINTERS pExInfo, bool passToDefaultCrashHandler);

	// Adds additional exception handler, so during the crash the callstack could be outputed to Editor.log
	// This won't override crash handler from the CrashHandlerLib.
	// So during the crash, the following sequence will be performed:
	// 1. ProcessInternalCrash in this file
	// 2. InternalProcessCrash in CrashHandler library
	void SetupInternalCrashHandler();


}; // namespace winutils

#define WIN_ERROR_TEXT(errorCode)	(winutils::ErrorCodeToMsg(errorCode).c_str())
#define WIN_LAST_ERROR_TEXT WIN_ERROR_TEXT(GetLastError())

#endif
