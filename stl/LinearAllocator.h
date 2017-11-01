#ifndef LINEAR_ALLOCATOR_H_
#define LINEAR_ALLOCATOR_H_

#include <cstddef>
#include <list>
#include "assert.h"
#include "UnityConfigure.h"
#include "LogAssert.h"
#if UNITY_XENON
#include <malloc.h>
#endif
#if UNITY_LINUX
#include <stdint.h>   // uintptr_t
#endif

#if ENABLE_THREAD_CHECK_IN_ALLOCS
#include "Thread.h"
#endif
#include "MemoryMacros.h"
#define Assert(x)
struct LinearAllocatorBase
{
	static const int	kMinimalAlign = 4;
		
	struct Block
	{
		char*	m_Begin;
		char*	m_Current;
		size_t	m_Size;
		MemLabelId m_Label;
		
		void initialize (size_t size, MemLabelId label)
		{
			m_Label = label;
			m_Current = m_Begin = (char*)UNITY_MALLOC(label,size);
			m_Size = size;
		}
		
		void reset ()
		{
			m_Current = m_Begin;
		}
		
		void purge ()
		{
			UNITY_FREE(m_Label, m_Begin);
		}
		
		size_t used () const
		{
			return m_Current - m_Begin;
		}
		
		void* current () const
		{
			return m_Current;
		}
		
		size_t available () const
		{
			return m_Size - used ();
		}
		
		size_t padding (size_t alignment) const
		{
			size_t pad = ((uintptr_t)m_Current - 1 | alignment - 1) + 1 - (uintptr_t)m_Current;
			return pad;
		}

		void* bump (size_t size)
		{
			Assert (size <= available());
			char* p = m_Current;
			m_Current += size;
			return p;
		}
		
		void roll_back (size_t size)
		{
			Assert (used () >= size);
			m_Current -= size;
		}
		
		bool belongs (const void* p)
		{
			//if (p >= m_Begin && p <= m_Begin + m_Size)
			//	return true;
			//return false;

			//return p >= m_Begin && p <= m_Begin + m_Size;

			return (uintptr_t)p - (uintptr_t)m_Begin <= (uintptr_t)m_Size;
		}

		void set (void* p)
		{
			Assert (p >= m_Begin && p < m_Begin + m_Size);
			m_Current = (char*)p;
		}
	};
	
	typedef std::list<Block, STL_ALLOCATOR(kMemPoolAlloc, Block) >	block_container;
	
	LinearAllocatorBase (size_t blockSize, MemLabelId label)
	:	m_Blocks(), m_BlockSize (blockSize), m_AllocLabel (label)
	{
	}
	
	void add_block (size_t size)
	{
		m_Blocks.push_back (Block());
		size_t blockSize = size > m_BlockSize ? size : m_BlockSize;
		m_Blocks.back ().initialize (blockSize, m_AllocLabel);
	}
	
	void purge (bool releaseAllBlocks = false)
	{
		if (m_Blocks.empty ())
			return;
			
		block_container::iterator begin = m_Blocks.begin ();
			
		if (!releaseAllBlocks)
			begin++;
			
		for (block_container::iterator it = begin, end = m_Blocks.end (); it != end; ++it)
			it->purge ();

		m_Blocks.erase (begin, m_Blocks.end ());

		if (!releaseAllBlocks)
			m_Blocks.back ().reset ();
	}
	
	bool belongs (const void* p)
	{
		for (block_container::iterator it = m_Blocks.begin (), end = m_Blocks.end (); it != end; ++it)
		{
			if (it->belongs (p))
				return true;
		}
		
		return false;
	}

	void* current () const
	{
		return m_Blocks.empty () ? 0 : m_Blocks.back ().current ();
	}

	void rewind (void* mark)
	{
		for (block_container::iterator it = m_Blocks.end (); it != m_Blocks.begin (); --it)
		{
			block_container::iterator tit = it;
			--tit;

			if (tit->belongs (mark)) {
				tit->set (mark);

				for (block_container::iterator temp = it; temp != m_Blocks.end (); ++temp)
					temp->purge ();
				
				m_Blocks.erase (it, m_Blocks.end ());
				break;
			}
		}
	}
	
protected:
	block_container	m_Blocks;
	size_t			m_BlockSize;	
	MemLabelId      m_AllocLabel;
};


struct ForwardLinearAllocator : public LinearAllocatorBase
{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
	Thread::ThreadID		m_AllocThread, m_DeallocThread;
	int m_Allocated;
	bool m_RequireDeallocation;

	void SetThreadIDs (Thread::ThreadID allocThread, Thread::ThreadID deallocThread)
	{
		m_AllocThread = allocThread;
		m_DeallocThread = deallocThread;
	}

	void SetRequireDeallocation (bool v)
	{
		m_RequireDeallocation = v;
	}
#endif

	ForwardLinearAllocator (size_t blockSize, MemLabelId label)
	:	LinearAllocatorBase (blockSize, label)
#if ENABLE_THREAD_CHECK_IN_ALLOCS
	,	m_AllocThread (0), m_DeallocThread (0), m_Allocated(0), m_RequireDeallocation(false)
#endif
	{
	}
	
	~ForwardLinearAllocator ()
	{
		purge (true);
	}

	size_t GetAllocatedBytes() const
	{
		size_t s = 0;
		for (block_container::const_iterator it = m_Blocks.begin (); it != m_Blocks.end(); ++it)
			s += it->used();
		return s;
	}
	
