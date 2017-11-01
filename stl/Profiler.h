#ifndef _PROFILER_H_
#define _PROFILER_H_

#include "UnityConfigure.h"

#if ENABLE_PROFILER

#include "EnumFlags.h"
#include "ScriptingTypes.h"

/*
 // Example of profiling a code block:
 // Define PROFILER_INFORMATION outside of a function
 
 PROFILER_INFORMATION (gMyReallyFastFunctionProfile, "MyClass.MyFunction", kProfilerRender)
 
 void MyFunction ()
 {
 PROFILER_AUTO (gMyReallyFastFunctionProfile, this or NULL);
 // or
 PROFILER_BEGIN (gMyReallyFastFunctionProfile, this or NULL);
 PROFILER_END
 }
 
 // PROFILER_AUTO_THREAD_SAFE etc. can be used if you are not sure if the code might be called from another thread
 // PROFILER_AUTO_INTERNAL etc. can be used if you do not want the profiler blocks to be in released 
 //                         unity builds only in internal developer builds
 
 */

enum ProfilerMode 
{
	kProfilerEnabled = 1 << 0, 
	kProfilerGame = 1 << 1, 
	kProfilerDeepScripts = 1 << 2, 
	kProfilerEditor = 1 << 3, 
};
ENUM_FLAGS(ProfilerMode);

class Object;
struct ProfilerSample;

// ProfilerHistory uses AddToChart to sum the different groups into different charts.
// Only kProfilerRender, kProfilerScripts, kProfilerPhysics, kProfilerGC, kProfilerVSync currently make any impact in the UI.
enum ProfilerGroup
{
	kProfilerRender,
	kProfilerScripts,
	kProfilerGUI,
	kProfilerPhysics,	
	kProfilerAnimation,
	kProfilerAI,
	kProfilerAudio,
	kProfilerParticles,
	kProfilerNetwork,
	kProfilerLoading,
	kProfilerOther,
	kProfilerGC,
	kProfilerVSync,
	kProfilerOverhead,
	kProfilerPlayerLoop,
	kProfilerGroupCount
};

enum GpuSection
{
	kGPUSectionOther,
	kGPUSectionOpaquePass,
	kGPUSectionTransparentPass,
	kGPUSectionShadowPass,
	kGPUSectionDeferedPrePass,
	kGPUSectionDeferedLighting,
	kGPUSectionPostProcess
};

struct EXPORT_COREMODULE ProfilerInformation
{
	ProfilerInformation (const char* const functionName, ProfilerGroup grp, bool warn = false );
	
	const char* name; // function
	UInt16 group; // ProfilerGroup
	enum { kDefault = 0, kScriptMonoRuntimeInvoke = 1, kScriptEnterLeave = 2 };
	UInt8 flags;
	UInt8 isWarning;

	void* intelGPAData;
};

void EXPORT_COREMODULE profiler_begin(ProfilerInformation* info, const Object* obj);
void EXPORT_COREMODULE profiler_end();

void EXPORT_COREMODULE profiler_begin_thread_safe(ProfilerInformation* info, const Object* obj);
void EXPORT_COREMODULE profiler_end_thread_safe();

ProfilerSample* EXPORT_COREMODULE mono_profiler_begin(ScriptingMethodPtr method, ScriptingClassPtr profileKlass, ScriptingObjectPtr instance);
void EXPORT_COREMODULE mono_profiler_end(ProfilerSample* beginsample);

void EXPORT_COREMODULE gpu_time_sample();

void profiler_begin_frame();
void profiler_end_frame();
void profiler_start_mode(ProfilerMode flags);
void profiler_end_mode(ProfilerMode flags);

// Create&destroy a profiler for a specific thread (Used by worker threads & GPU thread)
void profiler_initialize_thread (const char* name, bool separateBeginEnd);
void profiler_cleanup_thread ();

// API for worker threads & GfxThread
void profiler_set_active_seperate_thread (bool enabled);
void profiler_begin_frame_seperate_thread (ProfilerMode mode);
void profiler_end_frame_seperate_thread (int frameIDAndValid);
void profiler_disable_sampling_seperate_thread ();

