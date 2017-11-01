#include "UnityPrefix.h"
#include "MemoryPool.h"
#include "Word.h"
#include "MemoryProfiler.h"
#include "InitializeAndCleanup.h"
#include "LogAssert.h"

static int kMinBlockSize = sizeof(void*);
UNITY_VECTOR(kMemPoolAlloc,MemoryPool*)* MemoryPool::s_MemoryPools = NULL;
void MemoryPool::StaticInitialize()
{
	s_MemoryPools = UNITY_NEW(UNITY_VECTOR(kMemPoolAlloc,MemoryPool*),kMemPoolAlloc);
}

void MemoryPool::StaticDestroy()
{
	for(size_t i = 0; i < s_MemoryPools->size(); i++)
		UNITY_DELETE((*s_MemoryPools)[i],kMemPoolAlloc);
	UNITY_DELETE(s_MemoryPools,kMemPoolAlloc);
}

static RegisterRuntimeInitializeAndCleanup s_MemoryPoolCallbacks(MemoryPool::StaticInitialize, MemoryPool::StaticDestroy);

void MemoryPool::RegisterStaticMemoryPool(MemoryPool* pool)
{
	s_MemoryPools->push_back(pool);
}

MemoryPool::MemoryPool( bool threadCheck, const char* name, int blockSize, int hintSize, MemLabelId label )
	: m_AllocLabel(MemLabelId(label.label, GET_CURRENT_ALLOC_ROOT_HEADER()))
	, m_Bubbles(MemLabelId(label.label, GET_CURRENT_ALLOC_ROOT_HEADER()))
#if DEBUGMODE
	,	m_PeakAllocCount(0)
	,	m_Name(name)
#endif
{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
	m_ThreadCheck = threadCheck;
#endif

	if (blockSize < kMinBlockSize)
		blockSize = kMinBlockSize;
	m_BlockSize = blockSize;


	m_BubbleSize = hintSize;
	m_BlocksPerBubble = (m_BubbleSize - sizeof(Bubble) + 1) / blockSize;

	int usedBubbleSize = sizeof(Bubble) + m_BlocksPerBubble * blockSize - 1;
	Assert(usedBubbleSize <= m_BubbleSize);
	Assert(m_BubbleSize - usedBubbleSize <= m_BlockSize);

	Assert (m_BlocksPerBubble >= 128);
	Assert(hintSize % 4096 == 0);

	m_AllocateMemoryAutomatically = true;

	Reset();
}

MemoryPool::~MemoryPool()
{
#if !UNITY_EDITOR && DEBUGMODE
	if (m_AllocCount > 0)
		ErrorStringMsg( "Memory pool has %d unallocated objects: %s", m_AllocCount, m_Name ); // some stuff not deallocated?
#endif
	DeallocateAll();
}

void MemoryPool::Reset()
{
#if DEBUGMODE
	m_AllocCount = 0;
#endif
	m_HeadOfFreeList = NULL;
}

void MemoryPool::DeallocateAll()
{
	Bubbles::iterator it, itEnd = m_Bubbles.end();
	for( it = m_Bubbles.begin(); it != itEnd; ++it )
		UNITY_FREE( m_AllocLabel, *it );
	m_Bubbles.clear();
	Reset();
}

void MemoryPool::PreallocateMemory (int size)
{
	bool temp = m_AllocateMemoryAutomatically;
	m_AllocateMemoryAutomatically = true;
	for (int i=0;i <= size / (m_BlocksPerBubble * m_BlockSize);i++)
	{
		AllocNewBubble();
	}
	m_AllocateMemoryAutomatically = temp;
}

void MemoryPool::AllocNewBubble(  )
{
	if (!m_AllocateMemoryAutomatically)
		return;

	AssertIf (m_BlocksPerBubble == 1); // can't have 1 element per bubble

	Bubble *bubble = (Bubble*)UNITY_MALLOC( m_AllocLabel, m_BubbleSize );
	AssertIf( !bubble );

	// put to bubble list
	m_Bubbles.push_back( bubble );

	// setup the free list inside a bubble
	void* oldHeadOfFreeList = m_HeadOfFreeList;
	m_HeadOfFreeList = bubble->data;
	AssertIf( !m_HeadOfFreeList );

	void **newBubble = (void**)m_HeadOfFreeList;
	for( int j = 0; j < m_BlocksPerBubble-1; ++j )
	{
		newBubble[0] = (char*)newBubble + m_BlockSize;
		newBubble = (void**)newBubble[0];
	}

	newBubble[0] = oldHeadOfFreeList; // continue with existing free list (or terminate with NULL if no free elements)

	// still failure, error out
	if( !m_HeadOfFreeList )
	{
		ErrorString( "out of memory!" );
	}
}

void* MemoryPool::Allocate()
{
	return Allocate( m_BlockSize );
}

void *MemoryPool::Allocate( size_t amount )
{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
	ErrorAndReturnValueIf(m_ThreadCheck && Thread::mainThreadId && !Thread::CurrentThreadIsMainThread(), NULL);
#endif


	void *returnBlock;

	if( amount > (unsigned int)m_BlockSize ) {
		ErrorString( Format("requested larger amount than block size! requested: %d, blocksize: %d", (unsigned)amount, (unsigned)m_BlockSize ));
		return NULL;
	}

	if( !m_HeadOfFreeList ) {
		// allocate new bubble
		AllocNewBubble();

		// Can't allocate
		if( m_HeadOfFreeList == NULL )
			return NULL;
	}

#if DEBUGMODE
	++m_AllocCount;
	if( m_AllocCount > m_PeakAllocCount )
		m_PeakAllocCount = m_AllocCount;
#endif

	returnBlock = m_HeadOfFreeList;

	// move the pointer to the next block
	m_HeadOfFreeList = *((void**)m_HeadOfFreeList);

	return returnBlock;
}

void MemoryPool::Deallocate( void *mem_Block )
{
#if ENABLE_THREAD_CHECK_IN_ALLOCS
	ErrorAndReturnIf(m_ThreadCheck && Thread::mainThreadId && !Thread::CurrentThreadIsMainThread());
#endif

	if( !mem_Block ) // ignore NULL deletes
		return;

#if DEBUGMODE
	// check to see if the memory is from the allocated range
	bool ok = false;
	size_t n = m_Bubbles.size();
	for( size_t i = 0; i < n; ++i ) {
		Bubble* p = m_Bubbles[i];
		if( (char*)mem_Block >= p->data && (char*)mem_Block < (p->data + m_BlockSize * m_BlocksPerBubble) ) {
			ok = true;
			break;
		}
	}
	AssertIf( !ok );
#endif

#if DEBUGMODE
	// invalidate the memory
	memset( mem_Block, 0xDD, m_BlockSize );
	AssertIf(m_AllocCount == 0);
#endif

#if DEBUGMODE
	--m_AllocCount;
#endif

	// make the block point to the first free item in the list
	*((void**)mem_Block) = m_HeadOfFreeList;
	// the list head is now the Deallocated block
	m_HeadOfFreeList = mem_Block;
}
