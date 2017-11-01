#include "UnityPrefix.h"
#include "BaseAllocator.h"

void* BaseAllocator::Reallocate( void* p, size_t size, int align )
{
	//	ErrorString("Not implemented"); 
	return 0;
}

static UInt32 g_IncrementIdentifier = 0x10;
BaseAllocator::BaseAllocator(const char* name) 
	: m_TotalRequestedBytes(0)
	, m_TotalReservedMemory(0)
	, m_BookKeepingMemoryUsage(0)
	, m_PeakRequestedBytes(0)
	, m_NumAllocations(0)
	, m_Name(name)
{
	m_AllocatorIdentifier = g_IncrementIdentifier++;
}

