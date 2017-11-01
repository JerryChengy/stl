#include "UnityPrefix.h"
#include "DynamicHeapAllocator.h"
#if ENABLE_MEMORY_MANAGER

#include "AllocationHeader.h"
#include "tlsf.h"
#include "BitUtility.h"
#include "MemoryProfiler.h"
#if UNITY_XENON
#include "PlatformDependent/Xbox360/Source/XenonMemory.h"
#endif
#include "Thread.h"
#include "AtomicOps.h"

template<class LLAllocator>
DynamicHeapAllocator<LLAllocator>::DynamicHeapAllocator(UInt32 poolIncrementSize, size_t splitLimit, bool useLocking, const char* name)
	: BaseAllocator(name), m_UseLocking(useLocking)
{
	m_SplitLimit = splitLimit;
	m_RequestedPoolSize = poolIncrementSize;
	m_FirstLargeAllocation = NULL;
}

template<class LLAllocator>
DynamicHeapAllocator<LLAllocator>::~DynamicHeapAllocator()
{
	Mutex::AutoLock m(m_DHAMutex);

	for(ListIterator<PoolElement> i=m_SmallTLSFPools.begin();i != m_SmallTLSFPools.end();i++)
	{
		PoolElement& pool = *i;
		tlsf_destroy(pool.tlsfPool);
		LLAllocator::Free(pool.memoryBase);
	}
	for(ListIterator<PoolElement> i=m_LargeTLSFPools.begin();i != m_LargeTLSFPools.end();i++)
	{
		PoolElement& pool = *i;
		tlsf_destroy(pool.tlsfPool);
		LLAllocator::Free(pool.memoryBase);
	}
}

template<class LLAllocator>
typename DynamicHeapAllocator<LLAllocator>::PoolElement& DynamicHeapAllocator<LLAllocator>::GetActivePool( size_t size )
{
	return GetPoolList( size ).front();
}

template<class LLAllocator>
typename DynamicHeapAllocator<LLAllocator>::PoolList& DynamicHeapAllocator<LLAllocator>::GetPoolList( size_t size )
{
	return size < m_SplitLimit? m_SmallTLSFPools: m_LargeTLSFPools;
}