	void* allocate (size_t size, size_t alignment = 4)
	{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
		ErrorIf (!Thread::EqualsCurrentThreadIDForAssert (m_AllocThread));
		m_Allocated++;
#endif
//		Assert (size == AlignUIntPtr (size, kMinimalAlign));
		
		if (m_Blocks.empty ())
			add_block (size);

		Block* block = &m_Blocks.back ();
		size_t padding = block->padding (alignment);
		
		if (size + padding > block->available ()) {
			add_block (size);
			block = &m_Blocks.back ();
		}
			
		uintptr_t p = (uintptr_t)block->bump (size + padding);
			
		return (void*)(p + padding);
	}

	void deallocate (void* dealloc)
	{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
		ErrorIf (Thread::GetCurrentThreadID () != m_DeallocThread);
		m_Allocated--;
#endif
	}
	
	void deallocate_no_thread_check (void* dealloc)
	{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
		m_Allocated--;
#endif
	}
	
	void purge (bool releaseAllBlocks = false)
	{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
		ErrorIf (Thread::GetCurrentThreadID () != m_DeallocThread && m_DeallocThread != 0);
		ErrorIf (m_RequireDeallocation && m_Allocated != 0);
#endif
		LinearAllocatorBase::purge (releaseAllBlocks);
	}

	void rewind (void* mark)
	{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
		ErrorIf (Thread::GetCurrentThreadID () != m_DeallocThread);
#endif
		LinearAllocatorBase::rewind (mark);
	}

	using LinearAllocatorBase::current;
	using LinearAllocatorBase::belongs;
};
/*
// std::allocator concept implementation for ForwardLinearAllocator objects.
// use it to make STL use your *locally* created ForwardLinearAllocator object
// example:
//	void HeavyMemoryAllocationFuncion ()
//	{
//		ForwardLinearAllocator fwdalloc (1024);
//		std::vector<int, forward_linear_allocator<int> > container (forward_linear_allocator<int>(fwdalloc));
//
//		// use vector
//
//		// memory is clean up automatically
//	}

template<class T>
class forward_linear_allocator
{
public:	
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;
	
	template <class U> struct rebind { typedef forward_linear_allocator<U> other; };
	
	forward_linear_allocator(ForwardLinearAllocator& al) throw() : m_LinearAllocator (al) {}
	forward_linear_allocator(const forward_linear_allocator& al) throw() : m_LinearAllocator (al.m_LinearAllocator) {}
	template <class U> forward_linear_allocator(const forward_linear_allocator<U>& al) throw() : m_LinearAllocator (al.m_LinearAllocator) {}
	~forward_linear_allocator() throw() {}
	
	pointer address(reference x) const { return &x; }
	const_pointer address(const_reference x) const { return &x; }
	
	pointer allocate(size_type count, void const* hint = 0)
	{ return (pointer)m_LinearAllocator.allocate (count * sizeof(T)); }
	void deallocate(pointer p, size_type n)
	{ m_LinearAllocator.deallocate(p); }
	
	size_type max_size() const throw()
	{	return 0x80000000; }
	
	void construct(pointer p, const T& val)
	{	new (p) T( val ); }
	
	void destroy(pointer p)
	{	p->~T(); }

private:
	ForwardLinearAllocator&	m_LinearAllocator;
};

// this a global ForwardLinearAllocator object that can be used to allocate (and release) memory from
// anywhere in the program. Caller is responsible for tracking memory used in total
// (use ForwardLinearAllocator::current/ForwardLinearAllocator::rewind to save and restore memory pointer)
extern ForwardLinearAllocator	g_ForwardFrameAllocator;

// std::allocator concept for global ForwardLinearAllocator object
// this is just to save one indirection, otherwise forward_linear_allocator referencing a global ForwardLinearAllocator object could be used
template<class T>
class global_linear_allocator
{
public:	
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;
	
	template <class U> struct rebind { typedef global_linear_allocator<U> other; };
	
	global_linear_allocator() {}
	global_linear_allocator(const global_linear_allocator&) throw() {}
	template <class U> global_linear_allocator(const global_linear_allocator<U>&) throw() {}
	~global_linear_allocator() throw() {}
	
	pointer address(reference x) const { return &x; }
	const_pointer address(const_reference x) const { return &x; }
	
	pointer allocate(size_type count, void const* hint = 0)
	{ return (pointer)g_ForwardFrameAllocator.allocate (count * sizeof(T)); }
	void deallocate(pointer p, size_type n)
	{ g_ForwardFrameAllocator.deallocate(p); }

	bool operator==(global_linear_allocator const& a) const
	{	return true; }
	bool operator!=(global_linear_allocator const& a) const
	{	return false; }
	
	size_type max_size() const throw()
	{	return 0x7fffffff; }
	
	void construct(pointer p, const T& val)
	{	new (p) T( val ); }
	
	void destroy(pointer p)
	{	p->~T(); }
};

#define DECLARE_GLOBAL_LINEAR_ALLOCATOR_MEMBER_NEW_DELETE \
public:	\
	inline void* operator new( size_t size ) { return g_ForwardFrameAllocator.allocate(size); } \
	inline void	operator delete( void* p ) { g_ForwardFrameAllocator.deallocate(p); }

inline void* operator new (size_t size, ForwardLinearAllocator& al) { return al.allocate (size); }
inline void* operator new [] (size_t size, ForwardLinearAllocator& al) { return al.allocate (size); }

inline void operator delete (void* p, ForwardLinearAllocator& al) { }
inline void operator delete [] (void* p, ForwardLinearAllocator& al) { }
*/


#endif
