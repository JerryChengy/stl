#include "UnityPrefix.h"
#include "UnityConfigure.h"
#include "LinearAllocator.h"


#if 0

struct FwdAllocatorMarker {
	FwdAllocatorMarker (ForwardLinearAllocator& al) : al_(al), p_(al.current ()) {}
	~FwdAllocatorMarker () { al_.rewind (p_); }

	void*					p_;
	ForwardLinearAllocator&	al_;
};


void TestLinearAllocator ()
{
	ForwardLinearAllocator la (32);

	void* p0 = la.allocate (16);
	void* p1 = la.allocate (16);

	{
		FwdAllocatorMarker m (la);
		void* p2 = la.allocate (32);
	}

	void* c1 = la.current ();
	{
		FwdAllocatorMarker m (la);
		void* p3 = la.allocate (8);
		void* p4 = la.allocate (32);
		void* p5 = la.allocate (32);
	}
	la.rewind (c1);
}
#endif
