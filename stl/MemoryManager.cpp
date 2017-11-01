#include "UnityPrefix.h"
#include "MemoryManager.h"
#include "MemoryUtilities.h"
#include "BitUtility.h"
#include "AtomicOps.h"
#include "MemoryProfiler.h"
//void* malloc_internal(size_t size, int align, MemLabelRef label, int allocateOptions, const char* file, int line)
//{
//	return GetMemoryManager ().Allocate(size, align, label, allocateOptions, file, line);
//}
// under new clang -fpermissive is no longer available, and it is quite strict about proper signatures of global new/delete
// eg throw() is reserved for nonthrow only
#define STRICTCPP_NEW_DELETE_SIGNATURES UNITY_OSX || UNITY_IPHONE
#if STRICTCPP_NEW_DELETE_SIGNATURES
	#define THROWING_NEW_THROW throw(std::bad_alloc)
#else
	#define THROWING_NEW_THROW throw()
#endif



#if UNITY_PS3
#include "PlatformDependent/PS3Player/Allocator/PS3DlmallocAllocator.h"
#endif

#define STOMP_MEMORY  !UNITY_PS3 && (!UNITY_RELEASE)

#define kMemoryManagerOverhead 	((sizeof(int) + kDefaultMemoryAlignment - 1) &  ~(kDefaultMemoryAlignment-1))

#if UNITY_XENON

#include "PlatformDependent/Xbox360/Source/XenonMemory.h"

#define UNITY_LL_ALLOC(l, s, a) xenon::trackedMalloc(s, a)
#define UNITY_LL_REALLOC(l, p, s, a) xenon::trackedRealloc(p, s, a)
#define UNITY_LL_FREE(l, p) xenon::trackedFree(p)

#elif UNITY_PS3

void* operator new (size_t size, size_t align) throw() { return GetMemoryManager().Allocate (size == 0 ? 4 : size, align, kMemNewDelete); }
void* operator new [] (size_t size, size_t align) throw() { return GetMemoryManager().Allocate (size == 0 ? 4 : size, align, kMemNewDelete); }

#include "PlatformDependent/PS3Player/Allocator/PS3Memory.h"

#define UNITY_LL_ALLOC(l,s,a) PS3Memory::Alloc(l,s,a)
#define UNITY_LL_REALLOC(l,p,s, a) PS3Memory::Realloc(l,p,s,a)
#define UNITY_LL_FREE(l,p) PS3Memory::Free(l, p)

#elif UNITY_ANDROID

#define UNITY_LL_ALLOC(l,s,a) ::memalign(a, s)
#define UNITY_LL_REALLOC(l,p,s,a) ::realloc(p, s)
#define UNITY_LL_FREE(l,p) ::free(p)

#else

#define UNITY_LL_ALLOC(l,s,a) ::malloc(s)
#define UNITY_LL_REALLOC(l,p,s,a) ::realloc(p, s)
#define UNITY_LL_FREE(l,p) ::free(p)

#endif

void* operator new (size_t size, MemLabelRef label, bool set_root, int align, const char* file, int line)
{
#if ENABLE_MEM_PROFILER
	bool root_was_set = false;
	if (set_root) root_was_set = push_allocation_root(NULL, false);
#endif
	void* p = malloc_internal (size, align, label, kAllocateOptionNone, file, line);
#if ENABLE_MEM_PROFILER
	if (root_was_set) pop_allocation_root();
	if (set_root) {
		GetMemoryProfiler()->RegisterRootAllocation(p, GetMemoryManager().GetAllocator(label), NULL, NULL);
		push_allocation_root(p, true);
	}
#endif
	return p;
}
void* operator new [] (size_t size, MemLabelRef label, bool set_root, int align, const char* file, int line)
{
#if ENABLE_MEM_PROFILER
	bool root_was_set = false;
	if (set_root) root_was_set = push_allocation_root(NULL, false);
#endif
	void* p = malloc_internal (size, align, label, kAllocateOptionNone, file, line);
#if ENABLE_MEM_PROFILER
	if (root_was_set) pop_allocation_root();
	if (set_root) {
		GetMemoryProfiler()->RegisterRootAllocation(p, GetMemoryManager().GetAllocator(label), NULL, NULL);
		push_allocation_root(p, true);
	}
#endif
	return p;
}
void operator delete (void* p, MemLabelRef label, bool /*set_root*/, int /*align*/, const char* /*file*/, int /*line*/) { free_alloc_internal (p, label); }
void operator delete [] (void* p, MemLabelRef label, bool /*set_root*/, int /*align*/, const char* /*file*/, int /*line*/) { free_alloc_internal (p, label); }

#if ENABLE_MEMORY_MANAGER
#include "UnityDefaultAllocator.h"
#include "DynamicHeapAllocator.h"
#include "LowLevelDefaultAllocator.h"
#include "StackAllocator.h"
#include "TLSAllocator.h"
#include "DualThreadAllocator.h"
#include "Thread.h"
#include "Allocator.h"
#include "Word.h"
#include "ThreadSpecificValue.h"

#if UNITY_IPHONE
	#include "PlatformDependent/iPhonePlayer/iPhoneNewLabelAllocator.h"
#endif

#include <map>

typedef DualThreadAllocator< DynamicHeapAllocator< LowLevelAllocator > > MainThreadAllocator;
typedef TLSAllocator< StackAllocator > TempTLSAllocator;

static MemoryManager* g_MemoryManager = NULL;

#if UNITY_FLASH
	extern "C" void NativeExt_FreeMemManager(void* p){
		GetMemoryManager().Deallocate (p, kMemNewDelete);
	}
#endif

// new override does not work on mac together with pace
#if !((UNITY_OSX || UNITY_LINUX) && UNITY_EDITOR) && !(UNITY_WEBGL) && !(UNITY_BB10) && !(UNITY_TIZEN)
void* operator new (size_t size) THROWING_NEW_THROW { return GetMemoryManager().Allocate (size==0?4:size, kDefaultMemoryAlignment, kMemNewDelete, kAllocateOptionNone, "Overloaded New"); }
void* operator new [] (size_t size) THROWING_NEW_THROW { return GetMemoryManager().Allocate (size==0?4:size, kDefaultMemoryAlignment, kMemNewDelete, kAllocateOptionNone, "Overloaded New[]"); }
void operator delete (void* p) throw() { GetMemoryManager().Deallocate (p, kMemNewDelete); }
void operator delete [] (void* p) throw() { GetMemoryManager().Deallocate (p, kMemNewDelete); }

void* operator new (size_t size, const std::nothrow_t&) throw() { return GetMemoryManager().Allocate (size, kDefaultMemoryAlignment, kMemNewDelete, kAllocateOptionNone, "Overloaded New"); }
void* operator new [] (size_t size, const std::nothrow_t&) throw() { return GetMemoryManager().Allocate (size, kDefaultMemoryAlignment, kMemNewDelete, kAllocateOptionNone, "Overloaded New[]"); };
void operator delete (void* p, const std::nothrow_t&) throw() { GetMemoryManager().Deallocate (p, kMemNewDelete); }
void operator delete [] (void* p, const std::nothrow_t&) throw() { GetMemoryManager().Deallocate (p, kMemNewDelete); }
#endif

