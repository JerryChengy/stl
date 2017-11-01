#pragma once
#include "UnityConfigure.h"
#include "TypeUtilities.h"

#ifndef SUPPORT_CPP0X_STATIC_ASSERT
#if (defined _MSC_VER) && (_MSC_VER >= 1600)
#define SUPPORT_CPP0X_STATIC_ASSERT 1
#elif (defined  __has_extension)
#define SUPPORT_CPP0X_STATIC_ASSERT __has_extension(cxx_static_assert)
#else
#define SUPPORT_CPP0X_STATIC_ASSERT 0
#endif
#endif

#if SUPPORT_CPP0X_STATIC_ASSERT
#define CompileTimeAssert(expression, message) static_assert(expression, message)
#else
template<bool>  struct CompileTimeAssertImpl;
template<>      struct CompileTimeAssertImpl<true> {};

#define CT_ASSERT_HACK_JOIN(X,Y) CT_ASSERT_HACK_DO_JOIN(X,Y)
#define CT_ASSERT_HACK_DO_JOIN(X,Y) CT_ASSERT_HACK_DO_JOIN2(X,Y)
#define CT_ASSERT_HACK_DO_JOIN2(X,Y) X##Y

#define CompileTimeAssert(expression, message)                                                      \
	enum{ CT_ASSERT_HACK_JOIN(ct_assert_, __LINE__) = sizeof(CompileTimeAssertImpl<(expression)>) }
#endif


#ifndef SUPPORT_CPP0X_DECLTYPE
#if (defined _MSC_VER) && (_MSC_VER >= 1600)
#define SUPPORT_CPP0X_DECLTYPE 1
#elif (defined  __has_extension)
#define SUPPORT_CPP0X_DECLTYPE __has_extension(cxx_decltype)
#else
#define SUPPORT_CPP0X_DECLTYPE 0
#endif
#endif


#if SUPPORT_CPP0X_STATIC_ASSERT && SUPPORT_CPP0X_DECLTYPE
/// Usage Example:
/// int a;
/// double b;
/// AssertVariableType(a, int); // OK
/// AssertVariableType(b, int); // Error during compile time
/// AssertIfVariableType(a, int); // Error during compile time
/// AssertIfVariableType(b, int); // OK

#define AssertVariableType(Variable, Type)      CompileTimeAssert(IsSameType<Type,decltype(Variable)>::result, #Variable" is not of type "#Type)
#define AssertIfVariableType(Variable, Type)    CompileTimeAssert(!IsSameType<Type,decltype(Variable)>::result, #Variable" is of type "#Type)
#else
#define AssertVariableType(Variable, Type)      do { (void)sizeof(Variable); } while(0)
#define AssertIfVariableType(Variable, Type)    do { (void)sizeof(Variable); } while(0)
#endif
