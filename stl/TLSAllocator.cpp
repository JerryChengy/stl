#include "UnityPrefix.h"
#include "TLSAllocator.h"
#include "Thread.h"
#include "StackAllocator.h"

#if ENABLE_MEMORY_MANAGER

template <class UnderlyingAllocator>
int TLSAllocator<UnderlyingAllocator>::s_NumberOfInstances = 0;

template <class UnderlyingAllocator>
UNITY_TLS_VALUE(UnderlyingAllocator*) TLSAllocator<UnderlyingAllocator>::m_UniqueThreadAllocator;

template <class UnderlyingAllocator>
TLSAllocator<UnderlyingAllocator>::TLSAllocator(const char* name)
	: BaseAllocator(name)
{
	if(s_NumberOfInstances != 0)
		ErrorString("Only one instance of the TLS allocator is allowed because of TLS implementation");
	s_NumberOfInstances++;
	memset (m_ThreadTempAllocators, 0, sizeof(m_ThreadTempAllocators));
}

template <class UnderlyingAllocator>
TLSAllocator<UnderlyingAllocator>::~TLSAllocator()
{
	s_NumberOfInstances--;
}

template <class UnderlyingAllocator>
void TLSAllocator<UnderlyingAllocator>::ThreadInitialize(BaseAllocator *allocator)
{
	m_UniqueThreadAllocator = (UnderlyingAllocator*)allocator;

	for(int i = 0; i < kMaxThreadTempAllocators; i++)
	{
		if(m_ThreadTempAllocators[i] == NULL)
		{
			m_ThreadTempAllocators[i] = (UnderlyingAllocator*) allocator;
			break;
		}
	}

}

template <class UnderlyingAllocator>
void TLSAllocator<UnderlyingAllocator>::ThreadCleanup()
{
	UnderlyingAllocator* allocator = m_UniqueThreadAllocator;
	m_UniqueThreadAllocator = NULL;

	for(int i = 0; i < kMaxThreadTempAllocators; i++)
	{
		if(m_ThreadTempAllocators[i] == allocator)
		{
			m_ThreadTempAllocators[i] = NULL;
			break;
		}
	}
	UNITY_DELETE(allocator, kMemManager);
}

template <class UnderlyingAllocator>
void TLSAllocator<UnderlyingAllocator>::FrameMaintenance(bool cleanup)
{
	Assert(m_UniqueThreadAllocator->GetAllocatedMemorySize() == 0);
}

template <class UnderlyingAllocator>
bool TLSAllocator<UnderlyingAllocator>::IsAssigned() const
{
	return m_UniqueThreadAllocator != NULL;
}

template <class UnderlyingAllocator>
UnderlyingAllocator* TLSAllocator<UnderlyingAllocator>::GetCurrentAllocator()
{
	return m_UniqueThreadAllocator;
}


template <class UnderlyingAllocator>
void* TLSAllocator<UnderlyingAllocator>::Allocate( size_t size, int align )
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();
	return alloc ? alloc->UnderlyingAllocator::Allocate(size, align) : NULL;
}

template <class UnderlyingAllocator>
void* TLSAllocator<UnderlyingAllocator>::Reallocate( void* p, size_t size, int align )
{ 
	UnderlyingAllocator* alloc = GetCurrentAllocator();
	if(!alloc)
		return NULL;
	if(alloc->UnderlyingAllocator::Contains(p))
		return alloc->UnderlyingAllocator::Reallocate(p, size, align);

	return NULL;
}

template <class UnderlyingAllocator>
void TLSAllocator<UnderlyingAllocator>::Deallocate( void* p )
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();
	DebugAssert(alloc);
	DebugAssert(alloc->UnderlyingAllocator::Contains(p));
	return alloc->UnderlyingAllocator::Deallocate(p);
}

template <class UnderlyingAllocator>
bool TLSAllocator<UnderlyingAllocator>::TryDeallocate( void* p )
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();
	if(!alloc)
		return false;

	if(!alloc->UnderlyingAllocator::Contains(p))
		return false;

	alloc->UnderlyingAllocator::Deallocate(p);
	return true;
}


template <class UnderlyingAllocator>
bool TLSAllocator<UnderlyingAllocator>::Contains( const void* p )
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();
	if(alloc && alloc->UnderlyingAllocator::Contains(p))
		return true;
	return false;
}

template <class UnderlyingAllocator>
size_t TLSAllocator<UnderlyingAllocator>::GetAllocatedMemorySize( ) const
{
	size_t allocated = 0;
	for(int i = 0; i < kMaxThreadTempAllocators; i++)
	{
		if(m_ThreadTempAllocators[i] != NULL)
			allocated += m_ThreadTempAllocators[i]->UnderlyingAllocator::GetAllocatedMemorySize();
	}
	return allocated;
}

template <class UnderlyingAllocator>
size_t TLSAllocator<UnderlyingAllocator>::GetAllocatorSizeTotalUsed() const
{
	size_t total = 0;
	for(int i = 0; i < kMaxThreadTempAllocators; i++)
	{
		if(m_ThreadTempAllocators[i] != NULL)
			total += m_ThreadTempAllocators[i]->UnderlyingAllocator::GetAllocatorSizeTotalUsed();
	}
	return total;
}

template <class UnderlyingAllocator>
size_t TLSAllocator<UnderlyingAllocator>::GetReservedSizeTotal() const
{
	size_t total = 0;
	for(int i = 0; i < kMaxThreadTempAllocators; i++)
	{
		if(m_ThreadTempAllocators[i] != NULL)
			total += m_ThreadTempAllocators[i]->UnderlyingAllocator::GetReservedSizeTotal();
	}
	return total;
}

template <class UnderlyingAllocator>
size_t TLSAllocator<UnderlyingAllocator>::GetPtrSize( const void* ptr ) const
{
	// all allocators have the same allocation header
	return m_ThreadTempAllocators[0]->UnderlyingAllocator::GetPtrSize(ptr);
}

template <class UnderlyingAllocator>
ProfilerAllocationHeader* TLSAllocator<UnderlyingAllocator>::GetProfilerHeader( const void* ptr ) const
{
	return m_ThreadTempAllocators[0]->UnderlyingAllocator::GetProfilerHeader(ptr);
}


template <class UnderlyingAllocator>
bool TLSAllocator<UnderlyingAllocator>::CheckIntegrity()
{
	bool succes = true;
	for(int i = 0; i < kMaxThreadTempAllocators; i++)
	{
		if(m_ThreadTempAllocators[i] != NULL)
			succes &= m_ThreadTempAllocators[i]->UnderlyingAllocator::CheckIntegrity();
	}
	return succes;
}

template class TLSAllocator< StackAllocator >;

#endif // #if ENABLE_MEMORY_MANAGER