#if UNITY_EDITOR
static size_t kDynamicHeapChunkSize = 16*1024*1024;
static size_t kTempAllocatorMainSize = 4*1024*1024;
static size_t kTempAllocatorThreadSize = 64*1024;
#elif UNITY_IPHONE || UNITY_ANDROID || UNITY_WII || UNITY_XENON || UNITY_PS3 || UNITY_FLASH || UNITY_WEBGL || UNITY_BB10 || UNITY_WP8 || UNITY_TIZEN
#	if !UNITY_IPHONE && !UNITY_WP8
static size_t kDynamicHeapChunkSize = 1*1024*1024;
#	endif
static size_t kTempAllocatorMainSize = 128*1024;
static size_t kTempAllocatorThreadSize = 64*1024;
#else
// Win/osx/linux players
static size_t kDynamicHeapChunkSize = 4*1024*1024;
static size_t kTempAllocatorMainSize = 512*1024;
static size_t kTempAllocatorThreadSize = 64*1024;
#endif

#if ENABLE_MEMORY_MANAGER


void PrintShortMemoryStats(TEMP_STRING& str, MemLabelRef label);

static int AlignUp(int value, int alignment)
{
	UInt32 ptr = value;
	UInt32 bitMask = (alignment - 1);
	UInt32 lowBits = ptr & bitMask;
	UInt32 adjust = ((alignment - lowBits) & bitMask);
	return adjust;
}

void* GetPreallocatedMemory(int size)
{
	const size_t numAllocators = 200; // should be configured per platform
	const size_t additionalStaticMem = sizeof(UnityDefaultAllocator<void>) * numAllocators;

	// preallocated memory for memorymanager and allocators
	static const int preallocatedSize = sizeof(MemoryManager) + additionalStaticMem;
#if UNITY_PS3
	static char __attribute__((aligned(16))) g_MemoryBlockForMemoryManager[preallocatedSize];
#elif UNITY_XENON
	static char __declspec(align(16)) g_MemoryBlockForMemoryManager[preallocatedSize];
#else
	static char g_MemoryBlockForMemoryManager[preallocatedSize];
#endif
	static char* g_MemoryBlockPtr = g_MemoryBlockForMemoryManager;

	size += AlignUp(size, 16);

	void* ptr = g_MemoryBlockPtr;
	g_MemoryBlockPtr+=size;
	// Ensure that there is enough space on the preallocated block
	if(g_MemoryBlockPtr > g_MemoryBlockForMemoryManager + preallocatedSize)
		return NULL;
	return ptr;
}
#endif

#if UNITY_WIN && ENABLE_MEM_PROFILER
#define _CRTBLD
#include <..\crt\src\dbgint.h>
_CRT_ALLOC_HOOK pfnOldCrtAllocHook;
int catchMemoryAllocHook(int allocType, void *userData, size_t size, int blockType, long requestNumber, const unsigned char	*filename, int lineNumber);
#endif



#if (UNITY_OSX && UNITY_EDITOR)

#include <malloc/malloc.h>
#include <mach/vm_map.h>
void *(*systemMalloc)(malloc_zone_t *zone, size_t size);
void *(*systemCalloc)(malloc_zone_t *zone, size_t num_items, size_t size);
void *(*systemValloc)(malloc_zone_t *zone, size_t size);
void *(*systemRealloc)(malloc_zone_t *zone, void* ptr, size_t size);
void *(*systemMemalign)(malloc_zone_t *zone, size_t align, size_t size);
void  (*systemFree)(malloc_zone_t *zone, void *ptr);
void  (*systemFreeSize)(malloc_zone_t *zone, void *ptr, size_t size);

void* my_malloc(malloc_zone_t *zone, size_t size)
{
	void* ptr = (*systemMalloc)(zone,size);
	MemoryManager::m_LowLevelAllocated+=(*malloc_default_zone()->size)(zone, ptr);
	return ptr;

}

void* my_calloc(malloc_zone_t *zone, size_t num_items, size_t size)
{
	void* ptr = (*systemCalloc)(zone,num_items,size);
	MemoryManager::m_LowLevelAllocated+=(*malloc_default_zone()->size)(zone, ptr);
	return ptr;

}

void* my_valloc(malloc_zone_t *zone, size_t size)
{
	void* ptr = (*systemValloc)(zone,size);
	MemoryManager::m_LowLevelAllocated+=(*malloc_default_zone()->size)(zone, ptr);
	return ptr;
}

void* my_realloc(malloc_zone_t *zone, void* ptr, size_t size)
{
	MemoryManager::m_LowLevelAllocated-=(*malloc_default_zone()->size)(zone, ptr);
	void* newptr = (*systemRealloc)(zone,ptr,size);
	MemoryManager::m_LowLevelAllocated+=(*malloc_default_zone()->size)(zone, newptr);
	return newptr;
}

void* my_memalign(malloc_zone_t *zone, size_t align, size_t size)
{
	void* ptr = (*systemMemalign)(zone,align,size);
	MemoryManager::m_LowLevelAllocated+=(*malloc_default_zone()->size)(zone, ptr);
	return ptr;
}

void my_free(malloc_zone_t *zone, void *ptr)
{
	int oldsize = (*malloc_default_zone()->size)(zone,ptr);
	MemoryManager::m_LowLevelAllocated-=oldsize;
	systemFree(zone,ptr);
}

void my_free_definite_size(malloc_zone_t *zone, void *ptr, size_t size)
{
	MemoryManager::m_LowLevelAllocated-=(*malloc_default_zone()->size)(zone,ptr);
	systemFreeSize(zone,ptr,size);
}

#endif
inline void* CheckAllocation(void* ptr, size_t size, int align, MemLabelRef label, const char* file, int line)
{
	if(ptr == NULL)
		//OutOfMemoryError(size, align, label, line, file);
	return ptr;
}
void InitializeMemory()
{
	InitializeMemoryLabels();

#if (UNITY_OSX && UNITY_EDITOR)

	UInt32 osxversion = 0;
    Gestalt(gestaltSystemVersion, (MacSInt32 *) &osxversion);

	// overriding malloc_zone on osx 10.5 causes unity not to start up.
	if(osxversion >= 0x01060)
	{
		malloc_zone_t* dz = malloc_default_zone();

		systemMalloc = dz->malloc;
		systemCalloc = dz->calloc;
		systemValloc = dz->valloc;
		systemRealloc = dz->realloc;
		systemMemalign = dz->memalign;
		systemFree = dz->free;
		systemFreeSize = dz->free_definite_size;

		if(dz->version>=8)
			vm_protect(mach_task_self(), (uintptr_t)dz, sizeof(malloc_zone_t), 0, VM_PROT_READ | VM_PROT_WRITE);//remove the write protection

		dz->malloc=&my_malloc;
		dz->calloc=&my_calloc;
		dz->valloc=&my_valloc;
		dz->realloc=&my_realloc;
		dz->memalign=&my_memalign;
		dz->free=&my_free;
		dz->free_definite_size=&my_free_definite_size;

		if(dz->version>=8)
			vm_protect(mach_task_self(), (uintptr_t)dz, sizeof(malloc_zone_t), 0, VM_PROT_READ);//put the write protection back

	}
#endif
}
MemoryManager& GetMemoryManager()
{
	if (g_MemoryManager == NULL){
		InitializeMemory();
		g_MemoryManager = HEAP_NEW(MemoryManager)();
	}
	return *g_MemoryManager ;
}
volatile long MemoryManager::m_LowLevelAllocated = 0;
volatile long MemoryManager::m_RegisteredGfxDriverMemory = 0;
void* malloc_internal(size_t size, int align, MemLabelRef label, int allocateOptions, const char* file, int line)
{
	return GetMemoryManager ().Allocate(size, align, label, allocateOptions, file, line);
}
#if	ENABLE_MEMORY_MANAGER

