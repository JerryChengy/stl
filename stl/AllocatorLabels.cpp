#include "UnityPrefix.h"
#include "AllocatorLabels.h"
#include "Allocator.h"
#include "MemoryProfiler.h"

void InitializeMemoryLabels()
{
#define INITIALIZE_LABEL_STRUCT(Name) Name.Initialize(Name##Id);
#define DO_LABEL(Name) INITIALIZE_LABEL_STRUCT(kMem##Name) 
#include "AllocatorLabelNames.h"
#undef DO_LABEL
#undef INITIALIZE_LABEL_STRUCT
}

const char* const MemLabelName[] =
{
#define DO_LABEL(Name) #Name ,
#include "AllocatorLabelNames.h"
#undef DO_LABEL
};

#define DO_LABEL_STRUCT(Name) EXPORT_COREMODULE Name##Struct Name; 
#define DO_LABEL(Name) DO_LABEL_STRUCT(kMem##Name) 
#include "AllocatorLabelNames.h"
#undef DO_LABEL
#undef DO_LABEL_STRUCT

#if ENABLE_MEM_PROFILER

MemLabelId::MemLabelId( const MemLabelId& other ) : label(other.label), rootReference(NULL)
{	
	rootReference = copy_root_reference(other.rootReference);
}

void MemLabelId::SetRootHeader( ProfilerAllocationHeader* rootHeader )
{
	if (rootReference)
	//	ReleaseReference();
	rootReference = get_root_reference_from_header(rootHeader);
}

testlabelid::testlabelid( const testlabelid& other ) : label(other.label), rootReference(NULL)
{	
	rootReference = copy_root_reference(other.rootReference);
}

const testlabelid& testlabelid::operator=( const testlabelid& other )
{
	label = other.label;
	if (rootReference)
		ReleaseReference();
	rootReference = copy_root_reference(other.rootReference);
	return *this;
}
ProfilerAllocationHeader* testlabelid::GetRootHeader() const
{
	return rootReference ? get_root_header_from_reference(rootReference) : NULL;
}
void testlabelid::ReleaseReference()
{
	release_root_reference(rootReference); 
	rootReference = NULL;
}
void testlabelid::SetRootHeader( ProfilerAllocationHeader* rootHeader )
{
	if (rootReference)
		ReleaseReference();
	rootReference = get_root_reference_from_header(rootHeader);
}

ProfilerAllocationHeader* MemLabelId::GetRootHeader() const
{
	return rootReference ? get_root_header_from_reference(rootReference) : NULL;
}

void MemLabelId::ReleaseReference()
{
	release_root_reference(rootReference); 
	rootReference = NULL;
}

const MemLabelId& MemLabelId::operator=( const MemLabelId& other )
{
	label = other.label;
	if (rootReference)
		ReleaseReference();
	rootReference = copy_root_reference(other.rootReference);
	return *this;
}

#endif

