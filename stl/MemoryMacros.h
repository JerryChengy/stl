#ifndef _MEMORY_MACROS_H_
#define _MEMORY_MACROS_H_

#include <new>
#include "FileStripped.h"
#include "AllocatorLabels.h"


#if defined(__GNUC__)
#define ALIGN_OF(T) __alignof__(T)
#define ALIGN_TYPE(val) __attribute__((aligned(val)))
#define FORCE_INLINE inline __attribute__ ((always_inline))
#elif defined(_MSC_VER)
#define ALIGN_OF(T) __alignof(T)
#define ALIGN_TYPE(val) __declspec(align(val))
#define FORCE_INLINE __forceinline
#else
#define ALIGN_TYPE(size)
#define FORCE_INLINE inline
#endif


#if ENABLE_MEMORY_MANAGER
//	These methods are added to be able to make some initial allocations that does not use the memory manager
extern void* GetPreallocatedMemory(int size);
#	define HEAP_NEW(cls) new (GetPreallocatedMemory(sizeof(cls))) cls
#	define HEAP_DELETE(obj, cls) obj->~cls();
#else
#	define HEAP_NEW(cls) new (UNITY_LL_ALLOC(kMemDefault,sizeof(cls),kDefaultMemoryAlignment)) cls
#	define HEAP_DELETE(obj, cls) {obj->~cls();UNITY_LL_FREE(kMemDefault,(void*)obj);}
#endif

enum
{
#if UNITY_OSX || UNITY_IPHONE || UNITY_ANDROID || UNITY_PS3 || UNITY_XENON || UNITY_BB10 || UNITY_TIZEN
	kDefaultMemoryAlignment = 16
#else
	kDefaultMemoryAlignment = sizeof(void*)
#endif
};

enum
{
	kAllocateOptionNone = 0,						// Fatal: Show message box with out of memory error and quit application
	kAllocateOptionReturnNullIfOutOfMemory = 1	// Returns null if allocation fails (doesn't show message box)
};


#if ENABLE_MEMORY_MANAGER

// new override does not work on mac together with pace
#if !(UNITY_OSX && UNITY_EDITOR) && !(UNITY_PLUGIN) && !UNITY_WEBGL && !UNITY_BB10 && !UNITY_TIZEN
void* operator new (size_t size) throw();
#if UNITY_PS3
void* operator new (size_t size, size_t alignment) throw();
void* operator new [] (size_t size, size_t alignment) throw();
#endif
void* operator new [] (size_t size) throw();
void operator delete (void* p) throw();
void operator delete [] (void* p) throw();

void* operator new (size_t size, const std::nothrow_t&) throw();
void* operator new [] (size_t size, const std::nothrow_t&) throw();
void operator delete (void* p, const std::nothrow_t&) throw();
void operator delete [] (void* p, const std::nothrow_t&) throw();
#endif

#endif
#if ENABLE_MEM_PROFILER

EXPORT_COREMODULE bool  push_allocation_root(void* root, bool forcePush);
EXPORT_COREMODULE void  pop_allocation_root();
EXPORT_COREMODULE ProfilerAllocationHeader* get_current_allocation_root_header_internal();
EXPORT_COREMODULE void  set_root_allocation(void* root,  MemLabelRef label, const char* areaName, const char* objectName);
EXPORT_COREMODULE void  assign_allocation_root(void* root,  MemLabelRef label, const char* areaName, const char* objectName);
EXPORT_COREMODULE ProfilerAllocationHeader* get_allocation_header_internal(void* ptr,  MemLabelRef label);
EXPORT_COREMODULE AllocationRootReference* get_root_reference_from_header(ProfilerAllocationHeader* root);
EXPORT_COREMODULE AllocationRootReference* copy_root_reference(AllocationRootReference* rootref);
EXPORT_COREMODULE void release_root_reference(AllocationRootReference* rootRef);
EXPORT_COREMODULE ProfilerAllocationHeader* get_root_header_from_reference(AllocationRootReference* rootref);

class AutoScopeRoot
{
public:
	AutoScopeRoot(void* root) { pushed = push_allocation_root(root, false); }
	~AutoScopeRoot() { if(pushed) pop_allocation_root(); }
	bool pushed;
};

#define GET_CURRENT_ALLOC_ROOT_HEADER() get_current_allocation_root_header_internal()
#define GET_ALLOC_HEADER(ptr, label) get_allocation_header_internal(ptr, label)
#define SET_ALLOC_OWNER(root) AutoScopeRoot autoScopeRoot(root)
#define UNITY_TRANSFER_OWNERSHIP(source, label, newroot) transfer_ownership(source, label, newroot)
#define UNITY_TRANSFER_OWNERSHIP_TO_HEADER(source, label, newrootheader) transfer_ownership_root_header(source, label, newrootheader)