template<class LLAllocator>
void* DynamicHeapAllocator<LLAllocator>::Allocate(size_t size, int align)
{
	if(m_UseLocking)
		m_DHAMutex.Lock();

	DebugAssert(align > 0 && align <= 16*1024 && IsPowerOfTwo(align));

	size_t realSize =  AllocationHeader::CalculateNeededAllocationSize(size, align);;

	/// align size to tlsf block requirements
	if(realSize > 32)
	{
		int tlsfalign = (1 << HighestBit(realSize >> 5))-1;
		realSize = (realSize + tlsfalign) & ~tlsfalign;
	}

	char* ptr = NULL;
	if(!GetPoolList(realSize).empty())
		ptr = (char*)tlsf_malloc(GetActivePool(realSize).tlsfPool, realSize);
	LargeAllocations* largeAlloc = NULL;
	if(ptr == NULL)
	{
		// only try to make new tlsfBlocks if the amount is less than a 16th of the blocksize - else spill to LargeAllocations
		if(size < m_RequestedPoolSize/4)
		{
			// not enough space in the current block.
			// Iterate from the back, and find one that fits the allocation
			// put the found block at the head of the list
			PoolList& poolList = GetPoolList(realSize);
			ListIterator<PoolElement> pool = poolList.end();
			--pool;
			while(pool != poolList.end()) // List wraps around so end is the element just before the head of the list
			{
				ptr = (char*)tlsf_malloc(pool->tlsfPool, realSize);
				if(ptr != NULL)
				{
					// push_front removes the node from the list and reinserts it at the start
					Mutex::AutoLock m(m_DHAMutex);
					poolList.push_front(*pool);
					break;
				}
				--pool;
			}

			if(ptr == 0)
			{
				int allocatePoolSize = m_RequestedPoolSize;
				void* memoryBlock = NULL;

#if UNITY_ANDROID
				// HACK:
				// on android we reload libunity (and keep activity around)
				// this results in leaks, as MemoryManager::m_InitialFallbackAllocator is never freed actually
				// more to it: even if we free the mem - the hole left is taken by other mallocs
				// so we cant reuse it on re-init, effectively allocing anew
				// this hack can be removed when unity can be cleanly reloaded (sweet dreams)
				static const int _InitialFallbackAllocMemBlock_Size = 1024*1024;
				static bool _InitialFallbackAllocMemBlock_Taken = false;
				static char _InitialFallbackAllocMemBlock[_InitialFallbackAllocMemBlock_Size];

				if(!_InitialFallbackAllocMemBlock_Taken)
				{
					Assert(_InitialFallbackAllocMemBlock_Size == m_RequestedPoolSize);
					_InitialFallbackAllocMemBlock_Taken = true;
					memoryBlock = _InitialFallbackAllocMemBlock;
				}
#endif

				while(!memoryBlock && allocatePoolSize > size*2)
				{
					memoryBlock = LLAllocator::Malloc(allocatePoolSize);
					if(!memoryBlock)
						allocatePoolSize /= 2;
				}

				if(memoryBlock)
				{
					m_TotalReservedMemory += allocatePoolSize;
					PoolElement* newPoolPtr = (PoolElement*)LLAllocator::Malloc(sizeof(PoolElement));
					PoolElement& newPool = *new (newPoolPtr) PoolElement();
					newPool.memoryBase = (char*)memoryBlock;
					newPool.memorySize = allocatePoolSize;
					newPool.tlsfPool = tlsf_create(memoryBlock, allocatePoolSize);
					newPool.allocationCount = 0;
					newPool.allocationSize = 0;

					{
						Mutex::AutoLock lock(m_DHAMutex);
						poolList.push_front(newPool);
					}

					ptr = (char*)tlsf_malloc(GetActivePool(realSize).tlsfPool, realSize);
				}
			}
		}
		if(ptr == 0)
		{
			// large allocation that don't fit on a clean block
			largeAlloc = (LargeAllocations*)LLAllocator::Malloc(sizeof(LargeAllocations));
			largeAlloc->allocation = (char*)LLAllocator::Malloc(realSize);
			if(largeAlloc->allocation == NULL)
			{
				printf_console("DynamicHeapAllocator out of memory - Could not get memory for large allocation");
				if(m_UseLocking)
					m_DHAMutex.Unlock();
				return NULL;
			}
			largeAlloc->next = m_FirstLargeAllocation;
			largeAlloc->size = size;
			m_TotalReservedMemory += size;
			{
				Mutex::AutoLock lock(m_DHAMutex);
				m_FirstLargeAllocation = largeAlloc;
			}
			ptr = largeAlloc->allocation;
		}
	}

	if(!largeAlloc)
	{
		GetActivePool(realSize).allocationCount++;
		GetActivePool(realSize).allocationSize+=size;
	}

	void* realPtr = AddHeaderAndFooter(ptr, size, align);
	RegisterAllocation(realPtr);

	if (largeAlloc)
		largeAlloc->returnedPtr = realPtr;

	if(m_UseLocking)
		m_DHAMutex.Unlock();

	return realPtr;
}

template<class LLAllocator>
void* DynamicHeapAllocator<LLAllocator>::Reallocate (void* p, size_t size, int align)
{
	if (p == NULL)
		return Allocate(size, align);

	if(m_UseLocking)
		m_DHAMutex.Lock();

	AllocationHeader::ValidateIntegrity(p, m_AllocatorIdentifier, align);
	RegisterDeallocation(p);

	size_t oldSize = GetPtrSize(p);
	size_t oldPadCount = AllocationHeader::GetHeader(p)->GetPadding();

	void* realPtr = AllocationHeader::GetRealPointer(p);
	size_t realSize = AllocationHeader::CalculateNeededAllocationSize(size, align);

	PoolElement* allocedPool = FindPoolFromPtr(realPtr);
	char* realNewPtr = NULL;
	if(allocedPool != NULL)
	{
		realNewPtr = (char*)tlsf_realloc(allocedPool->tlsfPool, realPtr, realSize);
	}

	if(realNewPtr == NULL)
	{
		// if we didn't succeed doing a tlsf reallocation, just allocate, copy and delete
		RegisterAllocation(p); // Reregister the allocation again
		if(m_UseLocking)
			m_DHAMutex.Unlock();
		void* newPtr = Allocate(size, align);
		if (newPtr != NULL)
			memcpy(newPtr, p, ( oldSize < size ? oldSize : size ));
		Deallocate(p);
		return newPtr;
	}

	int newPadCount = AllocationHeader::GetRequiredPadding(realNewPtr, align);

	if (newPadCount != oldPadCount){
		// new ptr needs different align padding. move memory and repad
		char* srcptr = realNewPtr + AllocationHeader::GetHeaderSize() + oldPadCount;
		char* dstptr = realNewPtr + AllocationHeader::GetHeaderSize() + newPadCount;
		memmove(dstptr, srcptr, ( oldSize < size ? oldSize : size ) );
	}
	void* newptr = AddHeaderAndFooter(realNewPtr, size, align);
	RegisterAllocation(newptr);

	if(m_UseLocking)
		m_DHAMutex.Unlock();
	return newptr;
}

