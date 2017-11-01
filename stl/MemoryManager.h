#ifndef _MEMORY_MANAGER_H_
#define _MEMORY_MANAGER_H_

#include "AllocatorLabels.h"
#include "BaseAllocator.h"
#include "FileStripped.h"

#if UNITY_XENON
#include "PlatformDependent/Xbox360/Source/XenonMemory.h"
#endif

#if ENABLE_MEMORY_MANAGER

class MemoryManager
{
public:
	MemoryManager();
	~MemoryManager();
	static void StaticInitialize();
	static void StaticDestroy();

	void ThreadInitialize(size_t tempSize = 0);
	void ThreadCleanup();

	void* TestFunc(size_t size, int align, testlabelid label);

	void* TestFunc1(size_t size, int align, MemLabelId label);

	bool IsInitialized() {return m_IsInitialized;}
	bool IsActive() {return m_IsActive;}
	void* Allocate(size_t size, int align, MemLabelRef label, int allocateOptions = kAllocateOptionNone, const char* file = NULL, int line = 0);
	void* Reallocate(void* ptr, size_t size, int align, MemLabelRef label, int allocateOptions = kAllocateOptionNone, const char* file = NULL, int line = 0);
	void  Deallocate(void* ptr);
	void  Deallocate(void* ptr, MemLabelRef label);

	BaseAllocator* GetAllocator(MemLabelRef label);
	int GetAllocatorIndex(BaseAllocator* alloc);
	BaseAllocator* GetAllocatorAtIndex( int index );

	MemLabelId AddCustomAllocator(BaseAllocator* allocator);
	void RemoveCustomAllocator(BaseAllocator* allocator);

	void FrameMaintenance(bool cleanup = false);

	static void* LowLevelAllocate( size_t size );
	static void* LowLevelCAllocate( size_t count, size_t size );
	static void* LowLevelReallocate( void* p, size_t size );
	static void  LowLevelFree( void* p );

	const char* GetAllocatorName( int i );
	const char* GetMemcatName( MemLabelRef label );

	size_t GetTotalAllocatedMemory();
	size_t GetTotalUnusedReservedMemory();
	size_t GetTotalReservedMemory();

	size_t GetTotalProfilerMemory();

	int GetAllocatorCount( );

	size_t GetAllocatedMemory( MemLabelRef label );
	int GetAllocCount( MemLabelRef label );
	size_t GetLargestAlloc(MemLabelRef label);

#if UNITY_XENON
	size_t GetRegisteredGFXDriverMemory(){ return xenon::GetGfxMemoryAllocated();}
#else
	size_t GetRegisteredGFXDriverMemory(){ return m_RegisteredGfxDriverMemory;}
#endif

	void StartLoggingAllocations(size_t logAllocationsThreshold = 0);
	void StopLoggingAllocations();

	void DisallowAllocationsOnThisThread();
	void ReallowAllocationsOnThisThread();
	void CheckDisalowAllocation();

	BaseAllocator* GetAllocatorContainingPtr(const void* ptr);

	static inline bool IsTempAllocatorLabel( MemLabelRef label ) { return label.label == kMemTempAllocId; }

	volatile static long m_LowLevelAllocated;
	volatile static long m_RegisteredGfxDriverMemory;

private:
#if ENABLE_MEM_PROFILER
	void RegisterAllocation(void* ptr, size_t size, BaseAllocator* alloc, MemLabelRef label, const char* function, const char* file, int line);
	ProfilerAllocationHeader* RegisterDeallocation(void* ptr, BaseAllocator* alloc, MemLabelRef label, const char* function);
#endif
	void InitializeMainThreadAllocators();

	static const int kMaxAllocators = 16;

	BaseAllocator*   m_FrameTempAllocator;
	BaseAllocator*   m_InitialFallbackAllocator;

	BaseAllocator*   m_Allocators[kMaxAllocators];
	BaseAllocator*   m_MainAllocators[kMaxAllocators];
	BaseAllocator*   m_ThreadAllocators[kMaxAllocators];

	int              m_NumAllocators;
	bool             m_LogAllocations;
	bool             m_IsInitialized;
	bool             m_IsActive;


	size_t           m_LogAllocationsThreshold;

	struct LabelInfo
	{
		BaseAllocator* alloc;
		size_t allocatedMemory;
		int numAllocs;
		size_t largestAlloc;
	};
	LabelInfo   m_AllocatorMap[kMemLabelCount];
};
MemoryManager& GetMemoryManager();

#endif
#endif
