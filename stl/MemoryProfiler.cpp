#include "UnityPrefix.h"

#include "MemoryProfiler.h"
#include "MemoryManager.h"

#if ENABLE_MEM_PROFILER

#include "MemoryMacros.h"
#include "BitUtility.h"

#if RECORD_ALLOCATION_SITES
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoUtility.h"
#endif

#if !UNITY_EDITOR && ENABLE_STACKS_ON_ALL_ALLOCS
#include "Runtime/Utilities/Stacktrace.cpp"
#endif

#include "AtomicOps.h"

#define ROOT_UNRELATED_ALLOCATIONS 0

MemoryProfiler* MemoryProfiler::s_MemoryProfiler = NULL;
UNITY_TLS_VALUE(ProfilerAllocationHeader**) MemoryProfiler::m_RootStack;
UNITY_TLS_VALUE(UInt32) MemoryProfiler::m_RootStackSize;
UNITY_TLS_VALUE(ProfilerAllocationHeader**) MemoryProfiler::m_CurrentRootHeader;
UNITY_TLS_VALUE(bool) MemoryProfiler::m_RecordingAllocation;

struct ProfilerAllocationHeader
{
	enum RootStatusMask
	{
		kIsRootAlloc = 0x1,
		kRegisteredRoot = 0x2
	};

#if MAINTAIN_RELATED_ALLOCATION_LIST
	// pointer to next allocation in root chain. Root is first element in this list
	ProfilerAllocationHeader* next; 
	union
	{
		// pointer to prev allocation in root chain. Needed for removal
		ProfilerAllocationHeader* prev;
		// for Roots, the rootReference points to the AllocationRootReference,
		AllocationRootReference* rootReference;
	};

#endif

	volatile int accumulatedSize; // accumulated size of every allocation that relates to this one - just size if child alloc
	ProfilerAllocationHeader* GetRootPtr() { return (ProfilerAllocationHeader*)((size_t)relatesTo&~0x3); }
	void SetRootPtr (ProfilerAllocationHeader* ptr) { relatesTo = ptr; }
	bool IsRoot () { return ((size_t)relatesTo&kIsRootAlloc)!=0; }
	bool IsRegisteredRoot () { return ((size_t)relatesTo&kRegisteredRoot)!=0; }

	void SetAsRegisteredRoot () { relatesTo = (ProfilerAllocationHeader*)(kIsRootAlloc|kRegisteredRoot); }

	void SetAsRoot () { relatesTo = (ProfilerAllocationHeader*)(kIsRootAlloc); }

	ProfilerAllocationHeader* relatesTo;

#if RECORD_ALLOCATION_SITES
	const MemoryProfiler::AllocationSite* site; // Site of where the allocation was done
#endif
};


void MemoryProfiler::StaticInitialize()
{
	// This delayed initialization is necessary to avoid recursion, when TLS variables that are used
	// by the profiler, are themselves managed by the memory manager.
	MemoryProfiler* temp = new (MemoryManager::LowLevelAllocate(sizeof(MemoryProfiler))) MemoryProfiler();
	temp->AllocateStructs();

	s_MemoryProfiler = temp;
}

void MemoryProfiler::StaticDestroy()
{
	s_MemoryProfiler->~MemoryProfiler();
	MemoryManager::LowLevelFree(s_MemoryProfiler);
	s_MemoryProfiler = NULL;
}

#include "BaseAllocator.h"
#include "Word.h"
#include "Stacktrace.h"

MemoryProfiler::MemoryProfiler()
	: m_DefaultRootHeader (NULL)
	, m_SizeUsed (0)
	, m_NumAllocations (0)
	, m_AccSizeUsed (0)
	, m_AccNumAllocations (0)
{
	memset (m_SizeDistribution, 0, sizeof(m_SizeDistribution));
}

MemoryProfiler::~MemoryProfiler()
{
	m_RecordingAllocation = true;
#if RECORD_ALLOCATION_SITES
	UNITY_DELETE(m_AllocationSites,kMemMemoryProfiler);
	UNITY_DELETE(m_AllocationSizes,kMemMemoryProfiler);
#endif
	UNITY_DELETE(m_ReferenceIDSizes,kMemMemoryProfiler);
	UNITY_DELETE(m_RootAllocationTypes,kMemMemoryProfiler);

	// using allocator directly to avoid allocation registration
	BaseAllocator* alloc = GetMemoryManager().GetAllocator(kMemProfiler);
	alloc->Deallocate(m_RootStack);
	m_RecordingAllocation = false;
}


void MemoryProfiler::SetupAllocationHeader(ProfilerAllocationHeader* header, ProfilerAllocationHeader* root, int size)
{
	header->SetRootPtr(root);
	header->accumulatedSize = size;
#if MAINTAIN_RELATED_ALLOCATION_LIST
	header->next = NULL;
	header->prev = NULL;
#endif
#if RECORD_ALLOCATION_SITES
	header->site = NULL;
#endif
}