#if _DEBUG || UNITY_EDITOR
UNITY_TLS_VALUE(bool) s_DisallowAllocationsOnThread;
inline void MemoryManager::CheckDisalowAllocation()
{
	DebugAssert(IsActive());

	// Some codepaths in Unity disallow allocations. For example when a GC is running all threads are stopped.
	// If we allowed allocations we would allow for very hard to find race conditions.
	// Thus we explicitly check for it in debug builds.
	if (s_DisallowAllocationsOnThread)
	{
		s_DisallowAllocationsOnThread = false;
		FatalErrorMsg("CheckDisalowAllocation. Allocating memory when it is not allowed to allocate memory.\n");
	}
}

#else
inline void MemoryManager::CheckDisalowAllocation()
{
}
#endif

MemoryManager::MemoryManager()
: m_NumAllocators(0)
, m_FrameTempAllocator(NULL)
, m_IsInitialized(false)
, m_IsActive(true)
{
#if UNITY_WIN && ENABLE_MEM_PROFILER
//	pfnOldCrtAllocHook	= _CrtSetAllocHook(catchMemoryAllocHook);
#endif

	memset (m_Allocators, 0, sizeof(m_Allocators));
	memset (m_MainAllocators, 0, sizeof(m_MainAllocators));
	memset (m_ThreadAllocators, 0, sizeof(m_ThreadAllocators));
	memset (m_AllocatorMap, 0, sizeof(m_AllocatorMap));

	// Main thread will not have a valid TLSAlloc until ThreadInitialize() is called!
#if UNITY_FLASH || UNITY_WEBGL
	m_InitialFallbackAllocator = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>) ("ALLOC_FALLBACK");
#else
	m_InitialFallbackAllocator = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (1024*1024, 0, true,"ALLOC_FALLBACK");
#endif

	for (int i = 0; i < kMemLabelCount; i++)
		m_AllocatorMap[i].alloc = m_InitialFallbackAllocator;
}

void MemoryManager::StaticInitialize()
{
	GetMemoryManager().ThreadInitialize();
}

void MemoryManager::StaticDestroy()
{
#if !UNITY_OSX
    // not able to destroy profiler and memorymanager on osx because apple.coreaudio is still running a thread.
    // FMOD is looking into shutting this down properly. Untill then, don't cleanup
#if ENABLE_MEM_PROFILER
	MemoryProfiler::StaticDestroy();
#endif
	GetMemoryManager().ThreadCleanup();
#endif
}

void MemoryManager::InitializeMainThreadAllocators()
{
	m_FrameTempAllocator = HEAP_NEW(TempTLSAllocator)("ALLOC_TEMP_THREAD");

#if (UNITY_WIN && !UNITY_WP8) || UNITY_OSX
	BaseAllocator* defaultThreadAllocator = NULL;
	m_MainAllocators[m_NumAllocators] = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (kDynamicHeapChunkSize, 1024, false,"ALLOC_DEFAULT_MAIN");
	m_ThreadAllocators[m_NumAllocators] = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (1024*1024,1024, true,"ALLOC_DEFAULT_THREAD");
	BaseAllocator* defaultAllocator = m_Allocators[m_NumAllocators] = HEAP_NEW(MainThreadAllocator)("ALLOC_DEFAULT", m_MainAllocators[m_NumAllocators], m_ThreadAllocators[m_NumAllocators]);
	defaultThreadAllocator = m_ThreadAllocators[m_NumAllocators];
	m_NumAllocators++;
#else
	BaseAllocator* defaultAllocator = m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_DEFAULT");
#endif

	for (int i = 0; i < kMemLabelCount; i++)
		m_AllocatorMap[i].alloc = defaultAllocator;

	m_AllocatorMap[kMemTempAllocId].alloc = m_FrameTempAllocator;
    m_AllocatorMap[kMemStaticStringId].alloc = m_InitialFallbackAllocator;

#if UNITY_IPHONE
	m_AllocatorMap[kMemNewDeleteId].alloc = m_Allocators[m_NumAllocators++] = HEAP_NEW(IphoneNewLabelAllocator);
#endif

#if (UNITY_WIN && !UNITY_WP8) || UNITY_OSX
	m_MainAllocators[m_NumAllocators]   = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (kDynamicHeapChunkSize,0, false,"ALLOC_GFX_MAIN");
	m_ThreadAllocators[m_NumAllocators] = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (1024*1024,0, true,"ALLOC_GFX_THREAD");
	BaseAllocator* gfxAllocator         = m_Allocators[m_NumAllocators] = HEAP_NEW(MainThreadAllocator)("ALLOC_GFX", m_MainAllocators[m_NumAllocators], m_ThreadAllocators[m_NumAllocators]);
	BaseAllocator* gfxThreadAllocator   = m_ThreadAllocators[m_NumAllocators];
	m_NumAllocators++;

	m_MainAllocators[m_NumAllocators]   = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (8*1024*1024,0, false,"ALLOC_CACHEOBJECTS_MAIN");
	m_ThreadAllocators[m_NumAllocators] = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (2*1024*1024,0, true,"ALLOC_CACHEOBJECTS_THREAD");
	BaseAllocator* cacheAllocator         = m_Allocators[m_NumAllocators] = HEAP_NEW(MainThreadAllocator)("ALLOC_CACHEOBJECTS", m_MainAllocators[m_NumAllocators], m_ThreadAllocators[m_NumAllocators]);
	m_NumAllocators++;

	m_MainAllocators[m_NumAllocators]   = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (kDynamicHeapChunkSize,0, false,"ALLOC_TYPETREE_MAIN");
	m_ThreadAllocators[m_NumAllocators] = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (1024*1024,0, true,"ALLOC_TYPETREE_THREAD");
	BaseAllocator* typetreeAllocator    = m_Allocators[m_NumAllocators] = HEAP_NEW(MainThreadAllocator)("ALLOC_TYPETREE", m_MainAllocators[m_NumAllocators], m_ThreadAllocators[m_NumAllocators]);
	m_NumAllocators++;

	m_MainAllocators[m_NumAllocators]   = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (4*1024*1024,0, false,"ALLOC_PROFILER_MAIN");
	m_ThreadAllocators[m_NumAllocators] = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>) (4*1024*1024,0, true,"ALLOC_PROFILER_THREAD");
	BaseAllocator* profilerAllocator    = m_Allocators[m_NumAllocators] = HEAP_NEW(MainThreadAllocator)("ALLOC_PROFILER", m_MainAllocators[m_NumAllocators], m_ThreadAllocators[m_NumAllocators]);
	m_NumAllocators++;

	m_AllocatorMap[kMemDynamicGeometryId].alloc
		= m_AllocatorMap[kMemImmediateGeometryId].alloc
		= m_AllocatorMap[kMemGeometryId].alloc
		= m_AllocatorMap[kMemVertexDataId].alloc
		= m_AllocatorMap[kMemBatchedGeometryId].alloc
		= m_AllocatorMap[kMemTextureId].alloc = gfxAllocator;

	m_AllocatorMap[kMemTypeTreeId].alloc = typetreeAllocator;

	m_AllocatorMap[kMemThreadId].alloc = defaultThreadAllocator;
	m_AllocatorMap[kMemGfxThreadId].alloc = gfxThreadAllocator;

	m_AllocatorMap[kMemTextureCacheId].alloc = cacheAllocator;
	m_AllocatorMap[kMemSerializationId].alloc = cacheAllocator;
	m_AllocatorMap[kMemFileId].alloc = cacheAllocator;

	m_AllocatorMap[kMemProfilerId].alloc = profilerAllocator;
	m_AllocatorMap[kMemMemoryProfilerId].alloc = profilerAllocator;
	m_AllocatorMap[kMemMemoryProfilerStringId].alloc = profilerAllocator;

