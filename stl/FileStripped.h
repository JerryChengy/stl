
#ifndef __FILE_STRIPPED__

#if ENABLE_PROFILER || DEBUGMODE
#define __FILE_STRIPPED__ __FILE__
#else
#define __FILE_STRIPPED__ ""
#endif

#endif