void MemoryProfiler::AllocateStructs()
{
	m_RecordingAllocation = true;
	m_RootStackSize = 20;
	// using allocator directly to avoid allocation registration
	BaseAllocator* alloc = GetMemoryManager().GetAllocator(kMemProfiler);
	m_RootStack = (ProfilerAllocationHeader**)alloc->Allocate(m_RootStackSize*sizeof(ProfilerAllocationHeader*), kDefaultMemoryAlignment);
	m_CurrentRootHeader = &m_RootStack[0];
	*m_CurrentRootHeader = NULL;

#if RECORD_ALLOCATION_SITES
	m_AllocationSites = UNITY_NEW(AllocationSites, kMemMemoryProfiler)();
	m_AllocationSizes = UNITY_NEW(AllocationSizes,kMemMemoryProfiler)();
#endif
	m_ReferenceIDSizes = UNITY_NEW(ReferenceID,kMemMemoryProfiler)();
	m_RootAllocationTypes = UNITY_NEW(RootAllocationTypes, kMemMemoryProfiler)();

#if ROOT_UNRELATED_ALLOCATIONS
	int* defaultRootContainer = UNITY_NEW(int, kMemProfiler);
	m_DefaultRootHeader = GetMemoryManager().GetAllocator(kMemProfiler)->GetProfilerHeader(defaultRootContainer);
	SetupAllocationHeader(m_DefaultRootHeader, NULL, sizeof(int));
	RegisterRootAllocation(defaultRootContainer, GetMemoryManager().GetAllocator(kMemProfiler), "UnrootedAllocations", "");
	*m_CurrentRootHeader = m_DefaultRootHeader;
#endif

	m_RecordingAllocation = false;
}

void MemoryProfiler::ThreadCleanup()
{
	m_RecordingAllocation = true;
	if(m_RootStack)
	{
		// using allocator directly to avoid allocation registration
		BaseAllocator* alloc = GetMemoryManager().GetAllocator(kMemProfiler);
		alloc->Deallocate(m_RootStack);
	}
	m_RecordingAllocation = false;
}

void* g_LastAllocations[2];
int g_LastAllocationIndex = 0;

void MemoryProfiler::InitAllocation(void* ptr, BaseAllocator* alloc)
{
	Assert(!s_MemoryProfiler);
	if (alloc)
	{
		ProfilerAllocationHeader* const header = alloc->GetProfilerHeader(ptr);
		if (header)
		{
			size_t const size = alloc->GetPtrSize(ptr);
			SetupAllocationHeader(header, NULL, size);
		}
	}
}

void MemoryProfiler::RegisterAllocation(void* ptr, MemLabelRef label, const char* file, int line, size_t allocsize)
{
	BaseAllocator* alloc = GetMemoryManager().GetAllocator(label);
	size_t size = alloc ? alloc->GetPtrSize(ptr) : allocsize;

#if RECORD_ALLOCATION_SITES
	Mutex::AutoLock lock(m_Mutex);
#endif
	if (m_RecordingAllocation)
	{
		//mircea@ due to stupid init order, the gConsolePath std::string is not initialized when the assert triggers.
		// we really really really really need to find a good solution to avoid shit like that!
		//Assert(label.label == kMemMemoryProfilerId);
		ProfilerAllocationHeader* header = alloc ? alloc->GetProfilerHeader(ptr) : NULL;
		if(header)
		{
			SetupAllocationHeader(header, NULL, size);
		}
		return;
	}
	m_RecordingAllocation = true;

	ProfilerAllocationHeader* root = label.GetRootHeader();

	if(label.UseAutoRoot())
	{
		if(m_CurrentRootHeader != NULL)
			root = *m_CurrentRootHeader;
		else
			root = NULL;
	}

#if RECORD_ALLOCATION_SITES
	m_SizeUsed += size;
	m_NumAllocations++;
	m_AccSizeUsed += size;
	m_AccNumAllocations++;
	m_SizeDistribution[HighestBit(size)]++;

	AllocationSite site;
	site.label = label.label;
	site.file = file;
	site.line = line;
	site.allocated = 0;
	site.alloccount = 0;
	site.ownedAllocated = 0;
	site.ownedCount = 0;
	site.cummulativeAllocated = 0;
	site.cummulativeAlloccount = 0;
#if ENABLE_STACKS_ON_ALL_ALLOCS
	site.stack[0] = 0;
	static volatile bool track = false;
	//if(root && track)
	{
		//	if(Thread::CurrentThreadIsMainThread())
		{
			//static int counter = 0;
			//if(((counter++) & 0x100) == 0 || file == NULL)
			{
				SET_ALLOC_OWNER(NULL);
				site.stackHash = GetStacktrace(site.stack, 20, 3);
			}
		}
	}
#endif
	AllocationSites::iterator it = m_AllocationSites->find(site); // std::set will allocate on insertion (very rare)
	if(it == m_AllocationSites->end())
		it = m_AllocationSites->insert(site).first;
	AllocationSite* mutablesite = const_cast<AllocationSite*>(&(*it));
	mutablesite->allocated += size;
	mutablesite->alloccount++;
	mutablesite->cummulativeAllocated += size;
	mutablesite->cummulativeAlloccount++;
	if(root)
	{
		mutablesite->ownedAllocated += size;
		mutablesite->ownedCount++;
	}
#endif

	ProfilerAllocationHeader* header = alloc ? alloc->GetProfilerHeader(ptr) : NULL;
	if(header)
	{
		SetupAllocationHeader(header, root, size);

#if RECORD_ALLOCATION_SITES
		header->site = &(*it);
#endif

		if(root != NULL)
		{
			// Find the root owner and add this to the allocation list and accumulated size
			if(root)
			{
				AtomicAdd(&root->accumulatedSize, size);
#if MAINTAIN_RELATED_ALLOCATION_LIST
				InsertAfterRoot(root, header);
#endif
			}
		}

	}
	else if(alloc) // no alloc present for stray mallocs
	{
#if RECORD_ALLOCATION_SITES
		LocalHeaderInfo info = {size, &(*it)};
		m_AllocationSizes->insert(std::make_pair(ptr, info)); // Will allocate
#endif
	}

	g_LastAllocations[g_LastAllocationIndex]= ptr;
	g_LastAllocationIndex ^= 1;

	m_RecordingAllocation = false;
}