#elif UNITY_XENON
#if 1
	// DynamicHeapAllocator uses TLSF pools which are O(1) constant time for alloc/free
	// It should be used for high alloc/dealloc traffic
	BaseAllocator* dynAllocator = m_Allocators[m_NumAllocators++] = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocator>)(32*1024*1024, 0, true, "ALLOC_TINYBLOCKS");
	m_AllocatorMap[kMemBaseObjectId].alloc = dynAllocator;
	m_AllocatorMap[kMemAnimationId].alloc = dynAllocator;
	m_AllocatorMap[kMemSTLId].alloc = dynAllocator;
	m_AllocatorMap[kMemNewDeleteId].alloc = dynAllocator;
#else
	BaseAllocator* gameObjectAllocator = m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_GAMEOBJECT");
	BaseAllocator* gfxAllocator    = m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_GFX");

	BaseAllocator* profilerAllocator;
#if XBOX_USE_DEBUG_MEMORY
	if (xenon::GetIsDebugMemoryEnabled())
		profilerAllocator = m_Allocators[m_NumAllocators++] = HEAP_NEW(DynamicHeapAllocator<LowLevelAllocatorDebugMem>)(16*1024*1024, 0, true, "ALLOC_PROFILER");
	else
#endif
		profilerAllocator = m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_PROFILER");

	m_AllocatorMap[kMemDynamicGeometryId].alloc
	= m_AllocatorMap[kMemImmediateGeometryId].alloc
	= m_AllocatorMap[kMemGeometryId].alloc
	= m_AllocatorMap[kMemVertexDataId].alloc
	= m_AllocatorMap[kMemBatchedGeometryId].alloc
	= m_AllocatorMap[kMemTextureId].alloc = gfxAllocator;

	m_AllocatorMap[kMemBaseObjectId].alloc = gameObjectAllocator;

	m_AllocatorMap[kMemProfilerId].alloc = profilerAllocator;
	m_AllocatorMap[kMemMemoryProfilerId].alloc = profilerAllocator;
	m_AllocatorMap[kMemMemoryProfilerStringId].alloc = profilerAllocator;
#endif

#elif UNITY_PS3

	BaseAllocator* gameObjectAllocator	= m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_GAMEOBJECT");
	BaseAllocator* gfxAllocator			= m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_GFX");
	BaseAllocator* profilerAllocator    = m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_PROFILER");
	BaseAllocator* vertexDataAllocator	= m_Allocators[m_NumAllocators++] = HEAP_NEW(PS3DelayedReleaseAllocator("PS3_DELAYED_RELEASE_ALLOCATOR_PROXY", HEAP_NEW(PS3DlmallocAllocator)("PS3_DLMALLOC_ALLOCATOR")));
	BaseAllocator* delayedReleaseAllocator	= m_Allocators[m_NumAllocators++] = HEAP_NEW(PS3DelayedReleaseAllocator("PS3_DELAYED_RELEASE_ALLOCATOR_PROXY", HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("PS3_DELAYED_RELEASE_ALLOCATOR")));
	BaseAllocator* ioMappedAllocator	= m_Allocators[m_NumAllocators++] = HEAP_NEW(PS3DlmallocAllocator("PS3_IOMAPPED_ALLOCATOR"));

	m_AllocatorMap[kMemDynamicGeometryId].alloc
	= m_AllocatorMap[kMemImmediateGeometryId].alloc
	= m_AllocatorMap[kMemGeometryId].alloc
	= m_AllocatorMap[kMemVertexDataId].alloc
	= m_AllocatorMap[kMemBatchedGeometryId].alloc
	= m_AllocatorMap[kMemPS3RingBuffers.label].alloc
	= m_AllocatorMap[kMemPS3RSXBuffers.label].alloc
	= m_AllocatorMap[kMemTextureId].alloc = ioMappedAllocator;

	m_AllocatorMap[kMemBaseObjectId].alloc = gameObjectAllocator;
	m_AllocatorMap[kMemProfilerId].alloc =
	m_AllocatorMap[kMemMemoryProfilerId].alloc =
	m_AllocatorMap[kMemMemoryProfilerStringId].alloc = profilerAllocator;

	m_AllocatorMap[kMemSkinningId].alloc =
//	m_AllocatorMap[kMemSkinningTempId].alloc =
	m_AllocatorMap[kMemPS3DelayedReleaseId].alloc = delayedReleaseAllocator;

	m_AllocatorMap[kMemVertexDataId].alloc = vertexDataAllocator;

#else

	BaseAllocator* gameObjectAllocator = m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_GAMEOBJECT");
	BaseAllocator* gfxAllocator    = m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_GFX");
	BaseAllocator* profilerAllocator    = m_Allocators[m_NumAllocators++] = HEAP_NEW(UnityDefaultAllocator<LowLevelAllocator>)("ALLOC_PROFILER");

	m_AllocatorMap[kMemDynamicGeometryId].alloc
	= m_AllocatorMap[kMemImmediateGeometryId].alloc
	= m_AllocatorMap[kMemGeometryId].alloc
	= m_AllocatorMap[kMemVertexDataId].alloc
	= m_AllocatorMap[kMemBatchedGeometryId].alloc
	= m_AllocatorMap[kMemTextureId].alloc = gfxAllocator;

	m_AllocatorMap[kMemBaseObjectId].alloc = gameObjectAllocator;

	m_AllocatorMap[kMemProfilerId].alloc
	= m_AllocatorMap[kMemMemoryProfilerId].alloc
	= m_AllocatorMap[kMemMemoryProfilerStringId].alloc = profilerAllocator;

