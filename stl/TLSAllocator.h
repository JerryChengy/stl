#ifndef TLS_ALLOCATOR_H_
#define TLS_ALLOCATOR_H_

#if ENABLE_MEMORY_MANAGER

#include "BaseAllocator.h"
#include "ThreadSpecificValue.h"


// TLS Allocator is an indirection to a real allocator
// Has a tls value pointing to the threadspecific allocator if unique per thread.

template <class UnderlyingAllocator>
class TLSAllocator : public BaseAllocator
{
public:
	// when constructing it will be from the main thread
	TLSAllocator(const char* name);
	virtual ~TLSAllocator(); 

	virtual void* Allocate(size_t size, int align); 
	virtual void* Reallocate (void* p, size_t size, int align);
	virtual void  Deallocate (void* p);
	virtual bool  Contains (const void* p);

	virtual size_t GetAllocatedMemorySize() const;
	virtual size_t GetAllocatorSizeTotalUsed() const;
	virtual size_t GetReservedSizeTotal() const;

	virtual size_t GetPtrSize(const void* ptr) const;
	virtual ProfilerAllocationHeader* GetProfilerHeader(const void* ptr) const;

	virtual void ThreadInitialize(BaseAllocator* allocator);
	virtual void ThreadCleanup();

	virtual bool CheckIntegrity();

	virtual bool IsAssigned() const;
	bool TryDeallocate (void* p);

	UnderlyingAllocator* GetCurrentAllocator();
	virtual void FrameMaintenance(bool cleanup);

private:
	// because TLS values have to be static on some platforms, this is made static
	// and only one instance of the TLS is allowed 
	static UNITY_TLS_VALUE(UnderlyingAllocator*) m_UniqueThreadAllocator; // the memorymanager holds the list of allocators
	static int s_NumberOfInstances;

	static const int      kMaxThreadTempAllocators = 128;
	UnderlyingAllocator*  m_ThreadTempAllocators[kMaxThreadTempAllocators];
};

#endif
#endif