void MemoryProfiler::UnregisterAllocation(void* ptr, BaseAllocator* alloc, size_t freesize, ProfilerAllocationHeader** outputRootHeader, MemLabelRef label)
{
	if(ptr == NULL)
		return;

#if RECORD_ALLOCATION_SITES
	Mutex::AutoLock lock(m_Mutex);
#endif
	if (m_RecordingAllocation)
		return;

	size_t size = alloc ? alloc->GetPtrSize(ptr) : freesize;
	ProfilerAllocationHeader* header = alloc ? alloc->GetProfilerHeader(ptr) : NULL;
#if RECORD_ALLOCATION_SITES
	if(header && header->site == NULL)
		return;
#endif

	ProfilerAllocationHeader* root = NULL;
	m_RecordingAllocation = true;

	//	Assert(!header || header->IsRoot() || header->GetRootPtr() == label.GetRootHeader()); 

#if RECORD_ALLOCATION_SITES
	const AllocationSite* site = NULL;
	if(!alloc)
	{
#if ENABLE_STACKS_ON_ALL_ALLOCS
		AllocationSite tmpsite = {kMemLabelCount, {0}, 0, NULL, 0, 0, 0, 0, 0, 0, 0};
#else
		AllocationSite tmpsite = {kMemLabelCount, NULL, 0, 0, 0, 0, 0, 0, 0};
#endif
		AllocationSites::iterator it = m_AllocationSites->insert(tmpsite).first;
		site = &(*it);
	}
	else if ( !header )
	{
		AllocationSizes::iterator itPtrSize =  m_AllocationSizes->find(ptr);

#if UNITY_IPHONE
		// oh hello osam apple. When we override global new - we override it for everything.
		// BUT some libraries calls delete on pointers that were new-ed with system operator new
		// effectively crashing here. So play safe, yes
		if( itPtrSize == m_AllocationSizes->end() )
		{
			m_RecordingAllocation = false;
			return kMemNewDeleteId;
		}
#endif

		Assert(itPtrSize != m_AllocationSizes->end());

		size = (*itPtrSize).second.size;
		site = (*itPtrSize).second.site;
		m_AllocationSizes->erase(itPtrSize);
	}
#endif
	if(header)
	{
		root = header->GetRootPtr();
		if (header->GetRootPtr() || header->IsRoot())
		{
			if(header->IsRoot())
				root = header;

			int memoryleft = AtomicAdd(&root->accumulatedSize, -(int)size);
			if (root == header)
			{
				// This is the root object. Check that all linked allocations are deleted. If not, unlink all.
				if (memoryleft != 0)
				{
#if MAINTAIN_RELATED_ALLOCATION_LIST
					Mutex::AutoLock lock(m_Mutex);
					UnlinkAllAllocations(root);
#else
					ErrorString("Not all allocations related to a root has been deleted - might cause unity to crash later on!!");
#endif
				}
				root->rootReference->root = NULL;
				root->rootReference->Release();
				root->rootReference = NULL;
			}
#if MAINTAIN_RELATED_ALLOCATION_LIST
			UnlinkHeader(header);
#endif
		}
		else
			AtomicAdd(&header->accumulatedSize, -(int)size);
#if RECORD_ALLOCATION_SITES
		site = header->site;
#endif
	}

#if RECORD_ALLOCATION_SITES
	AllocationSite* mutablesite = const_cast<AllocationSite*>(site);
	mutablesite->allocated -= size;
	mutablesite->alloccount--;
	if(root)
	{
		mutablesite->ownedAllocated -= size;
		mutablesite->ownedCount--;
	}
	m_SizeUsed -= size;
	m_NumAllocations--;
	m_SizeDistribution[HighestBit(size)]--;
#endif
	// Roots are registered with their related data pointing to themselves.
	if(header && header->IsRegisteredRoot() )
		UnregisterRootAllocation (ptr);

	if (outputRootHeader != NULL)
		*outputRootHeader = root;

	m_RecordingAllocation = false;
	return;
}

