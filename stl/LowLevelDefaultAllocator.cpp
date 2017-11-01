#include "UnityPrefix.h"
#include "LowLevelDefaultAllocator.h"
#include "MemoryManager.h"

#if ENABLE_MEMORY_MANAGER


void* LowLevelAllocator::Malloc (size_t size) { return MemoryManager::LowLevelAllocate(size); }
void* LowLevelAllocator::Realloc (void* ptr, size_t size) { return MemoryManager::LowLevelReallocate(ptr, size); }
void  LowLevelAllocator::Free (void* ptr) { MemoryManager::LowLevelFree(ptr); }


#if UNITY_XENON
#include "PlatformDependent/Xbox360/Source/XenonMemory.h"

#if XBOX_USE_DEBUG_MEMORY
// Uses debug memory on compatible devkits
void* LowLevelAllocatorDebugMem::Malloc(size_t size)
{
	return DmDebugAlloc(size);
}
void* LowLevelAllocatorDebugMem::Realloc (void* ptr, size_t size)
{
	ErrorString("LowLevelAllocatorDebugMem::Realloc is not implemented.");
	return 0;
}
void  LowLevelAllocatorDebugMem::Free (void* ptr)
{
	DmDebugFree(ptr);
}
#endif // XBOX_USE_DEBUG_MEMORY
#endif
#endif