template<class LLAllocator>
void DynamicHeapAllocator<LLAllocator>::Deallocate (void* p)
{
	if (p == NULL)
		return;

	if(m_UseLocking)
		m_DHAMutex.Lock();

	AllocationHeader::ValidateIntegrity(p, m_AllocatorIdentifier);
	RegisterDeallocation(p);

	void* realpointer = AllocationHeader::GetRealPointer(p);

	PoolElement* allocedPool = FindPoolFromPtr(realpointer);
	if(allocedPool != NULL)
	{
		allocedPool->allocationCount--;
		allocedPool->allocationSize-=GetPtrSize(p);
		tlsf_free(allocedPool->tlsfPool, realpointer);
		if(allocedPool->allocationCount == 0)
		{
			{
				Mutex::AutoLock lock(m_DHAMutex);
				allocedPool->RemoveFromList();
			}
			tlsf_destroy(allocedPool->tlsfPool);
			LLAllocator::Free(allocedPool->memoryBase);
			m_TotalReservedMemory -= allocedPool->memorySize;
			allocedPool->~PoolElement();
			LLAllocator::Free(allocedPool);
		}
	}
	else
	{
		// is this a largeAllocation
		LargeAllocations* alloc = m_FirstLargeAllocation;
		LargeAllocations* prev = NULL;
		while (alloc != NULL)
		{
			if (alloc->allocation == realpointer)
			{
				LLAllocator::Free(realpointer);
				m_TotalReservedMemory -= alloc->size;
				alloc->allocation = NULL;
				alloc->size = 0;
				{
					Mutex::AutoLock lock(m_DHAMutex);
					if(prev == NULL)
						m_FirstLargeAllocation = alloc->next;
					else
						prev->next = alloc->next;
				}
				LLAllocator::Free(alloc);
				if(m_UseLocking)
					m_DHAMutex.Unlock();
				return;
			}
			prev = alloc;
			alloc = alloc->next;
		}
		ErrorString("Could not find reallocpointer in LargeAllocationlist");
	}
	if(m_UseLocking)
		m_DHAMutex.Unlock();
}

template<class LLAllocator>
bool DynamicHeapAllocator<LLAllocator>::ValidatePointer(void* ptr)
{
	AllocationHeader::ValidateIntegrity(ptr, -1, -1);
	return true;
}

void AllocBlockValidate (void* memptr, size_t /*size*/, int isused, void* /*userptr*/)
{
	if (isused )
		AllocationHeader::ValidateIntegrity(((char*)AllocationHeader::GetHeaderFromRealPointer(memptr))+AllocationHeader::GetHeaderSize(),-1,-1);
}

template<class LLAllocator>
bool DynamicHeapAllocator<LLAllocator>::CheckIntegrity()
{
	Mutex::AutoLock m(m_DHAMutex);
	for(ListIterator<PoolElement> i=m_SmallTLSFPools.begin();i != m_SmallTLSFPools.end();i++)
		tlsf_check_heap(i->tlsfPool);
	for(ListIterator<PoolElement> i=m_LargeTLSFPools.begin();i != m_LargeTLSFPools.end();i++)
		tlsf_check_heap(i->tlsfPool);

	for(ListIterator<PoolElement> i=m_SmallTLSFPools.begin();i != m_SmallTLSFPools.end();i++)
		tlsf_walk_heap (i->tlsfPool, &AllocBlockValidate, NULL);
	for(ListIterator<PoolElement> i=m_LargeTLSFPools.begin();i != m_LargeTLSFPools.end();i++)
		tlsf_walk_heap (i->tlsfPool, &AllocBlockValidate, NULL);

	return true;
}

struct BlockCounter
{
	int* blockCount;
	int size;
};

void FreeBlockCount (void* /*memptr*/, size_t size, int isused, void* userptr)
{
	BlockCounter* counter = (BlockCounter*)userptr;
	if (!isused )
	{
		int index = HighestBit(size);
		index = index >= counter->size ? counter->size-1 : index;
		counter->blockCount[index]++;
	}
}

template<class LLAllocator>
void DynamicHeapAllocator<LLAllocator>::GetFreeBlockCount( int* freeCount, int size )
{
	Mutex::AutoLock m(m_DHAMutex);
	memset(freeCount, 0, size*sizeof(int));
	BlockCounter counter = {freeCount, size };
	for(ListIterator<PoolElement> i=m_SmallTLSFPools.begin();i != m_SmallTLSFPools.end();i++)
		tlsf_walk_heap (i->tlsfPool, &FreeBlockCount, &counter);
	for(ListIterator<PoolElement> i=m_LargeTLSFPools.begin();i != m_LargeTLSFPools.end();i++)
		tlsf_walk_heap (i->tlsfPool, &FreeBlockCount, &counter);
}

