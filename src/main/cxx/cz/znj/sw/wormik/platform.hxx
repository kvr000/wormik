#include <stdint.h>

#ifdef _WIN32

#include <windows.h>
#include <excpt.h>
#include <windef.h>
#include <winbase.h>
#include <io.h>
#include <malloc.h>

#ifndef _POSIX_
#define _POSIX_
#endif

#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define open _open
#define popen _popen
#define pclose _pclose
#define alloca _alloca

inline int ftruncate(int fd, size_t size)
{
	return _chsize(fd, size);
}

#else

#include <unistd.h>

#endif
