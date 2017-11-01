#ifndef UNITY_ATOMIC_OPS_HPP_
#define UNITY_ATOMIC_OPS_HPP_

// Primitive atomic instructions as defined by the CPU architecture.

#include "MemoryMacros.h"	// defines FORCE_INLINE (?!)

// AtomicAdd - Returns the new value, after the operation has been performed (as defined by OSAtomicAdd32Barrier)
FORCE_INLINE int AtomicAdd (int volatile* i, int value);

// AtomicSub - Returns the new value, after the operation has been performed (as defined by OSAtomicSub32Barrier)
FORCE_INLINE int AtomicSub (int volatile* i, int value);

// AtomicIncrement - Returns the new value, after the operation has been performed (as defined by OSAtomicAdd32Barrier)
FORCE_INLINE int AtomicIncrement (int volatile* i);

// AtomicDecrement - Returns the new value, after the operation has been performed (as defined by OSAtomicAdd32Barrier)
FORCE_INLINE int AtomicDecrement (int volatile* i);

// AtomicCompareExchange - Returns value is the initial value of the Destination pointer (as defined by _InterlockedCompareExchange)
FORCE_INLINE bool AtomicCompareExchange (int volatile* i, int newValue, int expectedValue);

// AtomicExchange - Returns the initial value pointed to by Target (as defined by _InterlockedExchange)
FORCE_INLINE int AtomicExchange (int volatile* i, int value);

#define ATOMIC_API_GENERIC (UNITY_OSX || UNITY_IPHONE || UNITY_WIN || UNITY_XENON || UNITY_PS3 || UNITY_ANDROID || UNITY_PEPPER || UNITY_LINUX || UNITY_BB10 || UNITY_WII || UNITY_TIZEN)

#if !ATOMIC_API_GENERIC && SUPPORT_THREADS
#	include "PlatformAtomicOps.h"
#else

// This file contains implementations for the platforms we already support.
// Going forward (some of) these implementations must move to the platform specific directories.

#include "Utility.h"

#if UNITY_WIN
#include <intrin.h>
//typedef char UnityAtomicsTypesAssert_FailsIfIntSize_NEQ_LongSize[sizeof(int) == sizeof(LONG) ? 1 : -1];
#elif UNITY_OSX



#include <libkern/OSAtomic.h>

#elif UNITY_IPHONE
#include <libkern/OSAtomic.h>
#elif UNITY_PS3
#include <cell/atomic.h>
#elif UNITY_ANDROID || (UNITY_LINUX && defined(__GNUC__) && defined(__arm__))
// use gcc builtin __sync_*
#elif UNITY_BB10
#include <atomic.h>
#include <arm/smpxchg.h>
#endif

#include "Utility.h"

// AtomicAdd - Returns the new value, after the operation has been performed (as defined by OSAtomicAdd32Barrier)
FORCE_INLINE int AtomicAdd (int volatile* i, int value) {
#if UNITY_OSX || UNITY_IPHONE
#if defined(__ppc__)
#error "Atomic addition undefined for this platform"
#endif

	return OSAtomicAdd32Barrier (value, (int*)i);

#elif UNITY_WIN || UNITY_XENON
	return _InterlockedExchangeAdd ((long volatile*)i, value) + value;
#elif UNITY_PS3
	return cellAtomicAdd32((uint32_t*)i, value) + value;	// on ps3 it returns the pre-increment value
#elif UNITY_ANDROID || UNITY_TIZEN
	return __sync_add_and_fetch(i,value);
#elif UNITY_PEPPER
	int temp = value;
	__asm__ __volatile__("lock; xaddl %0,%1"
		: "+r" (temp), "+m" (*i)
		: : "memory");
	// temp now holds the old value of *ptr
	//	if (AtomicOps_Internalx86CPUFeatures.has_amd_lock_mb_bug)
	__asm__ __volatile__("lfence" : : : "memory");
	return temp + value;
#elif UNITY_LINUX
	return __sync_add_and_fetch(i, value);
#elif UNITY_BB10
	return atomic_add_value((unsigned int*)i, value)+value;
#elif UNITY_WII
	int wasEnabled = OSDisableInterrupts();
	*i = (*i) + value;
	OSRestoreInterrupts(wasEnabled);
	return *i;
#elif !SUPPORT_THREADS
	return *i+=value;
#else
#error "Atomic op undefined for this platform"
#endif
}

