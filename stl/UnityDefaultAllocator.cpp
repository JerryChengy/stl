#include "UnityPrefix.h"
#include "UnityDefaultAllocator.h"

#if ENABLE_MEMORY_MANAGER

#include "AllocationHeader.h"
#include "MemoryProfiler.h"
#include "BitUtility.h"
#include "MemoryManager.h"

template<class LLAlloctor>
UnityDefaultAllocator<LLAlloctor>::UnityDefaultAllocator(const char* name)
	: BaseAllocator(name)
{
	memset(m_PageAllocationList,0,sizeof(m_PageAllocationList));
}

template<class LLAlloctor>
void* UnityDefaultAllocator<LLAlloctor>::Allocate (size_t size, int align)
{
	size_t realSize = AllocationHeader::CalculateNeededAllocationSize(size, align);
	void* rawPtr = LLAlloctor::Malloc( realSize );
	if(rawPtr == NULL)
		return NULL;

	void* realPtr = AddHeaderAndFooter(rawPtr, size, align);
	RegisterAllocation(realPtr);

	return realPtr;
}

template<class LLAlloctor>
void* UnityDefaultAllocator<LLAlloctor>::Reallocate( void* p, size_t size, int align)
{
	if (p == NULL)
		return Allocate(size, align);

	AllocationHeader::ValidateIntegrity(p, m_AllocatorIdentifier, align);
	RegisterDeallocation(p);

	size_t oldSize = GetPtrSize(p);
	size_t oldPadCount = AllocationHeader::GetHeader(p)->GetPadding();

	void* realPtr = AllocationHeader::GetRealPointer(p);
	size_t realSize = AllocationHeader::CalculateNeededAllocationSize(size, align);
	char* rawPtr = (char*)LLAlloctor::Realloc(realPtr, realSize);
	if(rawPtr == NULL)
		return NULL;

	int newPadCount = AllocationHeader::GetRequiredPadding(rawPtr, align);

	if (newPadCount != oldPadCount){
		// new ptr needs different align padding. move memory and repad
		char* srcptr = rawPtr + AllocationHeader::GetHeaderSize() + oldPadCount;
		char* dstptr = rawPtr + AllocationHeader::GetHeaderSize() + newPadCount;
		memmove(dstptr, srcptr, ( oldSize < size ? oldSize : size ) );
	}

	void* newptr = AddHeaderAndFooter(rawPtr, size, align);
	RegisterAllocation(newptr);

	return newptr;
}

template<class LLAlloctor>
void UnityDefaultAllocator<LLAlloctor>::Deallocate (void* p)
{
	if (p == NULL)
		return;

	AllocationHeader::ValidateIntegrity(p, m_AllocatorIdentifier);
	RegisterDeallocation(p);

	void* realpointer = AllocationHeader::GetRealPointer(p);

	LLAlloctor::Free(realpointer);
}

