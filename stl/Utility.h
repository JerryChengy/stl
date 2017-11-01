#ifndef UTILITY_H
#define UTILITY_H

#include "MemoryMacros.h"

#define UNUSED(x)	(void)sizeof(x)

#define SAFE_RELEASE(obj) if (obj) { (obj)->Release(); (obj) = NULL; } else {}
#define SAFE_RELEASE_LABEL(obj,label) if (obj) { (obj)->Release(label); (obj) = NULL; } else {}

template <class T0, class T1>
inline void CopyData (T0 *dst, const T1 *src, long int inHowmany)
{
	for (long int i=0;i<inHowmany;i++)
	{
		dst[i] = src[i];
	}
}

template <class DataType>
inline void CopyOverlappingData (DataType *dst, const DataType *src, long int inHowmany)
{
	if (dst < src)
	{
		for (long int i=0;i<inHowmany;i++)
		{
			dst[i] = src[i];
		}
	}
	else if (dst > src)
	{
		for (long int i=inHowmany-1;i>=0;i--)
		{
			dst[i] = src[i];
		}
	}
}

template <class DataType>
inline void CopyRange (DataType *dst, const DataType *src, long int inStart, long int inHowmany)
{
	for (long int i=inStart;i<inHowmany+inStart;i++)
	{
		dst[i] = src[i];
	}
}

template <class DataType>
inline void CopyData (DataType *dst, const DataType src, long int inHowmany)
{
	for (long int i=0;i<inHowmany;i++)
	{
		dst[i] = src;
	}
}

template <class DataType>
inline void CopyDataReverse (DataType *dst, const DataType *src, long int inHowmany)
{
	for (long int i=0;i<inHowmany;i++)
	{
		dst[i] = src[inHowmany-1-i];
	}
}

template <class DataType>
inline bool CompareArrays (const DataType *lhs, const DataType *rhs, long int arraySize)
{
	for (long int i=0; i<arraySize; i++)
	{
		if (lhs[i] != rhs[i])
			return false;
	}
	return true;
}

template <class DataType>
inline bool CompareMemory (const DataType& lhs, const DataType& rhs)
{
#ifdef ALIGN_OF
	// We check at compile time if it's safe to cast data to int*
	if (ALIGN_OF(DataType) >= ALIGN_OF(int) && (sizeof(DataType) % sizeof(int))==0)
	{
		return CompareArrays((const int*)&lhs, (const int*)&rhs, sizeof(DataType) / sizeof(int));
	}
#endif
	return CompareArrays((const char*)&lhs, (const char*)&rhs, sizeof(DataType));
}

template <class T>
class UNITY_AutoDelete
{
public:
	UNITY_AutoDelete() : m_val(T()) { }
	~UNITY_AutoDelete() { if(m_val) UNITY_DELETE ( m_val, m_label ); }

	void Assign(T val, MemLabelId label) { m_val = val; m_label = label; return *this; }
	bool operator!() { return !m_val; }

	/* Releases the internal pointer without deleting */
	T releasePtr() { T tmp = m_val; m_val = T(); return tmp; }
private:
	UNITY_AutoDelete &operator=(T val);
	UNITY_AutoDelete(const UNITY_AutoDelete<T>& other); // disabled
	void operator=(const UNITY_AutoDelete<T>& other); // disabled
	T m_val;
	MemLabelId m_label;
};

class AutoFree
{
public:
	AutoFree() : m_val(NULL), m_label(kMemDefault) { }
	~AutoFree() { if(m_val) UNITY_FREE ( m_label, m_val ); }

	bool operator!() { return !m_val; }
	void Assign(void* val, MemLabelId label) { m_label = label; m_val = val; }

	/* Releases the internal pointer without deleting */
	void* releasePtr() { void* tmp = m_val; m_val = NULL; return tmp; }
private:
	AutoFree &operator=(void* val); // disabled
	AutoFree(const AutoFree& other); // disabled
	void operator=(const AutoFree& other); // disabled
	void* m_val;
	MemLabelId m_label;
};

template <class T>
inline T clamp (const T&t, const T& t0, const T& t1)
{
	if (t < t0)
		return t0;
	else if (t > t1)
		return t1;
	else
		return t;
}

template <>
inline float clamp (const float&t, const float& t0, const float& t1)
{
#if UNITY_XENON || UNITY_PS3
	return FloatMin(FloatMax(t, t0), t1);
#else
	if (t < t0)
		return t0;
	else if (t > t1)
		return t1;
	else
		return t;
#endif
}

template <class T>
inline T clamp01 (const T& t)
{
	if (t < 0)
		return 0;
	else if (t > 1)
		return 1;
	else
		return t;
}

template <>
inline float clamp01<float> (const float& t)
{
#if UNITY_XENON || UNITY_PS3
	return FloatMin(FloatMax(t, 0.0f), 1.0f);
#else
	if (t < 0.0F)
		return 0.0F;
	else if (t > 1.0F)
		return 1.0F;
	else
		return t;
#endif
}

// Asserts if from is NULL or can't be cast to type To
template<class To, class From> inline
	To assert_cast (From from)
{
	AssertIf (dynamic_cast<To> (from) == NULL);
	return static_cast<To> (from);
}

inline float SmoothStep (float from, float to, float t) 
{
	t = clamp01(t);
	t = -2.0F * t*t*t + 3.0F * t*t;
	return to * t + from * (1.0f - t);
}
// Rounds value down.
// Note: base must be power of two value.
inline UInt32 RoundDown (UInt32 value, SInt32 base)
{
	return value & (-base);
}
// Rounds value up.
// Note: base must be power of two value.
inline UInt32 RoundUp (UInt32 value, SInt32 base)
{
	return (value + base - 1) & (-base);
}

template<class T>
inline T* Stride (T* p, size_t offset)
{
	return reinterpret_cast<T*>((char*)p + offset);
}

#endif // include-guard
