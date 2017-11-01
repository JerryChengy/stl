
// defines for missing functions
#define isnan _isnan
#if _MSC_VER < 1500
#define vsnprintf _vsnprintf
#endif
#define snprintf _snprintf
#define strdup _strdup
using namespace std;

/// Missing cdefs.h for yacc-generated stuff
#define __IDSTRING(name,string) static const char name[] = string
#define __P(a) a

#if defined(_XBOX) || defined(XBOX)
#include <malloc.h>
#endif