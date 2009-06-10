#ifndef __utility_h__
#define __utility_h__

#ifdef _MSC_VER

#define ASMBRK _asm {int 3}
#define PAUSE() system("PAUSE")
typedef __int64 int64;
typedef unsigned __int64 uint64;
#include <windows.h> // for QueryPerformanceCounter
#define GETDBLTIME(seconds) do{ \
	LARGE_INTEGER ticksPerSecond, tick; \
	QueryPerformanceFrequency(&ticksPerSecond); \
	QueryPerformanceCounter(&tick); \
	seconds = (double)tick.QuadPart/ticksPerSecond.QuadPart \
} while(0)

#elif __GNUC__

#include <stdlib.h> //for LINUX exit function exit(int) (used below)
#define ASMBRK __asm__("int $3")
#define PAUSE() do{ \
	std::cout << "Press [enter] to continue . . ."; \
	getchar(); \
}while(0)
typedef int64_t int64;
typedef uint64_t uint64;
#include <sys/time.h> //for gettimeofday
#define GETDBLTIME(seconds) do{ \
	timeval mytv; \
	gettimeofday(&mytv, NULL); \
	seconds = mytv.tv_sec + (double)mytv.tv_usec / 1000000.0; \
} while(0)

#else

#error unsupported compiler type?

#endif



#ifdef USE_MFC //if MFC we can pop up a sweet message box
#define REPORT(chars) do{ \
    AfxMessageBox(TEXT(chars));\
	ASMBRK; \
	exit(-1); \
}while(0)
#else
#include <iostream>
#define REPORT(chars) do{ \
	std::cout << chars << std::endl; \
	ASMBRK; \
	exit(-1); \
}while(0)
#endif

#define COMPILE_ASSERT(x) extern int __dummy[(int)x]
COMPILE_ASSERT(sizeof(int64) == 8 && sizeof(uint64) == 8);

#define COMBINE(i2, i1, n1) ((i2)*(n1) + (i1))

#define PRINTMASK(c) GenericDeck_maskString(&StdDeck, &c)

#endif
