#ifndef DYNAMIC_HEAP_ALLOCATOR_H_
#define DYNAMIC_HEAP_ALLOCATOR_H_

#if ENABLE_MEMORY_MANAGER

#include "BaseAllocator.h"
#include "Mutex.h"
#include "LowLevelDefaultAllocator.h"
#include "LinkedList.h"

template<class LLAllocator>
class DynamicHeapAllocator : public BaseAllocator
{
public:

	DynamicHeapAllocator( UInt32 poolIncrementSize, size_t splitLimit, bool useLocking, const char* name);
	~DynamicHeapAllocator();

	virtual void* Allocate (size_t size, int align);
	virtual void* Reallocate (void* p, size_t size, int align);
	virtual void Deallocate (void* p);
	virtual bool Contains (const void* p);

	virtual bool CheckIntegrity();
	virtual bool ValidatePointer(void* ptr);

	virtual size_t GetPtrSize(const void* ptr) const;

	virtual ProfilerAllocationHeader* GetProfilerHeader(const void* ptr) const;

	// return the free block count for each pow2
	virtual void GetFreeBlockCount(int* freeCount, int size);
	// return the used block count for each pow2
	virtual void GetUsedBlockCount(int* usedCount, int size);

private:
	struct PoolElement : public ListElement
	{
		bool Contains(const void* ptr) const
		{
			return ptr >= memoryBase && ptr < memoryBase + memorySize;
		}
		void* tlsfPool;
		char* memoryBase;
		UInt32 memorySize;
		UInt32 allocationCount;
		UInt32 allocationSize;
	};

	typedef List<PoolElement> PoolList;

	size_t m_SplitLimit; 
	PoolElement& GetActivePool(size_t size);
	PoolList& GetPoolList(size_t size);

	PoolList m_SmallTLSFPools;
	PoolList m_LargeTLSFPools;

	Mutex m_DHAMutex;
	bool m_UseLocking;
	size_t m_RequestedPoolSize;

	struct LargeAllocations
	{
		LargeAllocations* next;
		char* allocation;
		void* returnedPtr;
		size_t size;
	};
	LargeAllocations* m_FirstLargeAllocation;

	PoolElement* FindPoolFromPtr(const void* ptr);

	void RegisterAllocation(const void* p);
	void RegisterDeallocation(const void* p);

	void* AddHeaderAndFooter( void* ptr, size_t size, int align ) const;
};

#endif
#endif