#endif

	m_IsInitialized = true;
	m_IsActive = true;

#if ENABLE_MEM_PROFILER
	MemoryProfiler::StaticInitialize();
#endif

	Assert(m_FrameTempAllocator);
}

MemoryManager::~MemoryManager()
{
	for(int i = 0; i < m_NumAllocators; i++)
	{
		Assert(m_Allocators[i]->GetAllocatedMemorySize() == 0);
	}

	ThreadCleanup();

	for (int i = 0; i < m_NumAllocators; i++){
		HEAP_DELETE (m_Allocators[i], BaseAllocator);
	}
#if UNITY_WIN
#if ENABLE_MEM_PROFILER
//	_CrtSetAllocHook(pfnOldCrtAllocHook);
#endif
#endif
}

class MemoryManagerAutoDestructor
{
public:
	MemoryManagerAutoDestructor(){}
	~MemoryManagerAutoDestructor()
	{
		//HEAP_DELETE(g_MemoryManager, MemoryManager);
	}
};

MemoryManagerAutoDestructor g_MemoryManagerAutoDestructor;


#else


MemoryManager::MemoryManager()
: m_NumAllocators(0)
, m_FrameTempAllocator(NULL)
{
}

MemoryManager::~MemoryManager()
{
}

#endif

#if UNITY_WIN
#include <winnt.h>
#define ON_WIN(x) x
#else
#define ON_WIN(x)
#endif

#if ENABLE_MEMORY_MANAGER

void* MemoryManager::LowLevelAllocate(size_t size)
{
	ON_WIN( InterlockedExchangeAdd(&m_LowLevelAllocated, size) );
	int* ptr = (int*)UNITY_LL_ALLOC(kMemDefault, size + kMemoryManagerOverhead, kDefaultMemoryAlignment);
	if(ptr != NULL)
	{
		*ptr = size;
		ptr += (kMemoryManagerOverhead >> 2);
	}

	return ptr;
}

void* MemoryManager::LowLevelCAllocate(size_t count, size_t size)
{
	ON_WIN( InterlockedExchangeAdd(&m_LowLevelAllocated, count*size) );
	int allocSize = count*size + kMemoryManagerOverhead;
	int* ptr = (int*)UNITY_LL_ALLOC(kMemDefault, allocSize, kDefaultMemoryAlignment);
	if(ptr != NULL)
	{
		memset(ptr, 0, allocSize);
		*ptr = count*size;
		ptr += (kMemoryManagerOverhead >> 2);
	}
	return ptr;
}

void* MemoryManager::LowLevelReallocate( void* p, size_t size )
{
	int* ptr = (int*) p;
	ptr -= kMemoryManagerOverhead >> 2;
	ON_WIN( InterlockedExchangeAdd(&m_LowLevelAllocated, -*ptr) );
	int* newptr = (int*)UNITY_LL_REALLOC(kMemDefault, ptr, size + kMemoryManagerOverhead, kDefaultMemoryAlignment);
	if(newptr != NULL)
	{
		*newptr = size;
		newptr += (kMemoryManagerOverhead >> 2);
	}
	ON_WIN( InterlockedExchangeAdd(&m_LowLevelAllocated, size) );
	return newptr;
}

void  MemoryManager::LowLevelFree(void* p){
	if(p == NULL)
		return;
	int* ptr = (int*) p;
	ptr -= kMemoryManagerOverhead >> 2;
	ON_WIN( InterlockedExchangeAdd(&m_LowLevelAllocated, -*ptr) );
	UNITY_LL_FREE(kMemDefault,ptr);
}

void MemoryManager::ThreadInitialize(size_t tempSize)
{
	int tempAllocatorSize = kTempAllocatorThreadSize;
	if(Thread::CurrentThreadIsMainThread() && !m_IsInitialized)
	{
		InitializeMainThreadAllocators();
		tempAllocatorSize = kTempAllocatorMainSize;
	}

	if(tempSize != 0)
		tempAllocatorSize = tempSize;

	StackAllocator* tempAllocator = UNITY_NEW(StackAllocator(tempAllocatorSize, "ALLOC_TEMP_THREAD"), kMemManager);
	m_FrameTempAllocator->ThreadInitialize(tempAllocator);
}





void* calloc_internal(size_t count, size_t size, int align, MemLabelRef label, int allocateOptions, const char* file, int line)
{
	void* ptr = GetMemoryManager ().Allocate(size*count, align, label, allocateOptions, file, line);
	if (ptr) memset (ptr, 0, size*count);
	return ptr;
}

void* realloc_internal(void* ptr, size_t size, int align, MemLabelRef label, int allocateOptions, const char* file, int line)
{
	return GetMemoryManager ().Reallocate(ptr, size, align, label, allocateOptions, file, line);
}

void free_internal(void* ptr)
{
	// used for mac, since malloc is not hooked from the start, so a number of mallocs will have passed through
	// before we wrap. This results in pointers being freed, that were not allocated with the memorymanager
	// therefore we need the alloc->Contains check()
	GetMemoryManager().Deallocate (ptr);
}

void free_alloc_internal(void* ptr, MemLabelRef label)
{
	GetMemoryManager().Deallocate (ptr, label);
}

void* MemoryManager::Allocate(size_t size, int align, MemLabelRef label, int allocateOptions/* = kNone*/, const char* file /* = NULL */, int line /* = 0 */)
{
	DebugAssert(IsPowerOfTwo(align));
	DebugAssert(align != 0);

	align = ((align-1) | (kDefaultMemoryAlignment-1)) + 1; // Max(align, kDefaultMemoryAlignment)

	// Fallback to backup allocator if we have not yet initialized the MemoryManager
	if(!IsActive())
	{
		void* ptr = m_InitialFallbackAllocator->Allocate(size, align);
#if ENABLE_MEM_PROFILER
		if (ptr)
			MemoryProfiler::InitAllocation(ptr, m_InitialFallbackAllocator);
#endif
		return ptr;
	}

	if (IsTempAllocatorLabel(label))
	{
		void* ptr = ((TempTLSAllocator*)m_FrameTempAllocator)->TempTLSAllocator::Allocate(size, align);
		if(ptr)
			return ptr;
		// if tempallocator thread has not been initialized fallback to defualt
		return Allocate(size, align, kMemDefault, allocateOptions, file, line);
	}

	BaseAllocator* alloc = GetAllocator(label);
	CheckDisalowAllocation();

	void* ptr = alloc->Allocate(size, align);

	if ((allocateOptions & kAllocateOptionReturnNullIfOutOfMemory) && !ptr)
		return NULL;

	CheckAllocation( ptr, size, align, label, file, line );

#if ENABLE_MEM_PROFILER
	RegisterAllocation(ptr, size, alloc, label, "Allocate", file, line);
#endif

	DebugAssert(((int)ptr & (align-1)) == 0);

#if STOMP_MEMORY
	memset(ptr, 0xcd, size);
#endif

	return ptr;

}

