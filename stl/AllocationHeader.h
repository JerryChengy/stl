#ifndef ALLOCATION_HEADER_H_
#define ALLOCATION_HEADER_H_

#if ENABLE_MEMORY_MANAGER

#include "MemoryProfiler.h"

#define USE_MEMORY_DEBUGGING (!UNITY_RELEASE && !(UNITY_LINUX && UNITY_64))
#if UNITY_LINUX
#pragma warning "FIXME LINUX: 64bit memory debugging"
#endif

/*
Allocation Header:
(12345hhhhhhhhmmmm____________) (1-n:paddingcount, h: header, m:memoryprofiler)
hasPadding: 1 bit
size: 31 bit
*********USE_MEMORY_DEBUGGING*********
  allocator: 16 bit
  magicValue: 12 bit
*********USE_MEMORY_DEBUGGING*********
---------------
Followed by x bytes requested by MemoryProfiler
----------------------------------------
Actual Allocated Data
----------------------------------------
*********USE_MEMORY_DEBUGGING*********
Followed by a footer if memory guarding is enabled
*********USE_MEMORY_DEBUGGING*********
*/

struct AllocationHeader
{
public:
	static void Set(void* ptr, int id, int size, int padCount, int align);
	static AllocationHeader* GetHeader(const void* ptr);
	static ProfilerAllocationHeader* GetProfilerHeader(const void* ptr);

	UInt32 GetPadding () const { return m_HasPadding ? *(((UInt32*)(this))-1) : 0; }
	size_t GetRequestedSize () const { return m_AllocationSize; }

	static size_t CalculateNeededAllocationSize( size_t size, int align );
	static void ValidateIntegrity(void* ptr, int id, int align = -1);
	static void* GetRealPointer( void* ptr );
	static int GetRequiredPadding(void* realptr, int align);
	static int GetHeaderSize();
	static AllocationHeader* GetHeaderFromRealPointer( const void* realptr );

	int GetOverheadSize();

private:

	// prev byte is padding count, if there is padding
	UInt32         m_HasPadding : 1;
	UInt32         m_AllocationSize : 31;

#if USE_MEMORY_DEBUGGING
	// DEBUG:
	SInt16         m_AllocatorIdentifier;
	UInt16         m_Magic : 12;
	static const UInt32 kMagicValue = 0xDFA;
	static const UInt32 kFooterSize = 4;
#else
	static const UInt32 kFooterSize = 0;
#endif

	// followed by x bytes used for memory profiling header (0 if no memory profiling)
};

inline void AllocationHeader::Set(void* ptr, int id, int size, int padCount, int align)
{
	AllocationHeader* header = GetHeader(ptr);
	header->m_AllocationSize = size;
	header->m_HasPadding = padCount != 0;
	if(header->m_HasPadding)
		*(((UInt32*)(header))-1) = padCount;
#if USE_MEMORY_DEBUGGING
	// set header
	if(header->m_HasPadding) // set leading bytes
		memset(((char*)header)-padCount,0xAA,padCount-4);

	header->m_AllocatorIdentifier = id;
	header->m_Magic = kMagicValue;
	// set footer
	memset(((char*)ptr)+size,0xFD,kFooterSize);
#endif
}

inline ProfilerAllocationHeader* AllocationHeader::GetProfilerHeader( const void* ptr )
{
	return (ProfilerAllocationHeader*)( (const char*)ptr - MemoryProfiler::GetHeaderSize() );
}

inline AllocationHeader* AllocationHeader::GetHeader( const void* ptr )
{
	return (AllocationHeader*)( (const char*)ptr - GetHeaderSize() );
}

inline int AllocationHeader::GetHeaderSize()
{
	return sizeof(AllocationHeader) + MemoryProfiler::GetHeaderSize();
}

inline int AllocationHeader::GetRequiredPadding( void* realptr, int align )
{
	return align - ((((int)realptr + GetHeaderSize() - 1)&(align - 1)) + 1);
}

inline size_t AllocationHeader::CalculateNeededAllocationSize( size_t size, int align )
{
	int alignMask = align-1;
	return size + GetHeaderSize() + kFooterSize + alignMask;
}

inline void* AllocationHeader::GetRealPointer( void* ptr )
{
	AllocationHeader* header = GetHeader(ptr);
	int padCount = header->GetPadding();
	return ((char*)header) - padCount;
}

inline int AllocationHeader::GetOverheadSize()
{
	int alignMask = kDefaultMemoryAlignment-1; // estimate
	return GetHeaderSize() + kFooterSize + alignMask;
}

inline AllocationHeader* AllocationHeader::GetHeaderFromRealPointer( const void* realptr )
{
#if USE_MEMORY_DEBUGGING
	unsigned char* ptr = (unsigned char*)realptr;
	while(*ptr == 0xAA)
		ptr++;
	AllocationHeader* header = (AllocationHeader*)ptr;
	if(header->m_Magic != kMagicValue)
		header = (AllocationHeader*)(ptr+4);
	return header;
#else
	return NULL;
#endif
}

inline void AllocationHeader::ValidateIntegrity( void* ptr, int id, int /*align*/ )
{
#if USE_MEMORY_DEBUGGING
	AllocationHeader* header = GetHeader(ptr);
	Assert(header->m_Magic == AllocationHeader::kMagicValue);
	Assert(id == -1 || id == header->m_AllocatorIdentifier);

	int size = header->m_AllocationSize;
	unsigned char* footer = ((unsigned char*)ptr)+size;
	for(int i = 0; i < kFooterSize; i++, footer++)
		Assert(*footer == 0xFD);
#endif
}

#endif

#endif