// Profiler interface macros
#define PROFILER_INFORMATION(VAR_NAME, NAME, GROUP) static ProfilerInformation VAR_NAME(NAME, GROUP);
#define PROFILER_WARNING(VAR_NAME, NAME, GROUP) static ProfilerInformation VAR_NAME(NAME, GROUP, true);
#define PROFILER_AUTO(INFO, OBJECT_PTR) ProfilerAutoObject _PROFILER_AUTO_OBJECT_(&INFO, OBJECT_PTR);
#define PROFILER_BEGIN(INFO, OBJECT_PTR) profiler_begin (&INFO, OBJECT_PTR);
#define PROFILER_END profiler_end();

#define MONO_PROFILER_BEGIN(monomethod,monoclass,obj) ProfilerSample* beginsample = mono_profiler_begin (monomethod, monoclass,obj);
#define MONO_PROFILER_END mono_profiler_end(beginsample);

#define PROFILER_AUTO_THREAD_SAFE(INFO, OBJECT_PTR) ProfilerAutoObjectThreadSafe _PROFILER_AUTO_OBJECT_(&INFO, OBJECT_PTR);

#define GPU_TIMESTAMP() gpu_time_sample(); 
#define GPU_AUTO_SECTION(section) AutoGpuSection autoGpuSection(section);

struct ProfilerAutoObject
{
	ProfilerAutoObject (ProfilerInformation* info, const Object* obj) { /*profiler_begin(info, obj); */}
	~ProfilerAutoObject() { /*profiler_end();*/ }
};

struct ProfilerAutoObjectThreadSafe
{
	ProfilerAutoObjectThreadSafe (ProfilerInformation* info, const Object* obj) { profiler_begin_thread_safe (info, obj); }
	~ProfilerAutoObjectThreadSafe() { profiler_end_thread_safe(); }
};

extern GpuSection g_CurrentGPUSection;

class AutoGpuSection
{
public:
	AutoGpuSection(GpuSection section) { oldGPUSection = g_CurrentGPUSection; g_CurrentGPUSection = section; }
	~AutoGpuSection() { g_CurrentGPUSection = oldGPUSection; }
private:
	GpuSection oldGPUSection;
};


#else

#define PROFILER_INFORMATION(VAR_NAME, NAME, GROUP)
#define PROFILER_WARNING(VAR_NAME, NAME, GROUP)
#define PROFILER_AUTO(INFO, OBJECT_PTR) 
#define PROFILER_BEGIN(INFO, OBJECT_PTR) 
#define PROFILER_END 
#define MONO_PROFILER_BEGIN(monomethod,monoclass,obj)
#define MONO_PROFILER_END

#define PROFILER_AUTO_THREAD_SAFE(INFO, OBJECT_PTR) 

#define GPU_TIMESTAMP() 
#define GPU_AUTO_SECTION(section)


#endif


#if ENABLE_PROFILER_INTERNAL_CALLS
#define PROFILER_AUTO_INTERNAL(INFO, OBJECT_PTR) PROFILER_AUTO(INFO, OBJECT_PTR)
#define PROFILER_BEGIN_INTERNAL(INFO, OBJECT_PTR) PROFILER_BEGIN(INFO, OBJECT_PTR)
#define PROFILER_END_INTERNAL PROFILER_END

#define PROFILER_AUTO_THREAD_SAFE_INTERNAL(INFO, OBJECT_PTR) PROFILER_AUTO_THREAD_SAFE(INFO, OBJECT_PTR)
#else
#define PROFILER_AUTO_INTERNAL(INFO, OBJECT_PTR) 
#define PROFILER_BEGIN_INTERNAL(INFO, OBJECT_PTR) 
#define PROFILER_END_INTERNAL 

#define PROFILER_AUTO_THREAD_SAFE_INTERNAL(INFO, OBJECT_PTR) 
#endif

#endif /*_PROFILER_H_*/
