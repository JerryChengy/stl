#include "UnityPrefix.h"
#include "Argv.h"
#if UNITY_WIN
#include "WinUtils.h"
#endif
#include "Word.h"
using namespace std;

static int argc;
static const char** argv;
static vector<string> relaunchArguments;

struct KnownArguments
{
	bool isBatchmode;
	bool isAutomated;
};

static KnownArguments knownArgs;

void SetupArgv (int a, const char** b) 
{ 
	argc = a; 
	argv = b; 
	knownArgs.isBatchmode = HasARGV ("batchmode");
	knownArgs.isAutomated = HasARGV ("automated");
}

bool HasARGV (const string& name)
{
	for (int i=0;i<argc;i++)
	{
		if (StrICmp (argv[i], "-" + name) == 0)
			return true;
	}
	return false;
}

bool IsBatchmode ()
{
	return knownArgs.isBatchmode;
}

bool IsHumanControllingUs ()
{
	return !(knownArgs.isBatchmode || knownArgs.isAutomated);
}

void SetIsBatchmode (bool value)
{
	knownArgs.isBatchmode = true;
}

void PrintARGV ()
{
	for (int i=0;i<argc;i++)
	{
		printf_console ("%s\n", argv[i]);
	}
}

vector<string> GetValuesForARGV (const string& name)
{
	vector<string> values;
	values.reserve (argc);

	bool found = false;
	for (int i=0;i<argc;i++)
	{
		if (found)
		{
			if (argv[i][0] == '-')
				return values;
			else
				values.push_back (argv[i]);
		}
		else if (StrICmp (argv[i], "-" + name) == 0)
			found = true;
	}

	return values;
}

string GetFirstValueForARGV (const string& name)
{
	vector<string> values = GetValuesForARGV (name);
	if (values.empty ())
		return string ();
	else
		return values[0];
}

vector<string> GetAllArguments()
{
	vector<string> values;
	values.reserve (argc);
	for (int i=1;i<argc;i++)
		values.push_back (argv[i]);
	return values;
}

void SetRelaunchApplicationArguments (const vector<string>& args)
{
	relaunchArguments = args;
}

vector<string> GetRelaunchApplicationArguments ()
{
	return relaunchArguments;
}

void CheckBatchModeErrorString (const string& error)
{
	if (error.empty ())
		return;

	ErrorString(error);

	if (!IsBatchmode())
		return;

#if UNITY_WIN && UNITY_EDITOR
//	winutils::RedirectStdoutToConsole();
#elif UNITY_OSX
	ResetStdout();
#endif
	printf_console ("\nAborting batchmode due to failure:\n%s\n\n", error.c_str());
	exit(1);
}
