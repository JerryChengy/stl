#ifndef BASE_ALLOCATOR_H_
#define BASE_ALLOCATOR_H_

#if UNITY_OSX || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
#include <stddef.h> // for size_t
#endif
#include "PrefixConfigure.h"
#include "AllocatorLabels.h"

class BaseAllocator
{
public:
	BaseAllocator(const char* name);
	virtual ~BaseAllocator () {}

	virtual void* Allocate (size_t size, int align) = 0;
	virtual void* Reallocate (void* p, size_t size, int align);
	virtual void  Deallocate (void* p) = 0;

	virtual bool  Contains (const void* p) = 0;
	virtual bool  IsAssigned() const { return true; }
	virtual bool  CheckIntegrity() { return true; }
	virtual bool  ValidatePointer(void* /*ptr*/) { return true; }
	// return the actual number of requests bytes
	virtual size_t GetAllocatedMemorySize() const { return m_TotalRequestedBytes; }

	// get total used size (including overhead allocations)
	virtual size_t GetAllocatorSizeTotalUsed() const { return m_TotalRequestedBytes + m_BookKeepingMemoryUsage; }

	// get the reserved size of the allocator (including all overhead memory allocated)
	virtual size_t GetReservedSizeTotal() const { return m_TotalReservedMemory; }

	// get the peak allocated size of the allocator
	virtual size_t GetPeakAllocatedMemorySize() const { return m_PeakRequestedBytes; }

	// return the free block count for each pow2
	virtual void GetFreeBlockCount(int* /*freeCount*/, int /*size*/) { return; }
	// return the used block count for each pow2
	virtual void GetUsedBlockCount(int* /*usedCount*/, int /*size*/) { return; }

	virtual size_t GetPtrSize(const void* /*ptr*/) const {return 0;}
	// return NULL if allocator does not allocate the memory profile header
	virtual ProfilerAllocationHeader* GetProfilerHeader(const void* /*ptr*/) const { return NULL; }

	virtual const char* GetName() const { return m_Name; }

	virtual void ThreadInitialize(BaseAllocator* /*allocator*/) {}
	virtual void ThreadCleanup() {}

	virtual void FrameMaintenance(bool /*cleanup*/) {}

protected:
	void RegisterAllocationData(size_t requestedSize, size_t overhead);
	void RegisterDeallocationData(size_t requestedSize, size_t overhead);

	const char* m_Name;
	UInt32 m_AllocatorIdentifier;
	size_t m_TotalRequestedBytes; // Memory requested by the allocator
	size_t m_TotalReservedMemory; // All memory reserved by the allocator
	size_t m_BookKeepingMemoryUsage; // memory used for bookkeeping (headers etc.)
	size_t m_PeakRequestedBytes; // Memory requested by the allocator
	UInt32 m_NumAllocations; // Allocation count

};

inline void BaseAllocator::RegisterAllocationData(size_t requestedSize, size_t overhead)
{
	m_TotalRequestedBytes += requestedSize;
	m_BookKeepingMemoryUsage += overhead;
	m_PeakRequestedBytes = m_TotalRequestedBytes > m_PeakRequestedBytes ? m_TotalRequestedBytes : m_PeakRequestedBytes;
	m_NumAllocations++;
}

inline void BaseAllocator::RegisterDeallocationData(size_t requestedSize, size_t overhead)
{
	m_TotalRequestedBytes -= requestedSize;
	m_BookKeepingMemoryUsage -= overhead;
	m_NumAllocations--;
}

#endif