void* MemoryManager::TestFunc(size_t size, int align, testlabelid label)
{
	return NULL;
}

void* MemoryManager::TestFunc1(size_t size, int align, MemLabelId label)
{
	return NULL;
}
void MemoryManager::ThreadCleanup()
{
	for(int i = 0; i < m_NumAllocators; i++)
		m_Allocators[i]->ThreadCleanup();

	if(Thread::CurrentThreadIsMainThread())
	{
		m_FrameTempAllocator->ThreadCleanup();
		m_FrameTempAllocator = NULL;

		m_IsActive = false;

#if !UNITY_EDITOR
		for(int i = 0; i < m_NumAllocators; i++)
		{
			HEAP_DELETE(m_Allocators[i], BaseAllocator);
			if(m_MainAllocators[i])
				HEAP_DELETE(m_MainAllocators[i], BaseAllocator);
			if(m_ThreadAllocators[i])
				HEAP_DELETE(m_ThreadAllocators[i], BaseAllocator);
			m_Allocators[i] = 0;
			m_MainAllocators[i] = 0;
			m_ThreadAllocators[i] = 0;
		}
		m_NumAllocators = 0;
#else
		for(int i = 0; i < m_NumAllocators; i++)
		{
			if(m_MainAllocators[i])
				m_Allocators[i] = m_MainAllocators[i];
		}
#endif
		for (int i = 0; i < kMemLabelCount; i++)
			m_AllocatorMap[i].alloc = m_InitialFallbackAllocator;

		return;
	}
#if ENABLE_MEM_PROFILER
	GetMemoryProfiler()->ThreadCleanup();
#endif
	m_FrameTempAllocator->ThreadCleanup();
}

void MemoryManager::FrameMaintenance(bool cleanup)
{
	m_FrameTempAllocator->FrameMaintenance(cleanup);
	for(int i = 0; i < m_NumAllocators; i++)
		m_Allocators[i]->FrameMaintenance(cleanup);
}

#else

void* MemoryManager::LowLevelAllocate(int size)
{
	return UNITY_LL_ALLOC(kMemDefault,size+sizeof(int), 4);
}

void* MemoryManager::LowLevelCAllocate(int count, int size)
{
	int* ptr = (int*)UNITY_LL_ALLOC(kMemDefault,count*size, 4);
	memset(ptr,0,count*size);
	return ptr;
}

void* MemoryManager::LowLevelReallocate( void* p, int size )
{
	return UNITY_LL_REALLOC(kMemDefault,p, size,4);
}

void  MemoryManager::LowLevelFree(void* p){
	UNITY_LL_FREE(kMemDefault,p);
}

void MemoryManager::ThreadInitialize(size_t tempSize)
{}

//void MemoryManager::ThreadCleanup()
//{}

#endif

void OutOfMemoryError( size_t size, int align, MemLabelRef label, int line, const char* file )
{
	TEMP_STRING str;
	str.reserve(30*1024);
	str += FormatString<TEMP_STRING>("Could not allocate memory: System out of memory!\n");
	str += FormatString<TEMP_STRING>("Trying to allocate: %" PRINTF_SIZET_FORMAT "B with %d alignment. MemoryLabel: %s\n", size, align, GetMemoryManager().GetMemcatName(label));
	str += FormatString<TEMP_STRING>("Allocation happend at: Line:%d in %s\n", line, file);
	PrintShortMemoryStats(str, label);
	// first call the plain printf_console, to make a printout that doesn't do callstack and other allocations
	printf_console("%s", str.c_str());
	// Then do a FatalErroString, that brings up a dialog and launches the bugreporter.
	FatalErrorString(str.c_str());
}



void MemoryManager::DisallowAllocationsOnThisThread()
{
#if _DEBUG || UNITY_EDITOR
	s_DisallowAllocationsOnThread = true;
#endif
}
void MemoryManager::ReallowAllocationsOnThisThread()
{
#if _DEBUG || UNITY_EDITOR
	s_DisallowAllocationsOnThread = false;
#endif
}



void* MemoryManager::Reallocate(void* ptr, size_t size, int align, MemLabelRef label, int allocateOptions/* = kNone*/, const char* file /* = NULL */, int line /* = 0 */)
{
	DebugAssert(IsPowerOfTwo(align));
	DebugAssert(align != 0);

	if(ptr == NULL)
		return Allocate(size,align,label,allocateOptions,file,line);

	align = ((align-1) | (kDefaultMemoryAlignment-1)) + 1; // Max(align, kDefaultMemoryAlignment)

	// Fallback to backup allocator if we have not yet initialized the MemoryManager
	if(!IsActive())
		return m_InitialFallbackAllocator->Reallocate(ptr, size, align);

	if (IsTempAllocatorLabel(label))
	{
		void* newptr = ((TempTLSAllocator*)m_FrameTempAllocator)->TempTLSAllocator::Reallocate(ptr, size, align);
		if(newptr)
			return newptr;
		// if tempallocator thread has not been initialized fallback to defualt
		return Reallocate( ptr, size, align, kMemDefault, allocateOptions, file, line);
	}

	BaseAllocator* alloc = GetAllocator(label);
	CheckDisalowAllocation();

	if(ptr != NULL && !alloc->Contains(ptr))
	{
		// It wasn't the expected allocator that contained the pointer.
		// allocate on the expected allocator and move the memory there
		void* newptr = Allocate(size,align,label,allocateOptions,file,line);
		if ((allocateOptions & kAllocateOptionReturnNullIfOutOfMemory) && !newptr)
			return NULL;

		int oldSize = GetAllocatorContainingPtr(ptr)->GetPtrSize(ptr);
		memcpy(newptr, ptr, size<oldSize?size:oldSize);

		Deallocate(ptr);
		return newptr;
	}

	#if ENABLE_MEM_PROFILER
	// register the deletion of the old allocation and extract the old root owner into the label
	ProfilerAllocationHeader* root = RegisterDeallocation(ptr, alloc, label, "Reallocate");
	#endif

	void* newptr = alloc->Reallocate(ptr, size, align);

	if ((allocateOptions & kAllocateOptionReturnNullIfOutOfMemory) && !newptr)
		return NULL;

	CheckAllocation( newptr, size, align, label, file, line );

	#if ENABLE_MEM_PROFILER
	RegisterAllocation(newptr, size, alloc, MemLabelId(label.label, root), "Reallocate", file, line);
	#endif

	DebugAssert(((int)newptr & (align-1)) == 0);

	return newptr;
}

