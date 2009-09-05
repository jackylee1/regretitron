#ifndef __utility_h__
#define __utility_h__

#include <string>
#include <sstream>
#include <vector>
#include <poker_defs.h>

// how to get a 64 bit integer type

#ifdef _MSC_VER
typedef __int64 int64;
typedef unsigned __int64 uint64;
#elif __GNUC__ 
#include <stdint.h>
typedef int64_t int64;
typedef uint64_t uint64;
#endif

//functions defined in the cpp file

extern double getdoubletime(); //returns time since some (any) refence in seconds
enum report_t{ KILL, WARN, INFO };
extern void REPORT(std::string infomsg, report_t killswitch = KILL);
extern void checkdataformat(); //performs the magic numbers test
template <typename T> 
extern std::string tostring(const T &myobj);
extern std::string tostring(CardMask cards);
extern std::string tostring(const std::vector<CardMask> &cards);
extern std::string gameroundstring(int gr);
extern void setdirectory(std::string directory);
extern std::string getdirectory();
extern std::string space(int64 bytes);
extern bool file_exists(std::string filename);

//how to get strings of things!

template <typename T> 
std::string tostring(const T &myobj)
{
	std::ostringstream o;
	o << myobj;
	return o.str();
}

//how to get string of preprocessor symbol.

#define STRINGIFY(symb) #symb
#define TOSTRING(symb) STRINGIFY(symb)

// how to hard code a breakpoint

#ifdef _MSC_VER //prefer these to be define's instead of inlines
#define dobreakpoint() _asm {int 3}
#elif __GNUC__
#define dobreakpoint() __asm__("int $3")
#endif

// how to pause and wait for the user

#ifdef _MSC_VER
inline void PAUSE() { system("PAUSE"); }
#elif __GNUC__
#include <iostream>
inline void PAUSE() { std::cout << "Press [enter] to continue . . ."; getchar(); }
#endif

// how to convert a std::string to a CString

#if defined _MFC_VER
inline CString toCString(const std::string &stdstr) { return CString(stdstr.c_str()); }
#endif

// functions that combine multiple indices into one index

inline int64 combine(int64 i2, int64 i1, int64 n1)
	{ return i2*n1 + i1; }

inline int64 combine(int64 i3, int64 i2, int64 n2, int64 i1, int64 n1)
	{ return i3*n2*n1 + i2*n1 + i1; }

inline int64 combine(int64 i4, int64 i3, int64 n3, int64 i2, int64 n2, int64 i1, int64 n1)
	{ return i4*n3*n2*n1 + i3*n2*n1 + i2*n1 + i1; }

#endif