// AtomicSub - Returns the new value, after the operation has been performed (as defined by OSAtomicSub32Barrier)
inline int AtomicSub (int volatile* i, int value) {
#if UNITY_PS3
	return cellAtomicSub32((uint32_t*)i, value) - value;	// on ps3 it returns the pre-increment value
#elif UNITY_ANDROID || UNITY_TIZEN
	return __sync_sub_and_fetch(i,value);
#elif UNITY_LINUX
	return __sync_sub_and_fetch(i, value);
#elif UNITY_BB10
	return atomic_sub_value((unsigned int*)i, value) - value;
#else
	return AtomicAdd(i, -value);
#endif
}

// AtomicIncrement - Returns the new value, after the operation has been performed (as defined by OSAtomicAdd32Barrier)
FORCE_INLINE int AtomicIncrement (int volatile* i) {
#if UNITY_WIN || UNITY_XENON
	return _InterlockedIncrement ((long volatile*)i);
#elif UNITY_PS3
	return cellAtomicIncr32((uint32_t*)i)+1;	// on ps3 it returns the pre-increment value
#elif !SUPPORT_THREADS
	return ++*i;
#else
	return AtomicAdd(i, 1);
#endif
}

// AtomicDecrement - Returns the new value, after the operation has been performed (as defined by OSAtomicAdd32Barrier)
FORCE_INLINE int AtomicDecrement (int volatile* i) {
#if UNITY_WIN || UNITY_XENON
	return _InterlockedDecrement ((long volatile*)i);
#elif UNITY_PS3
	return cellAtomicDecr32((uint32_t*)i)-1;	// on ps3 it returns the pre-increment value
#elif !SUPPORT_THREADS
	return --*i;
#else
	return AtomicSub(i, 1);
#endif
}

// AtomicCompareExchange - Returns value is the initial value of the Destination pointer (as defined by _InterlockedCompareExchange)
#if UNITY_WIN || UNITY_XENON
FORCE_INLINE bool AtomicCompareExchange (int volatile* i, int newValue, int expectedValue) {
	return _InterlockedCompareExchange ((long volatile*)i, (long)newValue, (long)expectedValue) == expectedValue;
}
#elif UNITY_OSX || UNITY_IPHONE
FORCE_INLINE bool AtomicCompareExchange (int volatile* i, int newValue, int expectedValue) {
	return OSAtomicCompareAndSwap32Barrier (expectedValue, newValue, reinterpret_cast<volatile int32_t*>(i));
}
#elif UNITY_LINUX || UNITY_PEPPER || UNITY_ANDROID || UNITY_BB10 || UNITY_TIZEN
FORCE_INLINE bool AtomicCompareExchange (int volatile* i, int newValue, int expectedValue) {
#	if UNITY_BB10
	return _smp_cmpxchg((unsigned int*)i, expectedValue, newValue) == expectedValue;
#	else
	return __sync_bool_compare_and_swap(i, expectedValue, newValue);
#endif
}
#elif UNITY_PS3
FORCE_INLINE bool AtomicCompareExchange (int volatile* i, int newValue, int expectedValue) {
	return cellAtomicCompareAndSwap32((uint32_t*)i, (uint32_t)expectedValue, (uint32_t)newValue) == (uint32_t)expectedValue;
}
#endif

// AtomicExchange - Returns the initial value pointed to by Target (as defined by _InterlockedExchange)
#if UNITY_WIN || UNITY_XENON
FORCE_INLINE int AtomicExchange (int volatile* i, int value) {
	return (int)_InterlockedExchange ((long volatile*)i, (long)value);
}
#elif UNITY_ANDROID || UNITY_TIZEN || UNITY_OSX || UNITY_LINUX // fallback to loop
FORCE_INLINE int AtomicExchange (int volatile* i, int value) {
	int prev;
	do { prev = *i; }
	while (!AtomicCompareExchange(i, value, prev));
	return prev;
}
#endif

#endif // ATOMIC_API_GENERIC
#undef ATOMIC_API_GENERIC

#endif //UNITY_ATOMIC_OPS_HPP_
