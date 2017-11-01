#include "UnityPrefix.h"
#include "MemoryProfiler.h"
#include "MemoryManager.h"
#include "MemoryMacros.h"

#if ENABLE_MEM_PROFILER

bool push_allocation_root(void* root, bool forcePush)
{
	return GetMemoryProfiler()?GetMemoryProfiler()->PushAllocationRoot(root, forcePush):false;
}

void  pop_allocation_root()
{
	GetMemoryProfiler()->PopAllocationRoot();
}

ProfilerAllocationHeader* get_current_allocation_root_header_internal()
{
	return GetMemoryProfiler()?GetMemoryProfiler()->GetCurrentRootHeader():NULL;
}

ProfilerAllocationHeader* get_allocation_header_internal(void* ptr, MemLabelRef label)
{
	BaseAllocator* alloc = GetMemoryManager().GetAllocator(label);
	return alloc->GetProfilerHeader(ptr);
}

void transfer_ownership_root_header(void* source, MemLabelRef label, ProfilerAllocationHeader* newRootHeader)
{
	if(GetMemoryManager().IsTempAllocatorLabel(label))
		return;
	GetMemoryProfiler()->TransferOwnership(source, GetMemoryManager().GetAllocator(label), newRootHeader);
}

void transfer_ownership(void* source, MemLabelRef label, const void* newroot)
{
	BaseAllocator* rootAlloc = GetMemoryManager().GetAllocatorContainingPtr(newroot);
	ProfilerAllocationHeader* rootHeader = rootAlloc->GetProfilerHeader(newroot);
	transfer_ownership_root_header(source, label, rootHeader);
}

void set_root_allocation(void* root, MemLabelRef label, const char* areaName, const char* objectName)
{
	pop_allocation_root();
	GetMemoryProfiler ()->RegisterRootAllocation (root, GetMemoryManager().GetAllocator(label), areaName, objectName);
}

void  assign_allocation_root(void* root, MemLabelRef label, const char* areaName, const char* objectName)
{
	GetMemoryProfiler ()->RegisterRootAllocation (root, GetMemoryManager().GetAllocator(label), areaName, objectName);
}

AllocationRootReference* get_root_reference_from_header(ProfilerAllocationHeader* root)
{
	return MemoryProfiler::GetRootReferenceFromHeader(root);
}

AllocationRootReference* copy_root_reference(AllocationRootReference* rootRef)
{
	if (rootRef)
		rootRef->Retain();
	return rootRef;
}

void release_root_reference(AllocationRootReference* rootRef)
{
	if (rootRef)
		rootRef->Release();
}

ProfilerAllocationHeader* get_root_header_from_reference(AllocationRootReference* rootref)
{
	return rootref? rootref->root: NULL;
}

#endif

void ValidateAllocatorIntegrity(MemLabelId label)
{
#if ENABLE_MEMORY_MANAGER
	GetMemoryManager().GetAllocator(label)->CheckIntegrity();
#endif
}