void UsedBlockCount (void* /*memptr*/, size_t size, int isused, void* userptr)
{
	BlockCounter* counter = (BlockCounter*)userptr;
	if (isused )
	{
		int index = HighestBit(size);
		index = index >= counter->size ? counter->size-1 : index;
		counter->blockCount[index]++;
	}
}

template<class LLAllocator>
void DynamicHeapAllocator<LLAllocator>::GetUsedBlockCount( int* usedCount, int size )
{
	Mutex::AutoLock m(m_DHAMutex);
	BlockCounter counter = {usedCount, size };
	for(ListIterator<PoolElement> i=m_SmallTLSFPools.begin();i != m_SmallTLSFPools.end();i++)
		tlsf_walk_heap (i->tlsfPool, &UsedBlockCount, &counter);
	for(ListIterator<PoolElement> i=m_LargeTLSFPools.begin();i != m_LargeTLSFPools.end();i++)
		tlsf_walk_heap (i->tlsfPool, &UsedBlockCount, &counter);
}

template<class LLAllocator>
typename DynamicHeapAllocator<LLAllocator>::PoolElement* DynamicHeapAllocator<LLAllocator>::FindPoolFromPtr( const void* ptr )
{
	for(ListIterator<PoolElement> i=m_SmallTLSFPools.begin();i != m_SmallTLSFPools.end();i++)
	{
		if (i->Contains(ptr))
			return &*i;
	}
	for(ListIterator<PoolElement> i=m_LargeTLSFPools.begin();i != m_LargeTLSFPools.end();i++)
	{
		if (i->Contains(ptr))
			return &*i;
	}
	return NULL;
}


template<class LLAlloctor>
bool DynamicHeapAllocator<LLAlloctor>::Contains (const void* p)
{
	bool useLocking = m_UseLocking || !Thread::CurrentThreadIsMainThread();
	if(useLocking)
		m_DHAMutex.Lock();

	if(FindPoolFromPtr(p) != NULL)
	{
		if(useLocking)
			m_DHAMutex.Unlock();
		return true;
	}

	// is this a largeAllocation
	LargeAllocations* alloc = m_FirstLargeAllocation;
	while (alloc != NULL)
	{
		if (alloc->returnedPtr == p)
		{
			if(useLocking)
				m_DHAMutex.Unlock();
			return true;
		}
		alloc = alloc->next;
	}
	if(useLocking)
		m_DHAMutex.Unlock();
	return false;

}

template<class LLAlloctor>
void* DynamicHeapAllocator<LLAlloctor>::AddHeaderAndFooter( void* ptr, size_t size, int align ) const
{
	DebugAssert(align >= kDefaultMemoryAlignment && align <= 16*1024 && IsPowerOfTwo(align));
	// calculate required padding for ptr to be aligned after header addition
	// ppppppppHHHH***********
	int padCount = AllocationHeader::GetRequiredPadding(ptr, align);
	void* realPtr = ((char*)ptr) + (padCount + AllocationHeader::GetHeaderSize());
	AllocationHeader::Set(realPtr, m_AllocatorIdentifier, size, padCount, align);
	return realPtr;
}

template<class LLAlloctor>
void DynamicHeapAllocator<LLAlloctor>::RegisterAllocation( const void* p )
{
	RegisterAllocationData(GetPtrSize(p), AllocationHeader::GetHeader(p)->GetOverheadSize());
}

template<class LLAlloctor>
void DynamicHeapAllocator<LLAlloctor>::RegisterDeallocation( const void* p )
{
	RegisterDeallocationData(GetPtrSize(p), AllocationHeader::GetHeader(p)->GetOverheadSize());
}

template<class LLAlloctor>
size_t DynamicHeapAllocator<LLAlloctor>::GetPtrSize( const void* ptr ) const
{
	return AllocationHeader::GetHeader(ptr)->GetRequestedSize();
}

template<class LLAlloctor>
ProfilerAllocationHeader* DynamicHeapAllocator<LLAlloctor>::GetProfilerHeader(const void* ptr) const
{
	// LocalHeader:ProfilerHeader:Data
	return AllocationHeader::GetProfilerHeader(ptr);
}

template class DynamicHeapAllocator<LowLevelAllocator>;

#if UNITY_XENON && XBOX_USE_DEBUG_MEMORY
template class DynamicHeapAllocator<LowLevelAllocatorDebugMem>;
#endif
#endif