void MemoryProfiler::TransferOwnership(void* ptr, BaseAllocator* alloc, ProfilerAllocationHeader* newRootHeader)
{
	Assert(alloc);
	size_t size = alloc->GetPtrSize(ptr);
	ProfilerAllocationHeader* header = alloc->GetProfilerHeader(ptr);

	if(header)
	{
		ProfilerAllocationHeader* root = header->GetRootPtr();
		if(root != NULL)
		{
#if MAINTAIN_RELATED_ALLOCATION_LIST
			// Unlink from currentRoot
			UnlinkHeader(header);
#endif
			AtomicAdd(&root->accumulatedSize, -(int)size);
			header->SetRootPtr(NULL);
#if RECORD_ALLOCATION_SITES
			AllocationSite* mutablesite = const_cast<AllocationSite*>(header->site);
			mutablesite->ownedAllocated -= size;
			mutablesite->ownedCount--;
#endif
		}

		if(newRootHeader == NULL)
			newRootHeader = m_DefaultRootHeader;

		// we have a new root. Relink
		if(newRootHeader)
		{
			DebugAssert(newRootHeader->IsRoot());

			AtomicAdd(&newRootHeader->accumulatedSize, size);
#if MAINTAIN_RELATED_ALLOCATION_LIST
			InsertAfterRoot(newRootHeader, header);
#endif
			header->SetRootPtr(newRootHeader);
#if RECORD_ALLOCATION_SITES
			AllocationSite* mutablesite = const_cast<AllocationSite*>(header->site);
			mutablesite->ownedAllocated += size;
			mutablesite->ownedCount++;
#endif
		}
	}
}

#if MAINTAIN_RELATED_ALLOCATION_LIST
void MemoryProfiler::UnlinkAllAllocations(ProfilerAllocationHeader* root)
{
#if ENABLE_STACKS_ON_ALL_ALLOCS
	// Print stack for root and stacks for all unallocated child allocations
	{
		char buffer[4048];
		void*const* s = root->site->stack;
		GetReadableStackTrace(buffer, 4048, (void**)(s), 20);
		printf_console(FormatString<TEMP_STRING>("Root still has %d allocated memory\nRoot allocated from\n%s\n",root->accumulatedSize, buffer).c_str());

		ProfilerAllocationHeader* header = root->next;
		while(header)
		{
			// for each allocation: print stack and decrease sites ownedcount
			char buffer[4048];
			void*const* s = header->site->stack;
			GetReadableStackTrace(buffer, 4048, (void**)(s), 20);
			printf_console(FormatString<TEMP_STRING>("%s\n", buffer).c_str());
			AllocationSite* mutablesite = const_cast<AllocationSite*>(header->site);

			mutablesite->ownedAllocated -= header->accumulatedSize;
			mutablesite->ownedCount--;
			header = header->next;
		}
	}
#else
	//	ErrorString("Not all allocations related to a root has been deleted - might cause unity to crash later on!!");
#endif

	// unlink all allocations
	ProfilerAllocationHeader* header = root->next;
	while(header)
	{
		ProfilerAllocationHeader* nextHeader = header->next;
		UnlinkHeader(header);
#if ROOT_UNRELATED_ALLOCATIONS
		header->SetRootPtr(m_DefaultRootHeader); // set root owner
		InsertAfterRoot(m_DefaultRootHeader, header);
#else
		header->SetRootPtr(NULL); // clear root owner
#endif
		header = nextHeader;
	}
	root->next = NULL;
}

void MemoryProfiler::InsertAfterRoot( ProfilerAllocationHeader* root, ProfilerAllocationHeader* header )
{
	DebugAssert(root->IsRoot());
	Mutex::AutoLock lock(m_Mutex);
	if(root->next)
		root->next->prev = header;
	header->next = root->next;
	header->prev = root;
	root->next = header;
}

