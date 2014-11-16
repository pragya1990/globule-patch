/* This file exists because httpd.h from Apache disables the strtoul method
 * which is needed in VC7's cstdlib. The error message looks as follows:
 * C:\Program Files\Microsoft Visual Studio .NET 2003\Vc7\include\cstdlib(27) : error C2039: 'strtoul_is_not_a_portable_function_use_strtol_instead' : is not a member of 'operator``global namespace'''
 * C:\Program Files\Microsoft Visual Studio .NET 2003\Vc7\include\cstdlib(27) : error C2873: 'strtoul_is_not_a_portable_function_use_strtol_instead' : symbol cannot be used in a using-declaration
 */

/* Include the original httpd.h */
#include "C://Apache2/include/httpd.h"

/* Undo the definition set by httpd.h */
#ifdef WIN32

#undef strtoul


/* Automake generates these (in configure), but Windows does not have this */
#define PACKAGE "mod-globule"
#define VERSION "1.3.2-win32"

/* Windows truely sucks, why do they define such a common name? */
#undef ReportEvent
#define ReportEvent GlobuleReportEvent

#pragma warning( disable : 4290 )
#pragma warning( disable : 4800 )

#endif /* WIN32 */
