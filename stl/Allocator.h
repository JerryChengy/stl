#ifndef UNITY_CUSTOM_ALLOCATOR_H_
#define UNITY_CUSTOM_ALLOCATOR_H_



#if UNITY_WII
#	define MemPool1 0
#	define MemPool2 1
#elif UNITY_XENON
#	define MemPool1 0
#	define MemPool2 0
#else 
#	define MemPool1 0
#	define MemPool2 1
#endif

#endif // UNITY_CUSTOM_ALLOCATOR_H_