void MemoryProfiler::UnlinkHeader(ProfilerAllocationHeader* header )
{
	Mutex::AutoLock lock(m_Mutex);

	DebugAssert(header->prev == NULL || header->prev->next == header);
	DebugAssert(header->next == NULL || header->next->prev == header);

	if(header->prev)
		header->prev->next = header->next;
	if(header->next)
		header->next->prev = header->prev;
	header->prev = NULL;
	header->next = NULL;
}


AllocationRootReference* MemoryProfiler::GetRootReferenceFromHeader(ProfilerAllocationHeader* root)
{
	if (root == NULL || root->rootReference == NULL || GetMemoryProfiler()->IsRecording())
		return NULL;
	Assert(root->IsRoot());
	root->rootReference->Retain();
	return root->rootReference;
}

#endif

void MemoryProfiler::ValidateRoot(ProfilerAllocationHeader* root)
{
	if(!root)
		return;
	size_t accSize = 0;
	Assert(root->IsRoot());
	Assert(root->next == NULL || root->next->prev == root);
	Assert(root->rootReference != NULL && root->rootReference->root != root);
	ProfilerAllocationHeader* header = root->next;
	while(header)
	{
		Assert (header->next == NULL || header->next->prev == header);
		Assert (header->relatesTo == root);
		accSize += header->accumulatedSize;
		header = header->next;
	}
	Assert(root->accumulatedSize >= accSize);
}

bool MemoryProfiler::PushAllocationRoot(void* root, bool forcePush)
{
	ProfilerAllocationHeader** current_root_header = m_CurrentRootHeader;
	if(current_root_header == NULL)
	{
		if (root == NULL)
			return false;
		m_RecordingAllocation = true;
		m_RootStackSize = 10;
		// using allocator directly to avoid allocation registration
		BaseAllocator* alloc = GetMemoryManager().GetAllocator(kMemProfiler);
		m_RootStack = (ProfilerAllocationHeader**)alloc->Allocate(m_RootStackSize*sizeof(ProfilerAllocationHeader*), kDefaultMemoryAlignment);
		m_CurrentRootHeader = &m_RootStack[0];
		*m_CurrentRootHeader = 0;
		current_root_header = m_CurrentRootHeader;
		m_RecordingAllocation = false;
	}

	ProfilerAllocationHeader* rootHeader = NULL;
	if(root != NULL)
	{
		BaseAllocator* rootAlloc = GetMemoryManager().GetAllocatorContainingPtr(root);
		Assert(rootAlloc);
		rootHeader = (ProfilerAllocationHeader*)rootAlloc->GetProfilerHeader(root);
		Assert(rootHeader == NULL || rootHeader->IsRoot());
	}

	if(!forcePush && rootHeader == *current_root_header)
		return false;

	UInt32 offset = m_CurrentRootHeader-m_RootStack;
	if(offset == m_RootStackSize-1)
	{
		m_RecordingAllocation = true;
		m_RootStackSize = m_RootStackSize*2;
		// using allocator directly to avoid allocation registration
		BaseAllocator* alloc = GetMemoryManager().GetAllocator(kMemProfiler);
		m_RootStack = (ProfilerAllocationHeader**)alloc->Reallocate(&m_RootStack[0],m_RootStackSize*sizeof(ProfilerAllocationHeader*), kDefaultMemoryAlignment);
		m_CurrentRootHeader = &m_RootStack[offset];
		current_root_header = m_CurrentRootHeader;
		m_RecordingAllocation = false;
	}

	*(++current_root_header) = rootHeader;
	m_CurrentRootHeader = current_root_header;
	return true;
}

void MemoryProfiler::PopAllocationRoot()
{
	--m_CurrentRootHeader;
}

ProfilerAllocationHeader* MemoryProfiler::GetCurrentRootHeader()
{
	ProfilerAllocationHeader* root = m_CurrentRootHeader ? *m_CurrentRootHeader : NULL;
	return root;
}

int MemoryProfiler::GetHeaderSize()
{
	return sizeof(ProfilerAllocationHeader);
}

size_t MemoryProfiler::GetRelatedMemorySize(const void* ptr){
	// this could use a linked list of related allocations, and thereby this info could be viewed as a hierarchy
	BaseAllocator* alloc = GetMemoryManager().GetAllocatorContainingPtr(ptr);
	ProfilerAllocationHeader* header = alloc ? alloc->GetProfilerHeader(ptr) : NULL;
	return header ? header->accumulatedSize : 0;
}

void MemoryProfiler::RegisterMemoryToID( size_t id, size_t size )
{
	Mutex::AutoLock lock(m_Mutex);
	m_RecordingAllocation = true;
	ReferenceID::iterator itIDSize = m_ReferenceIDSizes->find(id);
	if(itIDSize == m_ReferenceIDSizes->end())
		m_ReferenceIDSizes->insert(std::make_pair(id,size));
	else
		itIDSize->second += size;
	m_RecordingAllocation = false;
}

