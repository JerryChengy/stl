#ifndef _SCRIPTINGTYPES_H_
#define _SCRIPTINGTYPES_H_

#if ENABLE_MONO
struct MonoObject;
struct MonoString;
struct MonoArray;
struct MonoType;
struct MonoClass;
struct MonoException;
struct MonoMethod;
struct MonoClassField;
typedef MonoObject ScriptingObject;
typedef MonoString ScriptingString;
typedef MonoArray ScriptingArray;
typedef MonoClass ScriptingType;
typedef MonoClass ScriptingClass;
typedef MonoException ScriptingException;
typedef MonoClassField ScriptingField;
struct ScriptingMethod;
typedef MonoMethod* BackendNativeMethod;
typedef MonoClass* BackendNativeType;

#elif UNITY_FLASH
struct ScriptingObject;
struct ScriptingString;
struct ScriptingArray;
struct ScriptingClass;
typedef ScriptingClass ScriptingType;
struct ScriptingException;

struct ScriptingMethod;
struct ScriptingField;
struct MonoImage;

//hack to not have to change everything in one go.
struct MonoMethod;
struct MonoObject;
typedef ScriptingType MonoClass; 
typedef void* BackendNativeType;
typedef void* BackendNativeMethod;
#elif UNITY_WINRT && ENABLE_SCRIPTING
struct WinRTNullPtrImplementation
{
	inline operator long long()
	{
		return -1;
	}
	inline operator decltype(__nullptr)()
	{
		return nullptr;
	}
};

// Implemented in WinRTUtility.cpp
class WinRTScriptingObjectWrapper
{
public:
	// Must be in sync with WinRTBridge\GCHandledObjects.cs, would be nice to somehow get those values directly...
	static const int kMaxHandlesShift = 11;
	static const int kMaxHandlesPerBlock = 1 << kMaxHandlesShift;
	static const int kHandleMask = kMaxHandlesPerBlock - 1;
public:
	WinRTScriptingObjectWrapper();
	WinRTScriptingObjectWrapper(decltype(__nullptr));
	WinRTScriptingObjectWrapper(long long handleId);
	WinRTScriptingObjectWrapper(const WinRTNullPtrImplementation& interopParam);
	WinRTScriptingObjectWrapper(const WinRTScriptingObjectWrapper& handleId);
	~WinRTScriptingObjectWrapper();

	const WinRTScriptingObjectWrapper &operator = (const WinRTScriptingObjectWrapper &ptr);
	WinRTScriptingObjectWrapper& operator=(decltype(__nullptr));
	WinRTScriptingObjectWrapper& operator=(const WinRTNullPtrImplementation& nullptrimpl);
	inline operator bool () const 
	{ 
		return m_Handle != -1;
	}
	inline operator long long() const
	{
		return m_Handle;
	}
	int GetReferenceCount() const;

	// Converters
	bool ToBool();

	inline long long GetHandle() const { return m_Handle;}
	inline int GetBlockId() const {return m_BlockId; }
	inline int GetHandleInBlock() const { return m_HandleInBlock; }

	void Validate();

	static void FreeHandles();
	static void ClearCache();
	static void ValidateCache();
private:
	friend bool operator == (const WinRTScriptingObjectWrapper& a, decltype(__nullptr));
	friend bool operator != (const WinRTScriptingObjectWrapper& a, decltype(__nullptr));
	friend bool operator == (const WinRTScriptingObjectWrapper& a, const WinRTNullPtrImplementation& b);
	friend bool operator != (const WinRTScriptingObjectWrapper& a, const WinRTNullPtrImplementation& b);
	static int* GetPtrToReferences(int blockId);

	void Init(long long handleId);
	void InternalAddRef();
	void InternalRelease();

	long long m_Handle;
	int m_BlockId;
	int m_HandleInBlock;
	int* m_PtrToReferences;

	static const int kMaxCachedPtrToReferences = 4096; 
	static int* m_CachedPtrToReferences[kMaxCachedPtrToReferences];
	static int m_CachedBlockCount;
};

