#ifndef STACK_ALLOCATOR_H_
#define STACK_ALLOCATOR_H_

#include "BaseAllocator.h"

class StackAllocator : public BaseAllocator
{
public:
	StackAllocator(int blocksize, const char* name);
	virtual ~StackAllocator ();

	virtual void* Allocate (size_t size, int align);
	virtual void* Reallocate (void* p, size_t size, int align);
	virtual void Deallocate (void* p);
	virtual bool Contains (const void* p);

	virtual size_t GetPtrSize(const void* ptr) const;

	virtual size_t GetAllocatedMemorySize() const;
	virtual size_t GetAllocatorSizeTotalUsed() const;
	virtual size_t GetReservedSizeTotal() const;

private:
	struct Header{
		int deleted:1;
		int size:31;
		char* prevPtr;
		void* realPtr;
	};
	char* m_Block;
	int m_BlockSize;

	char* m_LastAlloc;

	//ThreadID owningThread;
	bool InBlock ( const void* ptr ) const;
	bool IsDeleted ( const void* ptr ) const;
	void SetDeleted ( const void* ptr );
	char* GetPrevAlloc ( const void* ptr ) const;
	void* GetRealPtr( void* ptr ) const;
	char* GetBufferFreePtr() const;

	bool ContainsInternal (const void* p);

	UInt32 GetHeaderSize() const;

	void UpdateNextHeader(void* before, void* after);
};

inline bool StackAllocator::Contains (const void* p) 
{
	// most common case. pointer being queried is the one about to be destroyed
	if(p != NULL && p == m_LastAlloc)
		return true;

	return ContainsInternal(p);
}

inline char* StackAllocator::GetBufferFreePtr() const
{
	if (m_LastAlloc == NULL)
		return m_Block;
	Header* h = ((Header*)m_LastAlloc)-1;
	return m_LastAlloc + h->size;
}

inline size_t StackAllocator::GetPtrSize( const void* ptr ) const
{
	Header* header = ( (Header*)ptr )-1;
	return header->size;
}

inline void* StackAllocator::GetRealPtr( void* ptr ) const
{
	Header* header = ( (Header*)ptr )-1;
	return header->realPtr;
}

inline char* StackAllocator::GetPrevAlloc(const void* ptr ) const
{
	if (ptr == NULL)
		return NULL;
	Header* h = ((Header*)ptr)-1;
	return h->prevPtr;
}

inline UInt32 StackAllocator::GetHeaderSize() const
{
	return sizeof(Header);
}

inline bool StackAllocator::InBlock(const void* ptr) const
{
	return ptr >= m_Block && ptr < (m_Block + m_BlockSize);
}

inline bool StackAllocator::IsDeleted(const void* ptr ) const
{
	if (ptr == NULL)
		return false;
	Header* h = ((Header*)ptr)-1;
	return h->deleted != 0;
}

inline void StackAllocator::SetDeleted(const void* ptr )
{
	if (ptr == NULL)
		return;
	Header* h = ((Header*)ptr)-1;
	h->deleted = 1;
}


#endif