void MemoryProfiler::UnregisterMemoryToID( size_t id, size_t size )
{
	Mutex::AutoLock lock(m_Mutex);
	m_RecordingAllocation = true;
	ReferenceID::iterator itIDSize = m_ReferenceIDSizes->find(id);
	if (itIDSize != m_ReferenceIDSizes->end())
	{
		itIDSize->second -= size;
		if(itIDSize->second == 0)
			m_ReferenceIDSizes->erase(itIDSize);
	}
	else
		ErrorString("Id not found in map");
	m_RecordingAllocation = false;
}

size_t MemoryProfiler::GetRelatedIDMemorySize(size_t id)
{
	Mutex::AutoLock lock(m_Mutex);
	ReferenceID::iterator itIDSize = m_ReferenceIDSizes->find(id);
	if(itIDSize == m_ReferenceIDSizes->end())
		return 0;
	return itIDSize->second;
}


ProfilerString MemoryProfiler::GetOverview() const
{
#if RECORD_ALLOCATION_SITES
	struct MemCatInfo
	{
		UInt64 totalMem;
		UInt64 totalCount;
		UInt64 cummulativeMem;
		UInt64 cummulativeCount;
	};
	MemCatInfo info[kMemLabelCount+1];
	MemCatInfo allocatorMemUsage[16];
	MemCatInfo totalMemUsage;
	BaseAllocator* allocators[16];

	memset (&info,0,sizeof(info));
	memset (&allocatorMemUsage,0,sizeof(allocatorMemUsage));
	memset (&totalMemUsage,0,sizeof(totalMemUsage));
	memset (&allocators,0,sizeof(allocators));


	AllocationSites::iterator it = m_AllocationSites->begin();
	for( ;it != m_AllocationSites->end(); ++it)
	{
		MemLabelIdentifier label = (*it).label;
		if (label >= kMemLabelCount)
			label = kMemLabelCount;
		info[label].totalMem += (*it).allocated;
		info[label].totalCount += (*it).alloccount;
		info[label].cummulativeMem += (*it).cummulativeAllocated;
		info[label].cummulativeCount += (*it).cummulativeAlloccount;

		MemLabelId memLabel(label, NULL);
		BaseAllocator* alloc = GetMemoryManager().GetAllocator(memLabel);
		int allocatorindex = GetMemoryManager().GetAllocatorIndex(alloc);
		allocatorMemUsage[allocatorindex].totalMem += (*it).allocated;
		allocatorMemUsage[allocatorindex].totalCount += (*it).alloccount;
		allocatorMemUsage[allocatorindex].cummulativeMem += (*it).cummulativeAllocated;
		allocatorMemUsage[allocatorindex].cummulativeCount += (*it).cummulativeAlloccount;
		allocators[allocatorindex] = alloc;
		totalMemUsage.totalMem += (*it).allocated;
		totalMemUsage.totalCount += (*it).alloccount;
		totalMemUsage.cummulativeMem += (*it).cummulativeAllocated;
		totalMemUsage.cummulativeCount += (*it).cummulativeAlloccount;
	}

	const int kStringSize = 400*1024;
	TEMP_STRING str;
	str.reserve(kStringSize);
	// Total memory registered in all allocators
	str += FormatString<TEMP_STRING>("[ Total Memory ] : %0.2fMB ( %d ) [%0.2fMB ( %d )]\n\n", (float)(totalMemUsage.totalMem)/(1024.f*1024.f), totalMemUsage.totalCount,
		(float)(totalMemUsage.cummulativeMem)/(1024.f*1024.f), totalMemUsage.cummulativeCount);

	// Memory registered by allocators
	for(int i = 0; i < 16; i++)
	{
		if (allocatorMemUsage[i].cummulativeCount == 0)
			continue;
		BaseAllocator* alloc = allocators[i];
		str += FormatString<TEMP_STRING>("[ %s ] : %0.2fKB ( %d ) [acc: %0.2fMB ( %d )] (Requested:%0.2fKB, Overhead:%0.2fKB, Reserved:%0.2fMB)\n", (alloc?alloc->GetName():"Custom"), (float)(allocatorMemUsage[i].totalMem)/1024.f, allocatorMemUsage[i].totalCount,
			(float)(allocatorMemUsage[i].cummulativeMem)/(1024.f*1024.f), allocatorMemUsage[i].cummulativeCount,(float) (alloc?alloc->GetAllocatedMemorySize():0)/1024.f, (float)((alloc?alloc->GetAllocatorSizeTotalUsed():0) - (alloc?alloc->GetAllocatedMemorySize():0))/1024.f, (alloc?alloc->GetReservedSizeTotal():0)/(1024.f*1024.f));
	}

	// Memory registered on labels
	for(int i = 0; i <= kMemLabelCount; i++)
	{
		if (info[i].cummulativeCount == 0)
			continue;
		MemLabelId label((MemLabelIdentifier)i, NULL);
		BaseAllocator* alloc = GetMemoryManager().GetAllocator(label);
		str += FormatString<TEMP_STRING>("\n[ %s : %s ]\n", GetMemoryManager().GetMemcatName(label), alloc?alloc->GetName():"Custom");
		str += FormatString<TEMP_STRING>("  TotalAllocated : %0.2fKB ( %d ) [%0.2fMB ( %d )]\n", (float)(info[i].totalMem)/1024.f, info[i].totalCount,
			(float)(info[i].cummulativeMem)/(1024.f*1024.f), info[i].cummulativeCount);
	}

#if ENABLE_STACKS_ON_ALL_ALLOCS
	const int kBufferSize = 8*1024;
	char buffer[kBufferSize];
	UNITY_VECTOR(kMemMemoryProfiler,const AllocationSite*) sortedVector;
	sortedVector.reserve(m_AllocationSites->size());
	it = m_AllocationSites->begin();
	for( ;it != m_AllocationSites->end(); ++it)
		sortedVector.push_back(&(*it));
	std::sort(sortedVector.begin(), sortedVector.end(), AllocationSite::Sorter());
	{
		UNITY_VECTOR(kMemMemoryProfiler, const AllocationSite*)::iterator it = sortedVector.begin();
		for( ;it != sortedVector.end(); ++it)
		{
			if(str.length()>kStringSize-kBufferSize)
				break;
			if( (*it)->alloccount == 0)
				break;
			str += FormatString<TEMP_STRING>("\n[ %s ]\n", GetMemoryManager().GetMemcatName(MemLabelId((*it)->label, NULL)));
			if((*it)->stack[0] != 0)
			{
				GetReadableStackTrace(buffer, kBufferSize, (void**)((*it)->stack), 20);
				str += FormatString<TEMP_STRING>("%s\n", buffer);
			}
			else
				str += FormatString<TEMP_STRING>("[ %s:%d ]\n", (*it)->file, (*it)->line);
			//, (*it).file);
			str += FormatString<TEMP_STRING>("  TotalAllocated : %0.2fKB ( %d ) [%0.2fMB ( %d )]\n", (float)((*it)->allocated)/1024.f, (*it)->alloccount,
				(float)((*it)->cummulativeAllocated)/(1024.f*1024.f), (*it)->cummulativeAlloccount);
		}
	}
#endif
	return ProfilerString(str.c_str());
#else
	return ProfilerString();
#endif
}

