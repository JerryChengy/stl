#ifndef __MUTEX_H
#define __MUTEX_H

#if SUPPORT_THREADS

#if UNITY_WIN || UNITY_XENON
#	include "PlatformMutex.h"
#elif UNITY_OSX || UNITY_IPHONE || UNITY_ANDROID || UNITY_PEPPER || UNITY_LINUX || UNITY_BB10 || UNITY_TIZEN
#	include "Posix/PlatformMutex.h"
#else
#	include "PlatformMutex.h"
#endif

#include "Runtime/Utilities/NonCopyable.h"

/**
 *	A mutex class. Always recursive (a single thread can lock multiple times).
 */
class Mutex : public NonCopyable
{
public:
	
	class AutoLock
	{
	public:
		AutoLock( Mutex& mutex )
		: m_Mutex(&mutex)
		{
			mutex.Lock();
		}
		
		~AutoLock()
		{
			m_Mutex->Unlock();
		}
		
	private:
		AutoLock(const AutoLock&);
		AutoLock& operator=(const AutoLock&);
		
	private:
		Mutex*	m_Mutex;
	};

	Mutex();
	~Mutex();
	
	void Lock();
	void Unlock();

	// Returns true if locking succeeded
	bool TryLock();

	// Returns true if the mutex is currently locked
	bool IsLocked();
	
	void BlockUntilUnlocked();

private:
	
	PlatformMutex	m_Mutex;
};

#else

// Used for threadsafe refcounting
class Mutex
{
public:
	
	class AutoLock
	{
	public:
		AutoLock( Mutex& mutex ){}
		~AutoLock()	{}
	};

	bool TryLock () { return true; }
	void Lock () {  }
	void Unlock () {  }
	bool IsLocked () { return false; }
	void BlockUntilUnlocked() {  }
};

#endif //SUPPORT_THREADS
#endif