template<class LLAlloctor>
template<RequestType requestType>
bool UnityDefaultAllocator<LLAlloctor>::AllocationPage(const void* p){

	// A memory and performance optimization could be to register lone pointers in the array instead of setting the bit.
	// when multiple pointers arrive, pull the pointer out, register it, and register the next. Requires some bookkeeping.
	// bottom 2 bits of the pointer can be used for flags.
	int pageAllocationListIndex = 0;
	UInt32 val = (UInt32)p;

	if(sizeof(void*) > sizeof(UInt32))
	{
		Assert(sizeof(void*) == sizeof(UInt64));
		UInt32 highbits = (UInt32)((UInt64)(uintptr_t)(p) >> 32);
		if(highbits != 0)
		{
			pageAllocationListIndex = -1;
			for(int i = 0; i < kNumPageAllocationBlocks; i++)
				if(m_PageAllocationList[i].m_HighBits == highbits)
					pageAllocationListIndex = i;

			if(pageAllocationListIndex == -1)
			{
				// highbits not found in the list. find a free list element
				for(int i = 0; i < kNumPageAllocationBlocks; i++)
				{
					if(m_PageAllocationList[i].m_PageAllocations == NULL)
					{
						m_PageAllocationList[i].m_HighBits = highbits;
						pageAllocationListIndex = i;
						break;
					}
				}
				if(requestType == kTest)
				{
					return false;
				}
				else
				{
					//if(pageAllocationListIndex == -1)
						//ErrorString("Using memoryadresses from more that 16GB of memory");
				}
			}
		}
	}

	int ****& pageAllocations = m_PageAllocationList[pageAllocationListIndex].m_PageAllocations; 
	int page1 = (val >> (32-kPage1Bits)) & ((1<<kPage1Bits)-1);
	int page2 = (val >> (32-kPage1Bits-kPage2Bits)) & ((1<<kPage2Bits)-1);
	int page3 = (val >> (32-kPage1Bits-kPage2Bits-kPage3Bits)) & ((1<<kPage3Bits)-1);
	int page4 = (val >> (32-kPage1Bits-kPage2Bits-kPage3Bits-kPage4Bits)) & ((1<<kPage4Bits)-1);
	int bitindex = (val >> kTargetBitsRepresentedPerBit) & 0x1F;

	if(requestType == kUnregister){
		Assert(pageAllocations != NULL);
		Assert(pageAllocations[page1] != NULL);
		Assert(pageAllocations[page1][page2] != NULL);
		Assert(pageAllocations[page1][page2][page3] != NULL);

		pageAllocations[page1][page2][page3][page4] &= ~(1<<bitindex);
		if(--pageAllocations[page1][page2][page3][(1<<kPage4Bits)] == 0)
		{
			m_BookKeepingMemoryUsage -= ((1<<kPage4Bits)+1)*sizeof(int*);
			MemoryManager::LowLevelFree (pageAllocations[page1][page2][page3]);
			pageAllocations[page1][page2][page3] = NULL;
		}			
		if(--pageAllocations[page1][page2][(1<<kPage3Bits)] == 0)
		{
			m_BookKeepingMemoryUsage -= ((1<<kPage3Bits)+1)*sizeof(int**);
			MemoryManager::LowLevelFree (pageAllocations[page1][page2]);
			pageAllocations[page1][page2] = NULL;
		}
		if(--pageAllocations[page1][(1<<kPage2Bits)] == 0)
		{
			m_BookKeepingMemoryUsage -= ((1<<kPage2Bits)+1)*sizeof(int***);
			MemoryManager::LowLevelFree (pageAllocations[page1]);
			pageAllocations[page1] = NULL; 
		}
		if(--pageAllocations[(1<<kPage1Bits)] == 0)
		{
			m_BookKeepingMemoryUsage -= ((1<<kPage1Bits)+1)*sizeof(int***);
			MemoryManager::LowLevelFree (pageAllocations);
			pageAllocations = NULL; 
		}
		return true;
	}

	if(pageAllocations == NULL)
	{
		if(requestType == kRegister)
		{
			pageAllocations = (int****)MemoryManager::LowLevelCAllocate((1<<kPage1Bits)+1,sizeof(int****));
			m_BookKeepingMemoryUsage += ((1<<kPage1Bits)+1)*sizeof(int****);
			pageAllocations[(1<<kPage1Bits)] = 0;
		}
		else 
			return false;
	}
	if(pageAllocations[page1] == NULL)
	{
		if(requestType == kRegister)
		{
			pageAllocations[page1] = (int***)MemoryManager::LowLevelCAllocate((1<<kPage2Bits)+1,sizeof(int***));
			m_BookKeepingMemoryUsage += ((1<<kPage2Bits)+1)*sizeof(int***);
			pageAllocations[page1][(1<<kPage2Bits)] = 0;
		}
		else 
			return false;
	}
	if(pageAllocations[page1][page2] == NULL)
	{
		if(requestType == kRegister)
		{
			pageAllocations[page1][page2] = (int**)MemoryManager::LowLevelCAllocate((1<<kPage3Bits)+1,sizeof(int**));
			m_BookKeepingMemoryUsage += ((1<<kPage3Bits)+1)*sizeof(int**);
			pageAllocations[page1][page2][(1<<kPage3Bits)] = 0;
		}
		else
			return false;
	}
	if(pageAllocations[page1][page2][page3] == NULL)
	{
		if(requestType == kRegister)
		{
			pageAllocations[page1][page2][page3] = (int*)MemoryManager::LowLevelCAllocate((1<<kPage4Bits)+1,sizeof(int*));
			m_BookKeepingMemoryUsage += ((1<<kPage4Bits)+1)*sizeof(int*);
			pageAllocations[page1][page2][page3][(1<<kPage4Bits)] = 0;
		}
		else
			return false;
	}
	if(requestType == kTest)
		return (pageAllocations[page1][page2][page3][page4] & (1<<bitindex)) != 0;
	pageAllocations[page1][page2][page3][(1<<kPage4Bits)]++;
	pageAllocations[page1][page2][(1<<kPage3Bits)]++;
	pageAllocations[page1][(1<<kPage2Bits)]++;
	pageAllocations[(1<<kPage1Bits)]++;
	Assert((pageAllocations[page1][page2][page3][page4] & (1<<bitindex)) == 0); // the bit for this pointer should not be set yet
	pageAllocations[page1][page2][page3][page4] |= (1<<bitindex);
	return true;

}

