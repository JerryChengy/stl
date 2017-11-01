// Utility functions and types for working with static types.
#pragma once

template<typename T1, typename T2>
struct IsSameType
{
	static const bool result = false;
};

template<typename T>
struct IsSameType<T, T>
{
	static const bool result = true;
};

template<typename Expected, typename Actual>
inline bool IsOfType (Actual& value)
{
	return IsSameType<Actual, Expected>::result;
}
