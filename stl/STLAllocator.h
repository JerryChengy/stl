#ifndef STL_ALLOCATOR_H_
#define STL_ALLOCATOR_H_

#include "BaseAllocator.h"
#include "AllocatorLabels.h"
#include "MemoryMacros.h"
// Use STL_ALLOCATOR macro when declaring custom std::containers
#define STL_ALLOCATOR(label, type) stl_allocator<type, label##Id>
#define STL_ALLOCATOR_ALIGNED(label, type, align) stl_allocator<type, label##Id, align>

#if UNITY_EXTERNAL_TOOL
#define TEMP_STRING std::string
#else
#define TEMP_STRING std::basic_string<char, std::char_traits<char>, STL_ALLOCATOR(kMemTempAlloc, char) >
#endif

#define UNITY_STRING(label) std::basic_string<char, std::char_traits<char>, STL_ALLOCATOR(label, char) >
#define UNITY_WSTRING(label) std::basic_string<wchar_t, std::char_traits<wchar_t>, STL_ALLOCATOR(label, wchar_t) >

#define UNITY_LIST(label, type) std::list<type, STL_ALLOCATOR(label, type) >

#define UNITY_SET(label, type) std::set<type, std::less<type>, STL_ALLOCATOR(label, type) >

#define UNITY_MAP(label, key, value) std::map<key, value, std::less<key>, stl_allocator< std::pair< key const, value>, label##Id > >

#define UNITY_VECTOR(label, type) std::vector<type, STL_ALLOCATOR(label, type) >
#define UNITY_VECTOR_ALIGNED(label, type, align) std::vector<type, STL_ALLOCATOR_ALIGNED(label, type, align) >

#define UNITY_TEMP_VECTOR(type) std::vector<type, STL_ALLOCATOR(kMemTempAlloc, type) >


template<typename T, MemLabelIdentifier memlabel = kMemSTLId, int align = kDefaultMemoryAlignment >
class stl_allocator
{
public:
	typedef size_t    size_type;
	typedef ptrdiff_t difference_type;
	typedef T*        pointer;
	typedef const T*  const_pointer;
	typedef T&        reference;
	typedef const T&  const_reference;
	typedef T         value_type;
#if 0
	AllocationRootReference* rootref;
	ProfilerAllocationHeader* get_root_header() const { return get_root_header_from_reference(rootref); }
#else
	ProfilerAllocationHeader* get_root_header () const { return NULL; }
#endif
	template <typename U> struct rebind { typedef stl_allocator<U, memlabel, align> other; };

	stl_allocator () 
	{ 
		//IF_MEMORY_PROFILER_ENABLED( rootref = get_root_reference_from_header(GET_CURRENT_ALLOC_ROOT_HEADER()) ); 
	}
	stl_allocator (const stl_allocator& alloc) throw() 
	{ 
		//IF_MEMORY_PROFILER_ENABLED( rootref = copy_root_reference(alloc.rootref) ); 
	}
	template <typename U, MemLabelIdentifier _memlabel, int _align> stl_allocator (const stl_allocator<U, _memlabel, _align>& alloc) throw() 
	{ 
		//IF_MEMORY_PROFILER_ENABLED( rootref = copy_root_reference(alloc.rootref) ); 
	}
	~stl_allocator () throw() 
	{ 
		//IF_MEMORY_PROFILER_ENABLED( release_root_reference(rootref) );
	}

	pointer address (reference x) const { return &x; }
	const_pointer address (const_reference x) const { return &x; }

	pointer allocate (size_type count, void const* /*hint*/ = 0)
	{
		return (pointer)UNITY_MALLOC_ALIGNED( MemLabelId(memlabel, get_root_header()), count * sizeof(T), align);
	}
	void deallocate (pointer p, size_type /*n*/)
	{ 
		UNITY_FREE(MemLabelId(memlabel, get_root_header()), p); 
	}

	template <typename U, MemLabelIdentifier _memlabel, int _align>
	bool operator== (stl_allocator<U, _memlabel, _align> const& a) const {	return _memlabel == memlabel IF_MEMORY_PROFILER_ENABLED( && get_root_header() == a.get_root_header()); }
	template <typename U, MemLabelIdentifier _memlabel, int _align>
	bool operator!= (stl_allocator<U, _memlabel, _align> const& a) const {	return _memlabel != memlabel IF_MEMORY_PROFILER_ENABLED( || get_root_header() != a.get_root_header()); }

	size_type max_size () const throw()      {	return 0x7fffffff; }

	void construct (pointer p, const T& val) {	new (p) T(val); }

	void destroy (pointer p)                 {	p->~T(); }
};

#if !UNITY_EXTERNAL_TOOL && UNITY_WIN && !WEBPLUG
#define string mystlstring
#define wstring mystlwstring
#include <string>
#undef string
#undef wstring
#include <string>
namespace std{
	typedef UNITY_STRING(kMemString) string;
	typedef UNITY_WSTRING(kMemString) wstring;
}
#else
#include <string>
#endif

#define UNITY_STR_IMPL(StringName,label)      \
class StringName : public UNITY_STRING(label)      \
{      \
public:      \
	StringName():UNITY_STRING(label)(){}      \
	StringName(const char* str):UNITY_STRING(label)(str){}      \
	StringName(const char* str, int length):UNITY_STRING(label)(str,length){}      \
	StringName(const StringName& str):UNITY_STRING(label)(str.c_str(),str.length()){}      \
	template<typename alloc>      \
	StringName(const std::basic_string<char, std::char_traits<char>, alloc >& str):UNITY_STRING(label)(str.c_str(),str.length()){}      \
	template<typename alloc>      \
	operator std::basic_string<char, std::char_traits<char>, alloc > () const      \
{      \
	return std::basic_string<char, std::char_traits<char>, alloc >(this->c_str(), this->length());      \
}      \
	template<typename alloc>      \
	StringName& operator=(const std::basic_string<char, std::char_traits<char>, alloc >& rhs)      \
{      \
	assign(rhs.c_str(), rhs.length());      \
	return *this;      \
}      \
	template<typename alloc>      \
	bool operator==(const std::basic_string<char, std::char_traits<char>, alloc >& rhs) const      \
{      \
	return length() == rhs.length() && strncmp(c_str(), rhs.c_str(), length()) == 0;      \
}      \
	template<typename alloc>      \
	bool operator!=(const std::basic_string<char, std::char_traits<char>, alloc >& rhs) const      \
{      \
	return length() != rhs.length() || strncmp(c_str(), rhs.c_str(), length()) != 0;      \
}      \
};

UNITY_STR_IMPL(UnityStr, kMemString);
UNITY_STR_IMPL(StaticString, kMemStaticString);

#define ProfilerString UNITY_STRING(kMemMemoryProfilerString)

#endif