bool operator == (const WinRTScriptingObjectWrapper& a, decltype(__nullptr));
bool operator != (const WinRTScriptingObjectWrapper& a, decltype(__nullptr));
bool operator == (const WinRTScriptingObjectWrapper& a, const WinRTNullPtrImplementation& b);
bool operator != (const WinRTScriptingObjectWrapper& a, const WinRTNullPtrImplementation& b);
// Don't implement this, as same objects can have different handle ids, so currently there's no way of effectively comparing them
//bool operator == (const WinRTScriptingObjectWrapper& a, const WinRTScriptingObjectWrapper& b);

struct ScriptingObject;
struct ScriptingString;
typedef Platform::Object ScriptingArray;
//struct ScriptingArray;
struct ScriptingType;
typedef ScriptingType ScriptingClass;
struct ScriptingException;

typedef int BackendNativeMethod;
typedef BridgeInterface::IScriptingClassWrapper^ BackendNativeType;

struct ScriptingMethod;
struct ScriptingField;
struct MonoImage;

//hack to not have to change everything in one go.
struct MonoMethod;
struct MonoObject;
typedef ScriptingType MonoClass; 
#else
struct ScriptingObject;
struct ScriptingString;
struct ScriptingException;
struct ScriptingMethod;
struct ScriptingField;
struct ScriptingString;
struct ScriptingClass;
struct ScriptingType;
struct ScriptingArray;
struct ScriptingString;
struct MonoMethod;
struct MonoObject;
struct MonoClass;
typedef void* BackendNativeType;
typedef void* BackendNativeMethod;
#endif


#if UNITY_WINRT && ENABLE_SCRIPTING
typedef WinRTScriptingObjectWrapper ScriptingObjectPtr;
typedef Platform::Object^ ScriptingStringPtr;
typedef WinRTScriptingObjectWrapper ScriptingArrayPtr;
typedef WinRTScriptingObjectWrapper ScriptingPinnedArrayPtr;
typedef WinRTScriptingObjectWrapper const ScriptingArrayConstPtr;
typedef WinRTScriptingObjectWrapper ScriptingPinnedArrayConstPtr;
typedef ScriptingType* ScriptingTypePtr;
typedef ScriptingClass* ScriptingClassPtr;
typedef ScriptingException* ScriptingExceptionPtr;
typedef ScriptingMethod* ScriptingMethodPtr;
typedef ScriptingField* ScriptingFieldPtr;
//typedef Platform::Array<Platform::Object^> ScriptingParams;
//typedef ScriptingParams^ ScriptingParamsPtr;
typedef long long ScriptingParams;
typedef long long* ScriptingParamsPtr;
#define ScriptingBool bool
#define SCRIPTING_NULL WinRTNullPtrImplementation()
#define ScriptingObjectComPtr(Type) Microsoft::WRL::ComPtr<IInspectable>
#define ScriptingObjectToComPtr(Object) reinterpret_cast<IInspectable*>(Object)
#define ScriptingComPtrToObject(Type, Object) reinterpret_cast<Type>((Object).Get())
#else
typedef ScriptingObject* ScriptingObjectPtr;
typedef ScriptingString* ScriptingStringPtr;
typedef ScriptingArray* ScriptingArrayPtr;
typedef ScriptingType* ScriptingTypePtr;
typedef ScriptingClass* ScriptingClassPtr;
typedef ScriptingException* ScriptingExceptionPtr;
typedef ScriptingMethod* ScriptingMethodPtr;
typedef ScriptingField* ScriptingFieldPtr;
typedef void* ScriptingParams;
typedef ScriptingParams* ScriptingParamsPtr;
#define ScriptingBool short
#define SCRIPTING_NULL NULL
#define ScriptingObjectComPtr(Type) Type
#define ScriptingObjectToComPtr(Object) Object
#define ScriptingComPtrToObject(Type, Object) Object
#endif


#endif
