#pragma once

#include "MemoryMacros.h"

#if ENABLE_MEM_PROFILER

#define RECORD_ALLOCATION_SITES 0
#define ENABLE_STACKS_ON_ALL_ALLOCS 0
#define MAINTAIN_RELATED_ALLOCATION_LIST 1

#if ENABLE_STACKS_ON_ALL_ALLOCS
#undef RECORD_ALLOCATION_SITES
#define RECORD_ALLOCATION_SITES 1
#endif


#include "Mutex.h"
#include "ThreadSpecificValue.h"
#include "AtomicRefCounter.h"
#include <map>
#include "MemoryPool.h"

struct MonoObject;
// header for all allocations

struct AllocationRootReference
{
	AllocationRootReference(ProfilerAllocationHeader* root): root(root) {}

	void Retain() { m_Counter.Retain(); }
	void Release() 
	{ 
		if(m_Counter.Release())
			delete_internal(this, kMemProfiler); 
	}

	AtomicRefCounter m_Counter;
	ProfilerAllocationHeader* root;
};


class MemoryProfiler
{
public:
	MemoryProfiler();
	~MemoryProfiler();

	static void StaticInitialize();
	static void StaticDestroy();

	void ThreadCleanup();

	static void InitAllocation(void* ptr, BaseAllocator* alloc);
	void RegisterAllocation(void* ptr, MemLabelRef label, const char* file, int line, size_t size = 0);
	void UnregisterAllocation(void* ptr, BaseAllocator* alloc, size_t size, ProfilerAllocationHeader** rootHeader, MemLabelRef label);

	void TransferOwnership(void* ptr, BaseAllocator* alloc, ProfilerAllocationHeader* newRootHeader);

	static int GetHeaderSize();

	ProfilerString GetOverview() const;

	ProfilerString GetUnrootedAllocationsOverview();
	ProfilerAllocationHeader* GetAllocationRootHeader(void* ptr, MemLabelRef label) const;

	// Roots allocations can be created using UNITY_NEW_AS_ROOT.
	// They automatically show up in the memory profiler as a seperate section.
	struct RootAllocationInfo
	{
		const char* areaName;
		const char* objectName;
		size_t      memorySize;
	};
	typedef dynamic_array<RootAllocationInfo> RootAllocationInfos;

	void RegisterRootAllocation   (void* root, BaseAllocator* allocator, const char* areaName, const char* objectName);
	void UnregisterRootAllocation (void* root);
	void GetRootAllocationInfos (RootAllocationInfos& infos);
	void SetRootAllocationObjectName (void* root, const char* objectName);

	static AllocationRootReference* GetRootReferenceFromHeader(ProfilerAllocationHeader* root);

	bool PushAllocationRoot(void* root, bool forcePush);
	void PopAllocationRoot();
	ProfilerAllocationHeader* GetCurrentRootHeader();

	void RegisterMemoryToID(size_t id, size_t size);
	void UnregisterMemoryToID( size_t id, size_t size );
	size_t GetRelatedIDMemorySize(size_t id);

	size_t GetRelatedMemorySize(const void* ptr);

	static bool IsRecording() {return m_RecordingAllocation;}


	struct MemoryStackEntry
	{
		MemoryStackEntry():totalMemory(0),ownedMemory(0){}
		MonoObject* Deserialize();
		int totalMemory;
		int ownedMemory;
		std::map<void*, MemoryStackEntry> callerSites;
		std::string name;
	};
	MemoryStackEntry* GetStackOverview() const;
	void ClearStackOverview(MemoryStackEntry* entry) const;

	static MemoryProfiler* s_MemoryProfiler;

	void AllocateStructs();
	struct AllocationSite;
private:

	static void SetupAllocationHeader(ProfilerAllocationHeader* header, ProfilerAllocationHeader* root, int size);
	void ValidateRoot(ProfilerAllocationHeader* root);

#if MAINTAIN_RELATED_ALLOCATION_LIST
	void UnlinkAllAllocations(ProfilerAllocationHeader* root);
	void InsertAfterRoot( ProfilerAllocationHeader* root, ProfilerAllocationHeader* header );
	void UnlinkHeader(ProfilerAllocationHeader* header);
#endif

