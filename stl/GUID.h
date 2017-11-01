#ifndef UNITY_GUID_H
#define UNITY_GUID_H

#include "SerializeUtility.h"
#include <string>

enum { kGUIDStringLength = 32 };

#define UNITY_HAVE_GUID_INIT (UNITY_OSX || UNITY_WIN || UNITY_IPHONE || UNITY_ANDROID || UNITY_LINUX || UNITY_PEPPER || UNITY_PS3 || UNITY_XENON || UNITY_BB10 || UNITY_TIZEN)

// To setup the unique identifier use Init ().
// You can compare it against other unique identifiers
// It is guaranteed to be unique over space and time
//
// Called UnityGUID because Visual Studio really does not like structs named GUID!
struct UnityGUID
{
	UInt32 data[4];

	// Used to be called GUID, so for serialization it has the old name
	DECLARE_SERIALIZE_OPTIMIZE_TRANSFER (GUID)

	UnityGUID (UInt32 a, UInt32 b, UInt32 c, UInt32 d) { data[0] = a; data[1] = b; data[2] = c; data[3] = d; }
	UnityGUID ()  { data[0] = 0; data[1] = 0; data[2] = 0; data[3] = 0; }

	bool operator == (const UnityGUID& rhs) const {
		return data[0] == rhs.data[0] && data[1] == rhs.data[1] && data[2] == rhs.data[2] && data[3] == rhs.data[3];
	}
	bool operator != (const UnityGUID& rhs) const { return !(*this == rhs); }

	bool operator < (const UnityGUID& rhs) const { return CompareGUID (*this, rhs) == -1; }
	friend int CompareGUID (const UnityGUID& lhs, const UnityGUID& rhs);

	// Use this instead of guid != UnityGUID() -- Will often result in self-documented code
	bool IsValid() const { return data[0] != 0 || data[1] != 0 || data[2] != 0 || data[3] != 0; }

#if UNITY_HAVE_GUID_INIT
	void Init ();
#endif
};

std::string GUIDToString(const UnityGUID& guid);
void GUIDToString(const UnityGUID& guid, char* string);

UnityGUID StringToGUID (const std::string& guidString);
UnityGUID StringToGUID (const char* guidString, size_t stringLength);

inline int CompareGUID (const UnityGUID& lhs, const UnityGUID& rhs)
{
	for (int i=0;i<4;i++)
	{
		if (lhs.data[i] < rhs.data[i])
			return -1;
		if (lhs.data[i] > rhs.data[i])
			return 1;
	}
	return 0;
}

bool CompareGUIDStringLess (const UnityGUID& lhs, const UnityGUID& rhs);

template<class TransferFunction>
void UnityGUID::Transfer (TransferFunction& transfer)
{
	TRANSFER (data[0]);
	TRANSFER (data[1]);
	TRANSFER (data[2]);
	TRANSFER (data[3]);
}

extern const char kHexToLiteral[16];

#endif
