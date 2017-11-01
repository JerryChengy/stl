#include "UnityPrefix.h"
#include "DualThreadAllocator.h"
#include "Thread.h"
#include "DynamicHeapAllocator.h"

#if ENABLE_MEMORY_MANAGER

class DelayedPointerDeletionManager
{
public:
	DelayedPointerDeletionManager(BaseAllocator* mainAlloc, BaseAllocator* threadAlloc) : 
	  m_HasPendingDeletes (0), 
		  m_MainAlloctor (mainAlloc),
		  m_ThreadAlloctor (threadAlloc),
		  m_MainThreadPendingPointers(NULL),
		  m_MainThreadPendingCount (0),
		  m_MainThreadPendingReserved (0) {}
	  ~DelayedPointerDeletionManager(){ CleanupPendingMainThreadPointers(); }

	  bool HasPending() {return m_HasPendingDeletes != 0;}
	  void AddPointerToMainThreadDealloc( void* ptr) ;
	  void CleanupPendingMainThreadPointers() ;
	  void DeallocateLocalMemory();
private:
	void GrowPendingBuffer();
	void** m_MainThreadPendingPointers;
	UInt32 m_MainThreadPendingCount;
	UInt32 m_MainThreadPendingReserved;

	volatile int m_HasPendingDeletes;
	BaseAllocator* m_MainAlloctor;
	BaseAllocator* m_ThreadAlloctor;
	Mutex m_MainThreadPendingPointersMutex;
};

void DelayedPointerDeletionManager::AddPointerToMainThreadDealloc( void* ptr) 
{
	Mutex::AutoLock autolock(m_MainThreadPendingPointersMutex);
	if(++m_MainThreadPendingCount > m_MainThreadPendingReserved)
		GrowPendingBuffer();
	m_MainThreadPendingPointers[m_MainThreadPendingCount-1] = ptr;
	m_HasPendingDeletes = 1;
}

void DelayedPointerDeletionManager::CleanupPendingMainThreadPointers() 
{
	Mutex::AutoLock autolock(m_MainThreadPendingPointersMutex);
	Assert(Thread::CurrentThreadIsMainThread());
	m_HasPendingDeletes = 0;

	for(UInt32 i = 0; i < m_MainThreadPendingCount; ++i)
		m_MainAlloctor->Deallocate(m_MainThreadPendingPointers[i]);
	m_MainThreadPendingCount = 0;
}

void DelayedPointerDeletionManager::DeallocateLocalMemory() 
{ 
	Assert(!m_HasPendingDeletes && m_MainThreadPendingCount == 0);
	m_ThreadAlloctor->Deallocate(m_MainThreadPendingPointers);
	m_MainThreadPendingPointers = NULL;
	m_MainThreadPendingReserved = 0;
};

void DelayedPointerDeletionManager::GrowPendingBuffer()
{
	const UInt32 kInitialBufferSize = 128;
	m_MainThreadPendingReserved = std::max(m_MainThreadPendingReserved*2, kInitialBufferSize);
	m_MainThreadPendingPointers = (void**) m_ThreadAlloctor->Reallocate(m_MainThreadPendingPointers, m_MainThreadPendingReserved*sizeof(void*), kDefaultMemoryAlignment);
}


template <class UnderlyingAllocator>
DualThreadAllocator<UnderlyingAllocator>::DualThreadAllocator(const char* name, BaseAllocator* mainAllocator, BaseAllocator* threadAllocator)
	: BaseAllocator(name)
{
	m_MainAllocator = (UnderlyingAllocator*)mainAllocator;
	m_ThreadAllocator = (UnderlyingAllocator*)threadAllocator;
	m_DelayedDeletion = NULL;
}

template <class UnderlyingAllocator>
void DualThreadAllocator<UnderlyingAllocator>::ThreadCleanup()
{
	if(Thread::CurrentThreadIsMainThread())
		UNITY_DELETE(m_DelayedDeletion, kMemManager);
}

template <class UnderlyingAllocator>
void DualThreadAllocator<UnderlyingAllocator>::FrameMaintenance(bool cleanup)
{
	if(m_DelayedDeletion)
	{
		m_DelayedDeletion->CleanupPendingMainThreadPointers();
		if(cleanup)
			m_DelayedDeletion->DeallocateLocalMemory();
	}
}

template <class UnderlyingAllocator>
UnderlyingAllocator* DualThreadAllocator<UnderlyingAllocator>::GetCurrentAllocator()
{
	if(Thread::CurrentThreadIsMainThread())
		return m_MainAllocator;
	else
		return m_ThreadAllocator;
}


template <class UnderlyingAllocator>
void* DualThreadAllocator<UnderlyingAllocator>::Allocate( size_t size, int align )
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();
	bool isMainThread = alloc == m_MainAllocator;
	if(isMainThread && m_DelayedDeletion && m_DelayedDeletion->HasPending())
		m_DelayedDeletion->CleanupPendingMainThreadPointers();

	return alloc->UnderlyingAllocator::Allocate(size, align);
}

