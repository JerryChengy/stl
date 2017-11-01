#pragma once

#include "dynamic_array.h"
#include "Mutex.h"
#include <map>

class ConstantStringManager
{
#define PROFILE_COMMON_STRINGS !UNITY_RELEASE

#if PROFILE_COMMON_STRINGS
	typedef std::map<std::string, int> CommonStringCounter;
	CommonStringCounter m_CommonStringCounter;
	Mutex               m_CommonStringMutex;
#endif

	dynamic_array<const char*> m_Strings;
	void ProfileCommonString (const char* str);

public:

	ConstantStringManager ();
	~ConstantStringManager ();

	void AddConstantStrings (const char** strings, size_t size );
	void AddConstantString (const char* strings );

	const char* GetConstantString(const char* str);
	const char* GetEmptyString();

	// Init
	static void StaticInitialize ();
	static void StaticCleanup ();

	void DumpCommonStrings ();

};

ConstantStringManager& GetConstantStringManager ();