void MemoryManager::Deallocate(void* ptr, MemLabelRef label)
{
	if(ptr == NULL)
		return;

	if(!IsActive()) // if we are outside the scope of Initialize/Destroy - fallback
		return Deallocate(ptr);

	if (IsTempAllocatorLabel(label))
	{
		if ( ((TempTLSAllocator*)m_FrameTempAllocator)->TempTLSAllocator::TryDeallocate(ptr) )
			return;
		// If not found, Fallback to do a search for what allocator has the pointer
		return Deallocate(ptr);
	}

	BaseAllocator* alloc = GetAllocator(label);
	CheckDisalowAllocation();

	if(!alloc->Contains(ptr))
		return Deallocate(ptr);

#if ENABLE_MEM_PROFILER
	RegisterDeallocation(ptr, alloc, label, "Deallocate");
#endif

#if STOMP_MEMORY
	memset32(ptr, 0xdeadbeef, alloc->GetPtrSize(ptr));
#endif

	alloc->Deallocate(ptr);
}

void MemoryManager::Deallocate(void* ptr)
{
	if (ptr == NULL)
		return;

	BaseAllocator* alloc = GetAllocatorContainingPtr(ptr);

	if (alloc)
	{
		Assert (alloc != m_FrameTempAllocator);
#if ENABLE_MEM_PROFILER
		if(GetMemoryProfiler() && alloc != m_InitialFallbackAllocator)
		{
			size_t oldsize = alloc->GetPtrSize(ptr);
			GetMemoryProfiler()->UnregisterAllocation(ptr, alloc, oldsize, NULL, kMemDefault);
			//RegisterDeallocation(MemLabelId(labelid), oldsize); // since we don't have the label, we will not register this deallocation
			if(m_LogAllocations)
				printf_console("Deallocate (%8X): %11d\tTotal: %.2fMB (%d)\n", (unsigned int)ptr, -(int)oldsize, GetTotalAllocatedMemory() / (1024.0f * 1024.0f), (int)GetTotalAllocatedMemory());
		}
#endif
#if STOMP_MEMORY
		memset32(ptr, 0xdeadbeef, alloc->GetPtrSize(ptr));
#endif
		alloc->Deallocate(ptr);
	}
	else
	{
		if(IsActive())
			UNITY_LL_FREE (kMemDefault, ptr);
		//else ignore the deallocation, because the allocator is no longer around
	}
}

struct ExternalAllocInfo
{
	size_t size;
	size_t relatedID;
	const char* file;
	int line;
};
typedef UNITY_MAP(kMemMemoryProfiler,void*,ExternalAllocInfo ) ExternalAllocationMap;
static ExternalAllocationMap* g_ExternalAllocations = NULL;
Mutex g_ExternalAllocationLock;
void register_external_gfx_allocation(void* ptr, size_t size, size_t related, const char* file, int line)
{
	Mutex::AutoLock autolock(g_ExternalAllocationLock);
	if (g_ExternalAllocations == NULL)
	{
		SET_ALLOC_OWNER(NULL);
		g_ExternalAllocations = new ExternalAllocationMap();
	}
	ExternalAllocationMap::iterator it = g_ExternalAllocations->find(ptr);
	if (it != g_ExternalAllocations->end())
	{
		ErrorStringMsg("allocation 0x%p already registered @ %s:l%d size %d; now calling from %s:l%d size %d?",
			ptr,
			it->second.file, it->second.line, it->second.size,
			file, line, size);
	}

	if (related == 0)
		related = (size_t)ptr;

	ExternalAllocInfo info;
	info.size = size;
	info.relatedID = related;
	info.file = file;
	info.line = line;
	g_ExternalAllocations->insert(std::make_pair(ptr, info));
	MemoryManager::m_RegisteredGfxDriverMemory += size;
#if ENABLE_MEM_PROFILER
	GetMemoryProfiler()->RegisterMemoryToID(related, size);
#endif
}

void register_external_gfx_deallocation(void* ptr, const char* file, int line)
{
	if(ptr == NULL)
		return;
	Mutex::AutoLock autolock(g_ExternalAllocationLock);
	if (g_ExternalAllocations == NULL)
		g_ExternalAllocations = new ExternalAllocationMap();

	ExternalAllocationMap::iterator it = g_ExternalAllocations->find(ptr);
	if (it == g_ExternalAllocations->end())
	{
		// not registered
		return;
	}
	size_t size = it->second.size;
	size_t related = it->second.relatedID;
	MemoryManager::m_RegisteredGfxDriverMemory -= size;
	g_ExternalAllocations->erase(it);
#if ENABLE_MEM_PROFILER
	GetMemoryProfiler()->UnregisterMemoryToID(related,size);
#endif
}


typedef UNITY_MAP(kMemDefault,MemLabelIdentifier, BaseAllocator*) CustomAllocators;
CustomAllocators* g_CustomAllocators = NULL;
int nextCustomAllocatorIndex = (MemLabelIdentifier)0x1000;

BaseAllocator* MemoryManager::GetAllocatorContainingPtr(const void* ptr)
{
	if(m_FrameTempAllocator && m_FrameTempAllocator->Contains(ptr))
		return m_FrameTempAllocator;

	for(int i = 0; i < m_NumAllocators ; i++)
	{
		if(m_Allocators[i]->IsAssigned() && m_Allocators[i]->Contains(ptr))
			return m_Allocators[i];
	}

	if(m_InitialFallbackAllocator->Contains(ptr))
		return m_InitialFallbackAllocator;

	if(g_CustomAllocators)
	{
		CustomAllocators::iterator it = g_CustomAllocators->begin();
		for(;it != g_CustomAllocators->end(); ++it)
			if(it->second->Contains(ptr))
				return it->second;
	}
	return NULL;
}

const char* MemoryManager::GetAllocatorName( int i )
{
	return i < m_NumAllocators ? m_Allocators[i]->GetName() : "Custom";
}

const char* MemoryManager::GetMemcatName( MemLabelRef label )
{
	return label.label < kMemLabelCount ? MemLabelName[label.label] : "Custom";
}

MemLabelId MemoryManager::AddCustomAllocator(BaseAllocator* allocator)
{
	Assert(allocator->Contains(NULL) == false); // to assure that Contains does not just return true
	MemLabelIdentifier label =(MemLabelIdentifier)(nextCustomAllocatorIndex++);
	if(!g_CustomAllocators)
		g_CustomAllocators = UNITY_NEW(CustomAllocators,kMemDefault);
	(*g_CustomAllocators)[label] = allocator;
	return MemLabelId(label, NULL);
}

void MemoryManager::RemoveCustomAllocator(BaseAllocator* allocator)
{
	if(!g_CustomAllocators)
		return;

	for(CustomAllocators::iterator it = g_CustomAllocators->begin();
		it != g_CustomAllocators->end();
		++it)
	{
		if (it->second == allocator)
		{
			g_CustomAllocators->erase(it);
			break;
		}
	}
	if(g_CustomAllocators->empty())
		UNITY_DELETE(g_CustomAllocators, kMemDefault);
}

BaseAllocator* MemoryManager::GetAllocatorAtIndex( int index )
{
	return m_Allocators[index];
}