template <class UnderlyingAllocator>
void* DualThreadAllocator<UnderlyingAllocator>::Reallocate( void* p, size_t size, int align )
{ 
	UnderlyingAllocator* alloc = GetCurrentAllocator();

	if(alloc->UnderlyingAllocator::Contains(p))
		return alloc->UnderlyingAllocator::Reallocate(p, size, align);

	UnderlyingAllocator* containingAlloc = NULL;
	if (alloc == m_MainAllocator)
	{
		Assert(m_ThreadAllocator->UnderlyingAllocator::Contains(p));
		containingAlloc = m_ThreadAllocator;
	}
	else
	{
		Assert(m_MainAllocator->UnderlyingAllocator::Contains(p));
		containingAlloc = m_MainAllocator;
	}

	size_t oldSize = containingAlloc->UnderlyingAllocator::GetPtrSize(p);
	void* ptr = alloc->UnderlyingAllocator::Allocate(size, align);
	memcpy(ptr, p, std::min(size,oldSize));
	Deallocate(p);
	return ptr;
}

template <class UnderlyingAllocator>
void DualThreadAllocator<UnderlyingAllocator>::Deallocate( void* p )
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();

	if(alloc->UnderlyingAllocator::Contains(p))
		return alloc->UnderlyingAllocator::Deallocate(p);

	if (alloc == m_MainAllocator)
	{
		DebugAssert(m_ThreadAllocator->UnderlyingAllocator::Contains(p));
		m_ThreadAllocator->UnderlyingAllocator::Deallocate(p);
	}
	else
	{
		DebugAssert(m_MainAllocator->UnderlyingAllocator::Contains(p));
		if(!m_DelayedDeletion)
		{
			SET_ALLOC_OWNER(NULL);
			m_DelayedDeletion = UNITY_NEW(DelayedPointerDeletionManager(m_MainAllocator, m_ThreadAllocator), kMemManager);
		}
		m_DelayedDeletion->AddPointerToMainThreadDealloc(p);
	}
}

template <class UnderlyingAllocator>
bool DualThreadAllocator<UnderlyingAllocator>::TryDeallocate( void* p )
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();

	if(alloc->UnderlyingAllocator::Contains(p))
	{
		alloc->UnderlyingAllocator::Deallocate(p);
		return true;
	}

	if (m_ThreadAllocator->UnderlyingAllocator::Contains(p))
	{
		m_ThreadAllocator->UnderlyingAllocator::Deallocate(p);
		return true;
	}
	if(m_MainAllocator->UnderlyingAllocator::Contains(p))
	{
		if(!m_DelayedDeletion)
			m_DelayedDeletion = UNITY_NEW(DelayedPointerDeletionManager(m_MainAllocator, m_ThreadAllocator), kMemManager);
		m_DelayedDeletion->AddPointerToMainThreadDealloc(p);
		return true;
	}
	return false;
}


template <class UnderlyingAllocator>
bool DualThreadAllocator<UnderlyingAllocator>::Contains( const void* p )
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();
	if(alloc->UnderlyingAllocator::Contains(p))
		return true;
	if(m_ThreadAllocator->UnderlyingAllocator::Contains(p))
		return true;
	if(m_MainAllocator->UnderlyingAllocator::Contains(p))
		return true;
	return false;
}

template <class UnderlyingAllocator>
size_t DualThreadAllocator<UnderlyingAllocator>::GetAllocatedMemorySize( ) const
{
	return m_MainAllocator->GetAllocatedMemorySize() + (m_ThreadAllocator?m_ThreadAllocator->GetAllocatedMemorySize():0);
}

template <class UnderlyingAllocator>
size_t DualThreadAllocator<UnderlyingAllocator>::GetAllocatorSizeTotalUsed() const
{
	return m_MainAllocator->GetAllocatorSizeTotalUsed() + (m_ThreadAllocator?m_ThreadAllocator->GetAllocatorSizeTotalUsed():0);
}

template <class UnderlyingAllocator>
size_t DualThreadAllocator<UnderlyingAllocator>::GetReservedSizeTotal() const
{
	return m_MainAllocator->GetReservedSizeTotal() + (m_ThreadAllocator?m_ThreadAllocator->GetReservedSizeTotal():0);
}

template <class UnderlyingAllocator>
size_t DualThreadAllocator<UnderlyingAllocator>::GetPtrSize( const void* ptr ) const
{
	// all allocators have the same allocation header
	return m_MainAllocator->UnderlyingAllocator::GetPtrSize(ptr);
}

template <class UnderlyingAllocator>
ProfilerAllocationHeader* DualThreadAllocator<UnderlyingAllocator>::GetProfilerHeader( const void* ptr ) const
{
	return m_MainAllocator->UnderlyingAllocator::GetProfilerHeader(ptr);
}


template <class UnderlyingAllocator>
bool DualThreadAllocator<UnderlyingAllocator>::CheckIntegrity()
{
	bool valid = m_ThreadAllocator->UnderlyingAllocator::CheckIntegrity();
	if(Thread::CurrentThreadIsMainThread())
		valid &= m_MainAllocator->UnderlyingAllocator::CheckIntegrity();
	Assert(valid);
	return valid;
}

template <class UnderlyingAllocator>
bool DualThreadAllocator<UnderlyingAllocator>::ValidatePointer(void* ptr)
{
	UnderlyingAllocator* alloc = GetCurrentAllocator();
	return alloc->UnderlyingAllocator::ValidatePointer(ptr);
}

template class DualThreadAllocator< DynamicHeapAllocator< LowLevelAllocator > >;

#endif // #if ENABLE_MEMORY_MANAGER
