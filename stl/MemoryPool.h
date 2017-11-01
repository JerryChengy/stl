#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include "MemoryMacros.h"
#include "dynamic_array.h"
#include "ExportModules.h"

#if ENABLE_THREAD_CHECK_IN_ALLOCS
#include "Runtime/Threads/Thread.h"
#endif


// --------------------------------------------------------------------------
// A free-list based fixed size allocator.
//
// To override new/delete per class use DECLARE_POOLED_ALLOC and DEFINE_POOLED_ALLOC.
//
// memory_pool<T> is an STL allocator that only supports allocating a single element
// at a time. So it can be used with list, map, set but not with vector or string.
//
// Allocator creates "bubbles" of objects, each containin a free-list inside. When a bubble
// is full, it allocates a new one. Empty bubbles are NOT destroyed.


// --------------------------------------------------------------------------

class EXPORT_COREMODULE MemoryPool {
public:
	MemoryPool( bool threadCheck, const char* name, int blockSize, int allocatedSizeHint, MemLabelId label = kMemPoolAlloc );
	~MemoryPool();

	/// Allocate single block
	void*	Allocate();
	/// Allocate less than single block
	void*	Allocate( size_t amount );
	/// Deallocate
	void	Deallocate( void *ptr );
	/// Deallocate everything
	void	DeallocateAll();


	#if !DEPLOY_OPTIMIZED
	size_t GetBubbleCount() const { return m_Bubbles.size(); }
	int GetAllocCount() const { return m_AllocCount; }
	#endif

	int	GetAllocatedBytes() { return m_Bubbles.size () * m_BlocksPerBubble * m_BlockSize;  }

	#if DEBUGMODE
	int	GetAllocatedObjectsCount() { return m_AllocCount; }
	#endif

	void PreallocateMemory(int size);
	void SetAllocateMemoryAutomatically (bool allocateMemoryAuto) { m_AllocateMemoryAutomatically = allocateMemoryAuto; }

	static void StaticInitialize();
	static void StaticDestroy();
	static void RegisterStaticMemoryPool(MemoryPool* pool);

private:
	void	AllocNewBubble();

	struct Bubble
	{
		char	data[1]; // actually byteCount
	};
	typedef dynamic_array<Bubble*>	Bubbles;

	void	Reset();
private:
	int 	m_BlockSize;
	int 	m_BubbleSize;
	int 	m_BlocksPerBubble;

	Bubbles	m_Bubbles;

	void*	m_HeadOfFreeList;
	bool    m_AllocateMemoryAutomatically;

	MemLabelId m_AllocLabel;

	int 	m_AllocCount; // number of blocks currently allocated
	#if DEBUGMODE
	int 	m_PeakAllocCount; // stats
	const char*	m_Name; // for debugging
	#endif

	#if ENABLE_THREAD_CHECK_IN_ALLOCS
	bool m_ThreadCheck;
	#endif

	static UNITY_VECTOR(kMemPoolAlloc,MemoryPool*)* s_MemoryPools;
};


// --------------------------------------------------------------------------
//  Macros for class fixed-size pooled allocations:
//		DECLARE_POOLED_ALLOC in the .h file, in a private section of a class,
//		DEFINE_POOLED_ALLOC in the .cpp file

#define ENABLE_MEMORY_POOL 1

#if ENABLE_MEMORY_POOL

#define STATIC_INITIALIZE_POOL( _clazz ) _clazz::s_PoolAllocator = UNITY_NEW(MemoryPool, kMemPoolAlloc)(true, #_clazz, sizeof(_clazz), _clazz::s_PoolSize)
#define STATIC_DESTROY_POOL( _clazz ) UNITY_DELETE(_clazz::s_PoolAllocator, kMemPoolAlloc)

#define DECLARE_POOLED_ALLOC( _clazz ) \
public:	\
	inline void* operator new( size_t size ) { return s_PoolAllocator->Allocate(size); } \
	inline void	operator delete( void* p ) { s_PoolAllocator->Deallocate(p); } \
	static MemoryPool *s_PoolAllocator; \
	static int s_PoolSize; \
private:

#define DEFINE_POOLED_ALLOC( _clazz, _bubbleSize ) \
	MemoryPool* _clazz::s_PoolAllocator = NULL; \
	int _clazz::s_PoolSize = _bubbleSize;

#else

#define STATIC_INITIALIZE_POOL( _clazz )
#define STATIC_DESTROY_POOL( _clazz )
#define DECLARE_POOLED_ALLOC( _clazz )
#define DEFINE_POOLED_ALLOC( _clazz, _bubbleSize )

#endif


// --------------------------------------------------------------------------

