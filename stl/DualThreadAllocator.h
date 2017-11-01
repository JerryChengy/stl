#ifndef DUALTHREAD_ALLOCATOR_H_
#define DUALTHREAD_ALLOCATOR_H_

#if ENABLE_MEMORY_MANAGER

#include "BaseAllocator.h"
#include "ThreadSpecificValue.h"

class DelayedPointerDeletionManager;

// Dual Thread Allocator is an indirection to a real allocator

// Has pointer to the main allocator (nonlocking)
// Has pointer to the shared thread allocator (locking)

template <class UnderlyingAllocator>
class DualThreadAllocator : public BaseAllocator
{
public:
	// when constructing it will be from the main thread
	DualThreadAllocator(const char* name, BaseAllocator* mainAllocator, BaseAllocator* threadAllocator);
	virtual ~DualThreadAllocator() {} 

	virtual void* Allocate(size_t size, int align); 
	virtual void* Reallocate (void* p, size_t size, int align);
	virtual void  Deallocate (void* p);
	virtual bool  Contains (const void* p);

	virtual size_t GetAllocatedMemorySize() const;
	virtual size_t GetAllocatorSizeTotalUsed() const;
	virtual size_t GetReservedSizeTotal() const;

	virtual size_t GetPtrSize(const void* ptr) const;
	virtual ProfilerAllocationHeader* GetProfilerHeader(const void* ptr) const;

	virtual void ThreadCleanup();

	virtual bool CheckIntegrity();
	virtual bool ValidatePointer(void* ptr);

	bool TryDeallocate (void* p);

	virtual void FrameMaintenance(bool cleanup);

private:
	UnderlyingAllocator* GetCurrentAllocator();

	UnderlyingAllocator* m_MainAllocator;
	UnderlyingAllocator* m_ThreadAllocator;

	DelayedPointerDeletionManager* m_DelayedDeletion;
};

#endif
#endif
