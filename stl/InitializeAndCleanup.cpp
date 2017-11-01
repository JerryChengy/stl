#include "UnityPrefix.h"
#include "InitializeAndCleanup.h"
#include <algorithm>
enum { kMaxCount = 40 };
#define Assert(x)
struct OrderedCallback
{
	int														order;
	RegisterRuntimeInitializeAndCleanup::CallbackFunction*	init;
	RegisterRuntimeInitializeAndCleanup::CallbackFunction*	cleanup;
};

static int				gNumRegisteredCallbacks = 0;
static OrderedCallback	gCallbacks[kMaxCount];


bool operator < (const OrderedCallback& lhs, const OrderedCallback& rhs)
{
	return lhs.order < rhs.order;
}

RegisterRuntimeInitializeAndCleanup::RegisterRuntimeInitializeAndCleanup(CallbackFunction* Initialize, CallbackFunction* Cleanup, int order)
{
	gCallbacks[gNumRegisteredCallbacks].init = Initialize;
	gCallbacks[gNumRegisteredCallbacks].cleanup = Cleanup;
	gCallbacks[gNumRegisteredCallbacks].order = order;

	gNumRegisteredCallbacks++;
	Assert(gNumRegisteredCallbacks <= kMaxCount);
}

void RegisterRuntimeInitializeAndCleanup::ExecuteInitializations()
{
	std::sort (gCallbacks, gCallbacks + gNumRegisteredCallbacks);

	for (int i = 0; i < gNumRegisteredCallbacks; i++)
	{
		if (gCallbacks[i].init)
			gCallbacks[i].init ();
	}
}

void RegisterRuntimeInitializeAndCleanup::ExecuteCleanup()
{
	for (int i = gNumRegisteredCallbacks-1; i >=0 ; i--)
	{
		if (gCallbacks[i].cleanup)
			gCallbacks[i].cleanup ();
	}
}