template<int SIZE>
struct memory_pool_impl
{
	struct AutoPoolWrapper
	{
		AutoPoolWrapper( int size)
		{
			SET_ALLOC_OWNER(NULL);
			pool = UNITY_NEW(MemoryPool( true, "mempoolalloc", size, 32 * 1024, kMemPoolAlloc ), kMemPoolAlloc);
			MemoryPool::RegisterStaticMemoryPool(pool);
		}
		~AutoPoolWrapper()
		{
		}
		MemoryPool* pool;
	};
	static MemoryPool& get_pool () {
		static AutoPoolWrapper pool( SIZE );
		return *(pool.pool);
	}
};

/*

  THIS IS NOT THREAD SAFE. sharing of pools is by size, thus pools might randomly be shared from different threads.

*/



template<typename T>
class memory_pool
{
public:
	typedef size_t    size_type;
	typedef std::ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;

	template <class U> struct rebind { typedef memory_pool<U> other; };

	memory_pool() { }
	memory_pool( const memory_pool<T>& ) { }
	template<class B> memory_pool(const memory_pool<B>&) { } // construct from a related allocator
	template<class B> memory_pool<T>& operator=(const memory_pool<B>&) { return *this; } // assign from a related allocator

	~memory_pool() throw() { }

	pointer address(reference x) const { return &x; }
	const_pointer address(const_reference x) const { return &x; }
	size_type max_size() const throw() {return size_t(-1) / sizeof(value_type);}
	void construct(pointer p, const T& val) { ::new((void*)p) T(val); }
	void destroy(pointer p) { p->~T(); }

	pointer allocate(size_type n, std::allocator<void>::const_pointer /*hint*/ = 0)
	{
		if(n==1)
			return reinterpret_cast<pointer>( memory_pool_impl<sizeof(T)>::get_pool ().Allocate(n * sizeof(T)) );
		else
			return reinterpret_cast<pointer>(UNITY_MALLOC(kMemPoolAlloc,n*sizeof(T)));
	}

	void deallocate(pointer p, size_type n)
	{
		if(n==1)
			return memory_pool_impl<sizeof(T)>::get_pool ().Deallocate( p );
		else
			return UNITY_FREE(kMemPoolAlloc,p);
	}
};

template<typename T>
class memory_pool_explicit
{
public:
	typedef size_t    size_type;
	typedef std::ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;
	template <class U> struct rebind { typedef memory_pool_explicit<U> other; };

	MemoryPool*   m_Pool;

	memory_pool_explicit(MemoryPool& pool) { m_Pool = &pool; }
	memory_pool_explicit() { m_Pool = NULL; }
	memory_pool_explicit( const memory_pool_explicit<T>& b) { m_Pool = b.m_Pool; }
	template<class B> memory_pool_explicit(const memory_pool_explicit<B>& b) { m_Pool = b.m_Pool; } // construct from a related allocator
	template<class B> memory_pool_explicit<T>& operator=(const memory_pool_explicit<B>& b) { m_Pool = b.m_Pool; return *this; } // assign from a related allocator

	~memory_pool_explicit() throw() { }

	pointer address(reference x) const { return &x; }
	const_pointer address(const_reference x) const { return &x; }
	size_type max_size() const throw() {return size_t(-1) / sizeof(value_type);}
	void construct(pointer p, const T& val) { ::new((void*)p) T(val); }
	void destroy(pointer p) { p->~T(); }

	pointer allocate(size_type n, std::allocator<void>::const_pointer /*hint*/ = 0)
	{
		DebugAssertIf(n != 1);
		AssertIf(m_Pool == NULL);
		return reinterpret_cast<pointer>( m_Pool->Allocate(n * sizeof(T)) );
	}

	void deallocate(pointer p, size_type n)
	{
		DebugAssertIf(n != 1);
		AssertIf(m_Pool == NULL);
		m_Pool->Deallocate( p );
	}
};

template<typename A, typename B>
inline bool operator==( const memory_pool<A>&, const memory_pool<B>& )
{
	// test for allocator equality (always true)
	return true;
}

template<typename A, typename B>
inline bool operator!=( const memory_pool<A>&, const memory_pool<B>& )
{
	// test for allocator inequality (always false)
	return false;
}


template<typename A, typename B>
inline bool operator==( const memory_pool_explicit<A>& lhs, const memory_pool_explicit<B>& rhs)
{
	// test for allocator equality (always true)
	return lhs.m_Pool == rhs.m_Pool;
}

template<typename A, typename B>
inline bool operator!=( const memory_pool_explicit<A>& lhs, const memory_pool_explicit<B>& rhs)
{
	// test for allocator inequality (always false)
	return lhs.m_Pool != rhs.m_Pool;
}

#endif
