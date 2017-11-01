#ifndef STACKTRACE_H
#define STACKTRACE_H

void InitializeStackWalker();

std::string GetStacktrace(bool includeMonoStackFrames = false);

//can't use string version when debugging new/delete allocations, as string class may allocate memory itself!
void GetStacktrace(char *trace, int maxSize, int maxFrames = INT_MAX);
UInt32 GetStacktrace(void** trace, int maxFrames, int startframe);
void GetReadableStackTrace(char* output, int bufsize, void** input, int count);
#endif
