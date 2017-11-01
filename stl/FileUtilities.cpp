#include "UnityPrefix.h"
#include "FileUtilities.h"
#include "GUID.h"


using namespace std;


#if UNITY_HAVE_GUID_INIT

string GetUniqueTempPath (const std::string& basePath)
{
	while (true)
	{
		UnityGUID guid; guid.Init();
		string tmpFilePath = basePath + GUIDToString(guid);
		if (!IsFileCreated(tmpFilePath))
			return tmpFilePath;
	}
}

string GetUniqueTempPathInProject ()
{
	return GetUniqueTempPath ("Temp/UnityTempFile-");
}

#endif
