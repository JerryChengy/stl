#include "UnityPrefix.h"
#include "StackAllocator.h"
#include "LogAssert.h"
#include "MemoryManager.h"
#include "MemoryProfiler.h"

StackAllocator::StackAllocator(int blockSize, const char* name)
	: BaseAllocator(name)
	, m_Block(NULL)
	, m_LastAlloc(NULL)
	, m_BlockSize(blockSize)
{
#if ENABLE_MEMORY_MANAGER
	m_Block = (char*)MemoryManager::LowLevelAllocate(m_BlockSize);
#else
	m_Block = (char*)malloc(m_BlockSize);
#endif

	m_TotalReservedMemory = blockSize;
}

StackAllocator::~StackAllocator()
{
	while(m_LastAlloc)
		Deallocate(m_LastAlloc);

#if ENABLE_MEMORY_MANAGER
	MemoryManager::LowLevelFree(m_Block);
#else
	free(m_Block);
#endif
}

void* StackAllocator::Allocate (size_t size, int align)
{
	//1 byte alignment doesn't work for webgl; this is a fix(ish)....

#if UNITY_WEBGL || UNITY_BB10
	if(align % 8 != 0)
		align = 8;
#endif

	size_t alignmask = align - 1;

	// make header size a multiple
	int alignedHeaderSize = (GetHeaderSize() + alignmask) & ~alignmask; 
	int paddedSize = (size + alignedHeaderSize + alignmask) & ~alignmask;

	char* realPtr;

	char* freePtr = (char*)AlignPtr(GetBufferFreePtr(), align);
	size_t freeSize = m_BlockSize - (int)(freePtr - m_Block);

	if ( InBlock(freePtr) && paddedSize < freeSize )
	{
		realPtr = freePtr;
	}
	else
	{
		// Spilled over. We have to allocate the memory default alloc
		realPtr = (char*)UNITY_MALLOC_ALIGNED(kMemTempOverflow, paddedSize, align);
	}
	if(realPtr == NULL)
		return NULL;

	char* ptr = realPtr + alignedHeaderSize;
	Header* h = ( (Header*)ptr )-1;
	h->prevPtr = m_LastAlloc;
	h->size = size;
	h->deleted = 0;
	h->realPtr = realPtr;
	m_LastAlloc = ptr;

	return ptr;
}

void* StackAllocator::Reallocate (void* p, size_t size, int align)
{
	if (p == NULL)
		return Allocate(size, align);

	char* freePtr = (char*)AlignPtr(GetBufferFreePtr(), align);
	size_t freeSize = m_BlockSize - (int)(freePtr - m_Block);
	size_t oldSize = GetPtrSize(p);

	if ((p == m_LastAlloc || oldSize >= size) && InBlock(p) 
		&& AlignPtr(p,align) == p 
		&& oldSize + freeSize > size)
	{
		// just expand the top allocation of the stack to the realloc amount
		Header* h = ( (Header*)p )-1;
		h->size = size;
		return p;
	}
	void* newPtr = NULL;
	if (!InBlock(p))
	{
		size_t alignmask = align - 1;
		int alignedHeaderSize = (GetHeaderSize() + alignmask) & ~alignmask; 
		int paddedSize = (size + alignedHeaderSize + alignmask) & ~alignmask;

		char* realPtr = (char*)UNITY_REALLOC_ALIGNED(kMemTempOverflow, GetRealPtr(p), paddedSize, align);
		if(realPtr == NULL)
			return NULL;

		newPtr = realPtr + alignedHeaderSize;
		Header* h = ( (Header*)newPtr ) - 1;
		h->size = size;
		h->deleted = 0;
		h->realPtr = realPtr;

		if(m_LastAlloc == p)
			m_LastAlloc = (char*)newPtr;
		else
			UpdateNextHeader(p, newPtr);
	}
	else
	{
		newPtr = Allocate(size, align);
		if(newPtr != NULL)
			memcpy(newPtr, p, std::min(size, oldSize));
		Deallocate(p);
	}
	return newPtr;
}

void StackAllocator::Deallocate (void* p)
{
	if (p == m_LastAlloc){
		m_LastAlloc = GetPrevAlloc(p);
		if ( !InBlock(p) )
		{
			UNITY_FREE(kMemTempOverflow, GetRealPtr(p));
		}

		if (IsDeleted(m_LastAlloc))
			Deallocate(m_LastAlloc);
	}
	else
	{
		SetDeleted(p);
	}
}

bool StackAllocator::ContainsInternal (const void* p) 
{
	// if no temp allocations (should hit here most often)
	if (m_LastAlloc == NULL)
		return false;

	// test inblock
	if (InBlock(p))
		return true;

	// test overflow allocations (should almost never happen)
	void* ptr = m_LastAlloc;
	while (ptr != NULL && !InBlock(ptr))
	{
		if (p == ptr)
			return true;
		ptr = GetPrevAlloc(ptr);
	}
	return false;
}

void StackAllocator::UpdateNextHeader(void* before, void* after)
{
	if (before == m_LastAlloc)
		return;
	void* ptr = m_LastAlloc;
	while (ptr != NULL && !InBlock(ptr))
	{
		void* prevAlloc = GetPrevAlloc(ptr);
		if (before == prevAlloc)
		{
			Header* h = ((Header*)ptr)-1;
			h->prevPtr = (char*)after;
			return;
		}
		ptr = prevAlloc;
	}
	FatalErrorString("Allocation no found in temp allocation list");
}


size_t StackAllocator::GetAllocatedMemorySize() const
{
	int total = 0;
	void* ptr = m_LastAlloc;
	while (ptr != NULL)
	{
		total += GetPtrSize(ptr);
		ptr = GetPrevAlloc(ptr);
	}
	return total;
}

size_t StackAllocator::GetAllocatorSizeTotalUsed() const
{
	int total = 0;
	void* ptr = m_LastAlloc;
	while (ptr != NULL)
	{
		total += GetPtrSize(ptr)+GetHeaderSize();
		ptr = GetPrevAlloc(ptr);
	}
	return total;
}

size_t StackAllocator::GetReservedSizeTotal() const
{
	return m_TotalReservedMemory;
}
