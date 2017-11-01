#ifndef ALLOCATORLABELS_H
#define ALLOCATORLABELS_H

#include "ExportModules.h"
#define NULL 0
#define ENABLE_MEM_PROFILER (ENABLE_MEMORY_MANAGER && ENABLE_PROFILER)
#if ENABLE_MEM_PROFILER
#define IF_MEMORY_PROFILER_ENABLED(x) x
#else
#define IF_MEMORY_PROFILER_ENABLED(x) 
#endif

// Must be in Sync with ENUM WiiMemory in PlayerSettings.txt
enum MemLabelIdentifier
{
#define DO_LABEL(Name) kMem##Name##Id ,
#include "AllocatorLabelNames.h"
#undef DO_LABEL
	kMemLabelCount
};

struct AllocationRootReference;
struct ProfilerAllocationHeader;

struct EXPORT_COREMODULE testlabelid
{
	testlabelid() { IF_MEMORY_PROFILER_ENABLED( rootReference = NULL ); }
	explicit testlabelid ( MemLabelIdentifier id, ProfilerAllocationHeader* root ) : label(id) { IF_MEMORY_PROFILER_ENABLED( rootReference = NULL; SetRootHeader(root) ); }
	void Initialize ( MemLabelIdentifier id ) { label = id; IF_MEMORY_PROFILER_ENABLED( rootReference = NULL ); }
	MemLabelIdentifier label;
#if ENABLE_MEM_PROFILER
	testlabelid ( const testlabelid& other );
	~testlabelid () { if(rootReference) ReleaseReference(); }

	const testlabelid& operator= (const testlabelid& other);
	void ReleaseReference();
	ProfilerAllocationHeader* GetRootHeader () const;
	void SetRootHeader ( ProfilerAllocationHeader* rootHeader );
	bool UseAutoRoot () const { return rootReference == NULL; };
private:
	// root reference = NULL: use stack to get root
	AllocationRootReference* rootReference;
#else
ProfilerAllocationHeader* GetRootHeader () const {return NULL;}
void SetRootHeader ( ProfilerAllocationHeader* rootHeader ) ;
#endif
};

struct EXPORT_COREMODULE MemLabelId {
	MemLabelId() { IF_MEMORY_PROFILER_ENABLED( rootReference = NULL ); }
	explicit MemLabelId ( MemLabelIdentifier id, ProfilerAllocationHeader* root ) : label(id) { IF_MEMORY_PROFILER_ENABLED( rootReference = NULL; SetRootHeader(root) ); }
	void Initialize ( MemLabelIdentifier id ) { label = id; IF_MEMORY_PROFILER_ENABLED( rootReference = NULL ); }

	MemLabelIdentifier label;

#if ENABLE_MEM_PROFILER
	MemLabelId ( const MemLabelId& other );
	~MemLabelId () { if(rootReference) ReleaseReference(); }

	const MemLabelId& operator= (const MemLabelId& other);
	void ReleaseReference();
	ProfilerAllocationHeader* GetRootHeader () const;
	void SetRootHeader ( ProfilerAllocationHeader* rootHeader );
	bool UseAutoRoot () const { return rootReference == NULL; };
private:
	// root reference = NULL: use stack to get root
	AllocationRootReference* rootReference;
#else
	ProfilerAllocationHeader* GetRootHeader () const {return NULL;}
	void SetRootHeader ( ProfilerAllocationHeader* rootHeader ) ;
#endif
};

#if ENABLE_MEM_PROFILER
typedef MemLabelId MemLabelRef;
#else
typedef MemLabelId MemLabelRef;
#endif

#define DO_LABEL_STRUCT(Name) struct EXPORT_COREMODULE Name##Struct : public MemLabelId { }; extern EXPORT_COREMODULE Name##Struct Name;

#define DO_LABEL(Name) DO_LABEL_STRUCT(kMem##Name) 
#include "AllocatorLabelNames.h"
#undef DO_LABEL

#undef DO_LABEL_STRUCT

void InitializeMemoryLabels();

//#if UNITY_ENABLE_ACCOUNTING_ALLOCATORS
extern const char* const MemLabelName[];
//#endif


#endif
