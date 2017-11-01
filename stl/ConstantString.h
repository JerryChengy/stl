#pragma once
#include "AllocatorLabels.h"
#include "VisualStudioPostPrefix.h"
#include "string.h"
#define NULL 0
struct ConstantString
{
	ConstantString (const char* str, MemLabelId label)
		: m_Buffer (NULL)
	{
		assign (str, label);
	}

	ConstantString ()
		: m_Buffer (NULL)
	{
		create_empty();
	}

	ConstantString (const ConstantString& input)
		: m_Buffer (NULL)
	{
		assign(input);
	}

	~ConstantString ();

	void assign (const char* str, MemLabelId label);
	void assign (const ConstantString& input);

	void operator = (const ConstantString& input);

	const char* c_str() const { return get_char_ptr_fast (); }
	bool empty () const       { return m_Buffer[0] == 0; }

	friend bool operator == (const ConstantString& lhs, const ConstantString& rhs)
	{
		if (lhs.owns_string () || rhs.owns_string())
			return strcmp(lhs.c_str(), rhs.c_str()) == 0;
		else
			return lhs.m_Buffer == rhs.m_Buffer;
	}

	friend bool operator == (const ConstantString& lhs, const char* rhs)
	{
		return strcmp(lhs.c_str(), rhs) == 0;
	}

	friend bool operator == (const char* rhs, const ConstantString& lhs)
	{
		return strcmp(lhs.c_str(), rhs) == 0;
	}

	friend bool operator != (const ConstantString& lhs, const char* rhs)
	{
		return strcmp(lhs.c_str(), rhs) != 0;
	}

	friend bool operator != (const char* rhs, const ConstantString& lhs)
	{
		return strcmp(lhs.c_str(), rhs) != 0;
	}

private:

	inline bool owns_string () const              { return reinterpret_cast<size_t> (m_Buffer) & 1; }
	inline const char* get_char_ptr_fast () const { return reinterpret_cast<const char*> (reinterpret_cast<size_t> (m_Buffer) & ~1); }
	void cleanup ();
	void create_empty ();

	const char* m_Buffer;
};