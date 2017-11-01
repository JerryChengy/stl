#include "UnityPrefix.h"
#include "ConstantStringManager.h"
#include "InitializeAndCleanup.h"

static ConstantStringManager* gConstantStringManager = NULL;

void ConstantStringManager::ProfileCommonString (const char* str)
{
#if PROFILE_COMMON_STRINGS
	SET_ALLOC_OWNER(this);
	m_CommonStringMutex.Lock();
	m_CommonStringCounter[str]++;
	m_CommonStringMutex.Unlock();
#endif
}

void ConstantStringManager::DumpCommonStrings ()
{
#if PROFILE_COMMON_STRINGS
	for(CommonStringCounter::iterator i=m_CommonStringCounter.begin();i != m_CommonStringCounter.end();++i)
	{
		//printf_console("%d\t -> '%s'\n", i->second, i->first.c_str());
	}
#endif
}

ConstantStringManager::ConstantStringManager ()
{
	AddConstantString("");
	AddConstantString("TextMesh");
}

ConstantStringManager::~ConstantStringManager ()
{
	for (int i=0;i<m_Strings.size();i++)
	{
		UNITY_FREE(kMemStaticString, (void*)m_Strings[i]);
	}
}

const char* ConstantStringManager::GetEmptyString()
{
	return m_Strings[0];
}

const char* ConstantStringManager::GetConstantString(const char* str)
{
	ProfileCommonString (str);

	for (int i=0;i<m_Strings.size();i++)
	{
		if (strcmp(m_Strings[i], str) == 0)
			return m_Strings[i];
	}

	return NULL;
}

void ConstantStringManager::AddConstantString (const char* string )
{
	SET_ALLOC_OWNER(gConstantStringManager);
	int length = strlen(string);
	char* newString = (char*)UNITY_MALLOC(kMemStaticString, length+1);
	memcpy(newString, string, length+1);
	m_Strings.push_back(newString);
}

void ConstantStringManager::AddConstantStrings (const char** strings, size_t size )
{
	m_Strings.resize_uninitialized(size + m_Strings.size());
	for (int i=0;i<size;++i)
		AddConstantString(strings[i]);
}

void ConstantStringManager::StaticInitialize ()
{
	gConstantStringManager = UNITY_NEW_AS_ROOT(ConstantStringManager, kMemString, "SharedStrings", "");
}

void ConstantStringManager::StaticCleanup ()
{
	// gConstantStringManager->DumpCommonStrings ();
	UNITY_DELETE(gConstantStringManager, kMemString);
	gConstantStringManager = NULL;
}
static RegisterRuntimeInitializeAndCleanup s_ConstantStringManagerCallbacks(ConstantStringManager::StaticInitialize, ConstantStringManager::StaticCleanup);

ConstantStringManager& GetConstantStringManager ()
{
	return *gConstantStringManager;
}