#if RECORD_ALLOCATION_SITES
struct MonoObjectMemoryStackInfo
{
	bool expanded;
	bool sorted;
	int allocated;
	int ownedAllocated;
	MonoArray* callerSites;
	ScriptingStringPtr name;
};

MonoObject* MemoryProfiler::MemoryStackEntry::Deserialize()
{
	MonoClass* klass = GetMonoManager().GetMonoClass ("ObjectMemoryStackInfo", "UnityEditorInternal");
	MonoObject* obj = mono_object_new (mono_domain_get(), klass);

	MonoObjectMemoryStackInfo& memInfo = ExtractMonoObjectData<MonoObjectMemoryStackInfo> (obj);
	memInfo.expanded = false;
	memInfo.sorted = false;
	memInfo.allocated = totalMemory;
	memInfo.ownedAllocated = ownedMemory;
	string tempname (name.c_str(),name.size()-2);
	memInfo.name = MonoStringNew(tempname);
	memInfo.callerSites = mono_array_new(mono_domain_get(), klass, callerSites.size());

	std::map<void*,MemoryStackEntry>::iterator it = callerSites.begin();
	int index = 0;
	while(it != callerSites.end())
	{
		MonoObject* child = (*it).second.Deserialize();
		GetMonoArrayElement<MonoObject*> (memInfo.callerSites,index++) = child;
		++it;
	}
	return obj;
}

MemoryProfiler::MemoryStackEntry* MemoryProfiler::GetStackOverview() const
{
	m_RecordingAllocation = true;
	MemoryStackEntry* topLevel = UNITY_NEW(MemoryStackEntry,kMemProfiler);
	topLevel->name = "Allocated unity memory ";
	AllocationSites::iterator it = m_AllocationSites->begin();
	for( ;it != m_AllocationSites->end(); ++it)
	{
		int size = (*it).allocated;
		if(size == 0)
			continue;
		int ownedsize = (*it).ownedAllocated;
		void*const* stack = &(*it).stack[0];
		MemoryStackEntry* currentLevel = topLevel;
		currentLevel->totalMemory += size;
		currentLevel->ownedMemory += ownedsize;
		while(*stack)
		{
			currentLevel = &(currentLevel->callerSites[*stack]);
			if(currentLevel->totalMemory == 0)
			{
				char buffer[2048];
				void* temp[2];
				temp[0] = *stack;
				temp[1] = 0;
				GetReadableStackTrace(buffer, 2048, (void**)temp, 1);
				currentLevel->name = std::string(buffer,40);
			}
			currentLevel->totalMemory += size;
			currentLevel->ownedMemory += ownedsize;
			++stack;
		}
	}
	m_RecordingAllocation = false;
	return topLevel;
}