	static UNITY_TLS_VALUE(bool) m_RecordingAllocation;

	Mutex                        m_Mutex;

	size_t m_SizeUsed;
	UInt32 m_NumAllocations;

	size_t m_AccSizeUsed;
	UInt64 m_AccNumAllocations;

	UInt32 m_SizeDistribution[32];

	size_t m_InternalMemoryUsage;

	typedef std::pair< size_t, size_t> ReferenceIDPair;
	typedef STL_ALLOCATOR( kMemMemoryProfiler, ReferenceIDPair ) ReferenceIDAllocator;
	typedef std::map< size_t, size_t, std::less<size_t>, ReferenceIDAllocator > ReferenceID;
	ReferenceID* m_ReferenceIDSizes;

	struct RootAllocationType
	{
		const char* areaName;
		ProfilerString objectName;
	};

	typedef std::pair< void*, RootAllocationType> RootAllocationTypePair;
	typedef STL_ALLOCATOR( kMemMemoryProfiler, RootAllocationTypePair ) AllocationRootTypeAllocator;

	typedef std::map< void*, RootAllocationType, std::less<void*>, AllocationRootTypeAllocator > RootAllocationTypes;
	RootAllocationTypes* m_RootAllocationTypes;

	static UNITY_TLS_VALUE(ProfilerAllocationHeader**) m_RootStack;
	static UNITY_TLS_VALUE(UInt32) m_RootStackSize;
	static UNITY_TLS_VALUE(ProfilerAllocationHeader**) m_CurrentRootHeader;

	ProfilerAllocationHeader* m_DefaultRootHeader;

#if RECORD_ALLOCATION_SITES
public:
	struct AllocationSite
	{
		MemLabelIdentifier label;
#if ENABLE_STACKS_ON_ALL_ALLOCS
		void* stack[20];
		UInt32 stackHash;
#endif
		const char* file;
		int line;
		int allocated;
		int alloccount;
		int ownedAllocated;
		int ownedCount;
		size_t cummulativeAllocated;
		size_t cummulativeAlloccount;

		bool operator()(const AllocationSite& s1, const AllocationSite& s2) const
		{
			return s1.line != s2.line ? s1.line < s2.line :
				s1.label != s2.label ? s1.label < s2.label:
#if ENABLE_STACKS_ON_ALL_ALLOCS
				s1.stackHash != s2.stackHash? s1.stackHash< s2.stackHash:
#endif
				s1.file < s2.file ;
		}

		struct Sorter
		{
			bool operator()( const AllocationSite* a, const AllocationSite* b ) const
			{
				return a->allocated > b->allocated;
			}
		};
	};
	typedef std::set<AllocationSite, AllocationSite, STL_ALLOCATOR(kMemMemoryProfiler, AllocationSite) > AllocationSites;
	AllocationSites*    m_AllocationSites;
private:
	struct LocalHeaderInfo
	{
		size_t size;
		const AllocationSite* site;
	};

	// map used for headers for allocations that don't support profileheaders
	typedef std::pair< void* const, LocalHeaderInfo> SiteHeaderPair;
	typedef STL_ALLOCATOR( kMemMemoryProfiler, SiteHeaderPair ) MapAllocator;
	typedef std::map< void*, LocalHeaderInfo, std::less<void*>, MapAllocator > AllocationSizes;
	AllocationSizes* m_AllocationSizes;
#endif

	struct AllocationSiteSizeSorter {
		bool operator()( const std::pair<const AllocationSite*,size_t>& a, const std::pair<const AllocationSite*,size_t>& b ) const
		{
			return a.second > b.second;
		}
	};
};

inline MemoryProfiler* GetMemoryProfiler()
{
	return MemoryProfiler::s_MemoryProfiler;
}

#else

class MemoryProfiler
{
public:
	static int GetHeaderSize() { return 0; }
	static void SetInLLAllocator (bool inLowLevelAllocator) { }
	size_t GetRelatedMemorySize(const void* ptr) { return 0;}
	static bool IsRecording() {return false;}
};

#endif