template<typename T>
inline T* set_allocation_root(T* root, MemLabelRef label, const char* areaName, const char* objectName)
{
	set_root_allocation(root, label, areaName, objectName);
	return root;
}

#else

#define GET_CURRENT_ALLOC_ROOT_HEADER() NULL
#define GET_ALLOC_HEADER(ptr, label) NULL
#define SET_ALLOC_OWNER(root) {}
#define UNITY_TRANSFER_OWNERSHIP(source, label, newroot) {}
#define UNITY_TRANSFER_OWNERSHIP_TO_HEADER(source, label, newrootheader) {}

#endif

EXPORT_COREMODULE void* operator new (size_t size, MemLabelRef label, bool set_root, int align, const char* file, int line);
void* operator new [] (size_t size, MemLabelRef label, bool set_root, int align, const char* file, int line);
EXPORT_COREMODULE void operator delete (void* p, MemLabelRef label, bool set_root, int align, const char* file, int line);
void operator delete [] (void* p, MemLabelRef label, bool set_root, int align, const char* file, int line);

void* malloc_internal(size_t size, int align, MemLabelRef label, int allocateOptions, const char* file, int line);
 void* calloc_internal(size_t count, size_t size, int align, MemLabelRef label, int allocateOptions, const char* file, int line);
 void* realloc_internal(void* ptr, size_t size, int align, MemLabelRef label, int allocateOptions, const char* file, int line);
void free_internal(void* ptr);
void EXPORT_COREMODULE free_alloc_internal(void* ptr, MemLabelRef label);

void transfer_ownership(void* source, MemLabelRef label, const void* newroot);
void transfer_ownership_root_header(void* source, MemLabelRef label, ProfilerAllocationHeader* newRootHeader);

void register_external_gfx_allocation(void* ptr, size_t size, size_t related, const char* file, int line);
void register_external_gfx_deallocation(void* ptr, const char* file, int line);

#define UNITY_MALLOC(label, size)                      malloc_internal(size, kDefaultMemoryAlignment, label, kAllocateOptionNone, __FILE_STRIPPED__, __LINE__)
#define UNITY_MALLOC_NULL(label, size)                 malloc_internal(size, kDefaultMemoryAlignment, label, kAllocateOptionReturnNullIfOutOfMemory, __FILE_STRIPPED__, __LINE__)
#define UNITY_MALLOC_ALIGNED(label, size, align)       malloc_internal(size, align, label, kAllocateOptionNone, __FILE_STRIPPED__, __LINE__)
#define UNITY_MALLOC_ALIGNED_NULL(label, size, align)  malloc_internal(size, align, label, kAllocateOptionReturnNullIfOutOfMemory, __FILE_STRIPPED__, __LINE__)
#define UNITY_CALLOC(label, count, size)               calloc_internal(count, size, kDefaultMemoryAlignment, label, kAllocateOptionNone, __FILE_STRIPPED__, __LINE__)
#define UNITY_REALLOC_(label, ptr, size)               realloc_internal(ptr, size, kDefaultMemoryAlignment, label, kAllocateOptionNone, __FILE_STRIPPED__, __LINE__)
#define UNITY_REALLOC_ALIGNED(label, ptr, size, align) realloc_internal(ptr, size, align, label, kAllocateOptionNone, __FILE_STRIPPED__, __LINE__)
#define UNITY_FREE(label, ptr)                         free_alloc_internal(ptr, label)

#define REGISTER_EXTERNAL_GFX_ALLOCATION_REF(ptr, size, related) register_external_gfx_allocation((void*)ptr, size, (size_t)related, __FILE_STRIPPED__, __LINE__)
#define REGISTER_EXTERNAL_GFX_DEALLOCATION(ptr) register_external_gfx_deallocation((void*)ptr, __FILE_STRIPPED__, __LINE__)

template<typename T>
inline void delete_internal(T* ptr, MemLabelRef label) { if (ptr) ptr->~T(); UNITY_FREE(label,ptr); }

#define UNITY_NEW(type, label) new (label, false, kDefaultMemoryAlignment, __FILE_STRIPPED__, __LINE__) type
#define UNITY_NEW_ALIGNED(type, label, align) new (label, false, align, __FILE_STRIPPED__, __LINE__) type
#define UNITY_DELETE(ptr, label) { delete_internal(ptr, label); ptr = NULL; }