template<class LLAlloctor>
void* UnityDefaultAllocator<LLAlloctor>::AddHeaderAndFooter( void* ptr, size_t size, int align ) const
{
	Assert(align >= kDefaultMemoryAlignment && align <= 16*1024 && IsPowerOfTwo(align));
	// calculate required padding for ptr to be aligned after header addition
	// ppppppppHHHH***********
	int padCount = AllocationHeader::GetRequiredPadding(ptr, align);
	void* realPtr = ((char*)ptr) + (padCount + AllocationHeader::GetHeaderSize());
	AllocationHeader::Set(realPtr, m_AllocatorIdentifier, size, padCount, align);
	return realPtr;
}

template<class LLAlloctor>
void UnityDefaultAllocator<LLAlloctor>::RegisterAllocation( const void* p )
{
	Mutex::AutoLock lock(m_AllocLock);
	const size_t ptrSize = GetPtrSize(p);
	const int overheadSize = AllocationHeader::GetHeader(p)->GetOverheadSize();
	RegisterAllocationData(ptrSize, overheadSize);
	m_TotalReservedMemory += ptrSize + overheadSize;
	AllocationPage<kRegister>(p);
}

template<class LLAlloctor>
void UnityDefaultAllocator<LLAlloctor>::RegisterDeallocation( const void* p )
{
	Mutex::AutoLock lock(m_AllocLock);
	const size_t ptrSize = GetPtrSize(p);
	const int overheadSize = AllocationHeader::GetHeader(p)->GetOverheadSize();
	RegisterDeallocationData(ptrSize, overheadSize);
	m_TotalReservedMemory -= ptrSize + overheadSize;
	AllocationPage<kUnregister>(p);
}

template<class LLAlloctor>
bool UnityDefaultAllocator<LLAlloctor>::Contains (const void* p)
{
	Mutex::AutoLock lock(m_AllocLock);
	return AllocationPage<kTest>(p);
}

template<class LLAlloctor>
size_t UnityDefaultAllocator<LLAlloctor>::GetPtrSize( const void* ptr ) const
{
	return AllocationHeader::GetHeader(ptr)->GetRequestedSize();
}

template<class LLAlloctor>
ProfilerAllocationHeader* UnityDefaultAllocator<LLAlloctor>::GetProfilerHeader(const void* ptr) const
{
	// LocalHeader:ProfilerHeader:Data
	return AllocationHeader::GetProfilerHeader(ptr);
}

template<class LLAlloctor>
int UnityDefaultAllocator<LLAlloctor>::GetOverheadSize(void* ptr)
{
	return AllocationHeader::GetHeader(ptr)->GetOverheadSize();
}

template class UnityDefaultAllocator<LowLevelAllocator>;

#endif