BaseAllocator* MemoryManager::GetAllocator( MemLabelRef label )
{
	DebugAssert(!IsTempAllocatorLabel(label));
	if(label.label < kMemLabelCount)
	{
		BaseAllocator* alloc = m_AllocatorMap[label.label].alloc;
		return alloc;
	}
	else if(label.label == kMemLabelCount)
		return NULL;

	if(g_CustomAllocators)
	{
		CustomAllocators::iterator it = g_CustomAllocators->find(label.label);
		if(it != g_CustomAllocators->end())
			return (*it).second;
	}

	return NULL;
}

int MemoryManager::GetAllocatorIndex( BaseAllocator* alloc )
{
	for(int i = 0; i < m_NumAllocators; i++)
		if(alloc == m_Allocators[i])
			return i;

	return m_NumAllocators;
}

size_t MemoryManager::GetTotalAllocatedMemory()
{
	size_t total = m_FrameTempAllocator->GetAllocatedMemorySize();
	for(int i = 0; i < m_NumAllocators ; i++)
		total += m_Allocators[i]->GetAllocatedMemorySize();
	return total;
}

size_t MemoryManager::GetTotalReservedMemory()
{
	size_t total = m_FrameTempAllocator->GetReservedSizeTotal();
	for(int i = 0; i < m_NumAllocators ; i++)
		total += m_Allocators[i]->GetReservedSizeTotal();
	return total;
}

size_t MemoryManager::GetTotalUnusedReservedMemory()
{
	return GetTotalReservedMemory() - GetTotalAllocatedMemory();;
}

int MemoryManager::GetAllocatorCount( )
{
	return m_NumAllocators;
}

size_t MemoryManager::GetAllocatedMemory( MemLabelRef label )
{
	return m_AllocatorMap[label.label].allocatedMemory;
}

size_t MemoryManager::GetTotalProfilerMemory()
{
	return  GetAllocator(kMemProfiler)->GetAllocatedMemorySize();
}

int MemoryManager::GetAllocCount( MemLabelRef label )
{
	return m_AllocatorMap[label.label].numAllocs;
}

size_t MemoryManager::GetLargestAlloc( MemLabelRef label )
{
	return m_AllocatorMap[label.label].largestAlloc;
}

void MemoryManager::StartLoggingAllocations(size_t logAllocationsThreshold)
{
	m_LogAllocations = true;
	m_LogAllocationsThreshold = logAllocationsThreshold;
};

void MemoryManager::StopLoggingAllocations()
{
	m_LogAllocations = false;
};

#if ENABLE_MEM_PROFILER
void MemoryManager::RegisterAllocation(void* ptr, size_t size, BaseAllocator* alloc, MemLabelRef label, const char* function, const char* file, int line)
{
	if (GetMemoryProfiler())
	{
		DebugAssert(!IsTempAllocatorLabel(label));
		if(label.label < kMemLabelCount)
		{
			m_AllocatorMap[label.label].allocatedMemory += size;
			m_AllocatorMap[label.label].numAllocs++;
			m_AllocatorMap[label.label].largestAlloc = std::max(m_AllocatorMap[label.label].largestAlloc, size);
		}
		GetMemoryProfiler()->RegisterAllocation(ptr, label, file, line, size);
		if (m_LogAllocations && size >= m_LogAllocationsThreshold)
		{
			size_t totalAllocatedMemoryAfterAllocation = GetTotalAllocatedMemory();
			printf_console( "%s (%p): %11" PRINTF_SIZET_FORMAT "\tTotal: %.2fMB (%" PRINTF_SIZET_FORMAT ") in %s:%d\n",
				function, ptr, size, totalAllocatedMemoryAfterAllocation / (1024.0f * 1024.0f), totalAllocatedMemoryAfterAllocation, file, line
				);
		}
	}
}

ProfilerAllocationHeader* MemoryManager::RegisterDeallocation(void* ptr, BaseAllocator* alloc, MemLabelRef label, const char* function)
{
	ProfilerAllocationHeader* relatedHeader = NULL;
	if (ptr != NULL && GetMemoryProfiler())
	{
		DebugAssert(!IsTempAllocatorLabel(label));
		size_t oldsize = alloc->GetPtrSize(ptr);
		GetMemoryProfiler()->UnregisterAllocation(ptr, alloc, oldsize, &relatedHeader, label);
		if(label.label < kMemLabelCount)
		{
			m_AllocatorMap[label.label].allocatedMemory -= oldsize;
			m_AllocatorMap[label.label].numAllocs--;
		}

		if (m_LogAllocations && oldsize >= m_LogAllocationsThreshold)
		{
			size_t totalAllocatedMemoryAfterDeallocation = GetTotalAllocatedMemory() - oldsize;
			printf_console( "%s (%p): %11" PRINTF_SIZET_FORMAT "\tTotal: %.2fMB (%" PRINTF_SIZET_FORMAT ")\n",
				function, ptr, -(int)oldsize, totalAllocatedMemoryAfterDeallocation / (1024.0f * 1024.0f), totalAllocatedMemoryAfterDeallocation);
		}
	}
	return relatedHeader;
}
#endif

void PrintShortMemoryStats(TEMP_STRING& str, MemLabelRef label)
{
#if ENABLE_MEMORY_MANAGER
	MemoryManager& mm = GetMemoryManager();
	str += "Memory overview\n\n";
	for(int i = 0; i < mm.GetAllocatorCount() ; i++)
	{
		BaseAllocator* alloc = mm.GetAllocatorAtIndex(i);
		if(alloc == NULL)
			continue;
		str += FormatString<TEMP_STRING>( "\n[ %s ] used: %" PRINTF_SIZET_FORMAT "B | peak: %" PRINTF_SIZET_FORMAT "B | reserved: %" PRINTF_SIZET_FORMAT "B \n",
									alloc->GetName(), alloc->GetAllocatedMemorySize(), alloc->GetPeakAllocatedMemorySize(), alloc->GetReservedSizeTotal());
	}
#endif
}

#if UNITY_WIN && ENABLE_MEM_PROFILER

// this is only here to catch stray mallocs - UNITY_MALLOC should be used instead
// use tls to mark if we are in our own malloc or not. register stray mallocs.
int catchMemoryAllocHook(int allocType, void *userData, size_t size, int blockType, long requestNumber, const unsigned char *filename, int lineNumber)
{
	if(!GetMemoryProfiler())
		return TRUE;

	if (allocType == _HOOK_FREE)
	{
#ifdef _DEBUG
		int headersize = sizeof(_CrtMemBlockHeader);
		_CrtMemBlockHeader* header = (_CrtMemBlockHeader*)((size_t) userData - headersize);
		GetMemoryProfiler()->UnregisterAllocation(NULL, NULL, header->nDataSize, NULL, kMemDefault);
#endif
	}
	else
		GetMemoryProfiler()->RegisterAllocation(NULL, MemLabelId(kMemLabelCount, NULL), NULL, 0, size);
	return TRUE;
}

#endif


#endif