#if ENABLE_MEM_PROFILER
#define UNITY_NEW_AS_ROOT(type, label, areaName, objectName) set_allocation_root(new (label, true, kDefaultMemoryAlignment, __FILE_STRIPPED__, __LINE__) type, label, areaName, objectName)
#define UNITY_NEW_AS_ROOT_ALIGNED(type, label, align, areaName, objectName) set_allocation_root(new (label, true, align, __FILE_STRIPPED__, __LINE__) type, label, areaName, objectName)
#define SET_PTR_AS_ROOT(ptr, label, areaName, objectName) assign_allocation_root(ptr, label, areaName, objectName)
#else
#define UNITY_NEW_AS_ROOT(type, label, areaName, objectName) new (label, true, kDefaultMemoryAlignment, __FILE_STRIPPED__, __LINE__) type
#define UNITY_NEW_AS_ROOT_ALIGNED(type, label, align, areaName, objectName) set_allocation_root(new (label, true, align, __FILE_STRIPPED__, __LINE__) type, label, areaName, objectName)
#define SET_PTR_AS_ROOT(ptr, label, areaName, objectName) {}
#endif

// Deprecated -> Move to new Macros
#define UNITY_ALLOC(label, size, align)                UNITY_MALLOC_ALIGNED(label, size, align)
#define UNITY_REALLOC(label, ptr, size, align)         UNITY_REALLOC_ALIGNED(label, ptr, size, align)

// Check the integrity of the allocator backing a label. Use this to track down memory overwrites
void ValidateAllocatorIntegrity(MemLabelId label);

#include "STLAllocator.h"

/// ALLOC_TEMP allocates temporary memory that stays alive only inside the block it was allocated in.
/// It will automatically get freed!
/// (Watch out that you dont place ALLOC_TEMP inside an if block and use the memory after the if block.
///
/// eg.
/// float* data;
/// ALLOC_TEMP(data,float,500, kMemSkinning);

#define kMAX_TEMP_STACK_SIZE 2000

// Metrowerks debugger fucks up when we use alloca
#if defined(__MWERKS__) && !UNITY_RELEASE
#undef kMAX_TEMP_STACK_SIZE
#define kMAX_TEMP_STACK_SIZE 0
#endif

inline void* AlignPtr (void* p, size_t alignment)
{
	size_t a = alignment - 1;
	return (void*)(((size_t)p + a) & ~a);
}

struct FreeTempMemory
{
	FreeTempMemory() : m_Memory (NULL) { }
	~FreeTempMemory() { if (m_Memory) UNITY_FREE(kMemTempAlloc, m_Memory); }
	void* m_Memory;
};

#define ALLOC_TEMP_ALIGNED(ptr,type,count,alignment) \
	FreeTempMemory freeTempMemory_##ptr; \
{ \
	size_t allocSize = (count) * sizeof(type) + (alignment)-1; \
	void* allocPtr = NULL; \
	if (allocSize < kMAX_TEMP_STACK_SIZE) { \
	if ((count) != 0) \
	allocPtr = alloca(allocSize); \
	} else { \
	if ((count) != 0) { \
	allocPtr = UNITY_MALLOC_ALIGNED(kMemTempAlloc, allocSize, kDefaultMemoryAlignment); \
	freeTempMemory_##ptr.m_Memory = allocPtr; \
	} \
	} \
	ptr = reinterpret_cast<type*> (AlignPtr(allocPtr, alignment)); \
} \
	ANALYSIS_ASSUME(ptr)

#define ALLOC_TEMP(ptr, type, count) \
	ALLOC_TEMP_ALIGNED(ptr,type,count,kDefaultMemoryAlignment)

#define MALLOC_TEMP(ptr, size) \
	ALLOC_TEMP_ALIGNED(ptr,char,size,kDefaultMemoryAlignment)

#define ALLOC_TEMP_MANUAL(type,count) \
	(type*)UNITY_MALLOC_ALIGNED(kMemTempAlloc, (count) * sizeof (type), kDefaultMemoryAlignment)

#define FREE_TEMP_MANUAL(ptr) \
	UNITY_FREE(kMemTempAlloc, ptr)


#if UNITY_XENON
// Copies a specified number of bytes from a region of cached memory to a region of memory of an unspecified type.
#define UNITY_MEMCPY(dest, src, count) XMemCpyStreaming(dest, src, count)
#else
#define UNITY_MEMCPY(dest, src, count) memcpy(dest, src, count)
#endif


#endif