void MemoryProfiler::ClearStackOverview(MemoryProfiler::MemoryStackEntry* entry) const
{
	m_RecordingAllocation = true;
	UNITY_DELETE(entry, kMemProfiler);
	m_RecordingAllocation = false;
}
#endif

void MemoryProfiler::RegisterRootAllocation (void* root, BaseAllocator* allocator, const char* areaName, const char* objectName)
{
	bool result = true;
	if(areaName != NULL)
	{
		m_Mutex.Lock();
		m_RecordingAllocation = true;

		RootAllocationType typeData;
		typeData.areaName = areaName;
		typeData.objectName = objectName;
		result = m_RootAllocationTypes->insert(std::make_pair (root, typeData)).second;

		m_RecordingAllocation = false;
		m_Mutex.Unlock();
	}
	if (result)
	{
		ProfilerAllocationHeader* header = allocator->GetProfilerHeader(root);
		if(header)
		{
			if (!header->rootReference)
			{
				m_RecordingAllocation = true;
				header->rootReference = UNITY_NEW(AllocationRootReference, kMemProfiler) (header);
				m_RecordingAllocation = false;
			}
			if(areaName)
				header->SetAsRegisteredRoot();
			else
				header->SetAsRoot();
		}
	}
	else
	{
		ErrorString("Registered allocation root already exists");
	}
}

void MemoryProfiler::UnregisterRootAllocation (void* root)
{
	m_Mutex.Lock();
	m_RecordingAllocation = true;
	bool result = m_RootAllocationTypes->erase(root);
	m_RecordingAllocation = false;
	m_Mutex.Unlock();

	if (!result)
	{
		ErrorString("Allocation root has already been deleted");
	}
}

void MemoryProfiler::SetRootAllocationObjectName (void* root, const char* objectName)
{
	Mutex::AutoLock lock (m_Mutex);
	m_RecordingAllocation = true;
	RootAllocationTypes::iterator it = m_RootAllocationTypes->find(root);
	Assert(it != m_RootAllocationTypes->end());
	(*it).second.objectName = objectName;
	m_RecordingAllocation = false;
}

void MemoryProfiler::GetRootAllocationInfos (RootAllocationInfos& infos)
{
	Mutex::AutoLock lock (m_Mutex);
	m_RecordingAllocation = true;

	int index = infos.size();
	infos.resize_uninitialized(infos.size() + m_RootAllocationTypes->size());
	for (RootAllocationTypes::const_iterator i=m_RootAllocationTypes->begin();i != m_RootAllocationTypes->end();++i)
	{
		RootAllocationInfo& info = infos[index++];
		info.memorySize = GetRelatedMemorySize(i->first);
		info.areaName = i->second.areaName;
		info.objectName = i->second.objectName.c_str();
	}
	m_RecordingAllocation = false;
}

ProfilerAllocationHeader* MemoryProfiler::GetAllocationRootHeader(void* ptr, MemLabelRef label) const
{
	BaseAllocator* alloc = GetMemoryManager().GetAllocator(label);
	ProfilerAllocationHeader* header = alloc?alloc->GetProfilerHeader(ptr):NULL;
	return header?header->GetRootPtr(): NULL;
}


#if RECORD_ALLOCATION_SITES
ProfilerString MemoryProfiler::GetUnrootedAllocationsOverview()
{
	ProfilerString result;
#if ROOT_UNRELATED_ALLOCATIONS
	m_RecordingAllocation = true;
	std::map<const AllocationSite*,size_t> allocations;
	ProfilerAllocationHeader* next = m_DefaultRootHeader->next;
	while(next)
	{
		if(!next->IsRoot())
		{
			Assert(next->relatesTo == m_DefaultRootHeader);
			allocations[next->site]+=next->accumulatedSize;
		}
		next = next->next;
	}
	std::vector<std::pair<const AllocationSite*,size_t> > sorted;
	std::map<const AllocationSite*,size_t>::iterator it = allocations.begin();
	for( ;it != allocations.end(); ++it)
	{
		sorted.push_back(*it);
	}
	std::sort(sorted.begin(), sorted.end(), AllocationSiteSizeSorter());
	for(int i = 0;i < sorted.size(); ++i)
	{
		int size = sorted[i].second;
		printf_console("%db\n", size);
		char buffer[2048];
		GetReadableStackTrace(buffer, 2048, (void**)sorted[i].first->stack, 10);
		printf_console("%s\n", buffer);
		if(i > 20)
			break;
	}
	m_RecordingAllocation = false;
#endif
	return result;
}

#endif


#endif

