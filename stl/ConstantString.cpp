#include "ConstantString.h"
#include "ConstantStringManager.h"
#include "AtomicOps.h"
#include "MemoryManager.h"

///@TODO: Use 24 bits for refcount
///@TODO: Handle ref count overflow
#define Assert(x)
void ConstantString::create_empty ()
{
	cleanup();
	m_Buffer = GetConstantStringManager().GetEmptyString();
	Assert(!owns_string());
}

// The header compressed the label and refcount into a 32 bits.
// Refcount lives on the 0xFFFF, label lives on the higher bits.
// We use atomic ops for thread safety on refcounting when deleting ConstantStrings.
struct AllocatedStringHeader
{
	volatile int refCountAndLabel;
};

MemLabelId GetLabel (const AllocatedStringHeader& header)
{
	int intLabel = (header.refCountAndLabel & (0x0000FFFF)) << 16;
	ProfilerAllocationHeader* root = GET_ALLOC_HEADER(&GetConstantStringManager(), kMemString);
	MemLabelId label( (MemLabelIdentifier)intLabel, root);
	return label;
}

void SetLabel (AllocatedStringHeader& header, MemLabelId label)
{
	int labelInt = label.label;
	Assert(labelInt < 0xFFFF);
	labelInt <<= 16;

	header.refCountAndLabel &= 0x0000FFFF;
	header.refCountAndLabel |= labelInt;
}

int GetRefCount (int refCountAndLabel)
{
	return refCountAndLabel & 0xFFFF;
}


inline static AllocatedStringHeader* GetHeader (const char* ptr)
{
	return reinterpret_cast<AllocatedStringHeader*> (const_cast<char*> (ptr) - sizeof(AllocatedStringHeader));
}

void ConstantString::operator = (const ConstantString& input)
{
	assign(input);
}

void ConstantString::assign (const ConstantString& input)
{
	cleanup();
	m_Buffer = input.m_Buffer;
	if (owns_string())
	{
		AtomicIncrement(&GetHeader (get_char_ptr_fast())->refCountAndLabel);
	}
}


void ConstantString::assign (const char* str, MemLabelId label)
{
	cleanup();
	const char* constantString = GetConstantStringManager().GetConstantString(str);
	// Own Strings
	if (constantString == NULL)
	{
		//label.SetRootHeader(GET_ALLOC_HEADER(&GetConstantStringManager(), kMemString));
		size_t length = strlen(str);
		//char* allocated = (char*)GetMemoryManager ().Allocate(length + 1 + sizeof(AllocatedStringHeader), kDefaultMemoryAlignment, label, kAllocateOptionNone, __FILE_STRIPPED__, __LINE__);
		char* allocated = (char*)UNITY_MALLOC (label, length + 1 + sizeof(AllocatedStringHeader));
		char* allocatedString = allocated + sizeof(AllocatedStringHeader);

		AllocatedStringHeader& header = *GetHeader (allocatedString);
		header.refCountAndLabel = 1;
		SetLabel(header, label);

		Assert(GetRefCount(header.refCountAndLabel) == 1);
		memcpy(allocatedString, str, length);
		allocatedString[length] = 0;


		m_Buffer = reinterpret_cast<char*> (reinterpret_cast<size_t> (allocatedString) | 1);
		Assert(owns_string());
	}
	else
	{
		m_Buffer = constantString;
		Assert(!owns_string());
	}
}

void ConstantString::cleanup ()
{
	if (owns_string())
	{
		AllocatedStringHeader* header = GetHeader(get_char_ptr_fast ());

		int newRefCount = AtomicDecrement (&header->refCountAndLabel);
		newRefCount = GetRefCount(newRefCount);

		if (newRefCount == 0)
		{
		//	int intLabel = (header->refCountAndLabel & (0x0000FFFF)) << 16;
			//ProfilerAllocationHeader* root = GET_ALLOC_HEADER(&GetConstantStringManager(), kMemString);
		//	MemLabelId label( (MemLabelIdentifier)intLabel, root);
			//MemLabelId label = GetLabel(*header);
			//UNITY_FREE(label, header);
		}
	}

	m_Buffer = NULL;
}

ConstantString::~ConstantString ()
{
	cleanup ();
}
