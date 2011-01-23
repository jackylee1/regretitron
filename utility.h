#ifndef __utility_h__
#define __utility_h__

#include <string>
#include <string.h>
#include <sstream>
#include <vector>
#include <poker_defs.h>
#ifdef _MSC_VER
#include <windows.h>
#else
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "MersenneTwister.h"

// the version of my portfolio xml document

const int PORTFOLIO_VERSION = 1;

// this is useful for debugging playoff

#define PLAYOFFDEBUG -1 //if > 0 will seed playoff and use this MTRand
#if PLAYOFFDEBUG > 0
#warning Playoff debug is ON. A global MTRand will be created.
extern MTRand playoff_rand;
#endif

// universal logging

extern void turnloggingon(std::string what);
extern void turnloggingoff();
extern std::ostream& getlog();
extern bool isloggingon();

// how to get a 64 bit integer type

#ifdef _MSC_VER
typedef __int64 int64;
typedef unsigned __int64 uint64;
#elif __GNUC__ 
#include <stdint.h>
typedef int64_t int64;
typedef uint64_t uint64;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int8_t int8;
typedef uint8_t uint8;
#endif

//functions defined in the cpp file

extern double getdoubletime(); //returns time since some (any) refence in seconds
enum report_t{ KNOWN, KILL, WARN, INFO };
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

//floating point comparison!

template <typename Num> inline Num myabs(Num x) { return (x < 0 ? -x : x); } //this fucking thing
template <typename Num> inline Num mymin(Num x, Num y) { return (x < y ? x : y); }
template <typename Num> inline Num mymax(Num x, Num y) { return (x > y ? x : y); }
extern bool fpequal(double x, double y, int maxUlps=10);
inline bool fpgreater(double x, double y) { return x > y && !fpequal(x,y); }
inline bool fpgreatereq(double x, double y) { return x > y || fpequal(x,y); }

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

// My own custom file utility...

class InFile
{
public:
	InFile() : opened(false) { }
	InFile(const std::string &filename, int64 expectedsize) : fname(filename) { Open(filename, expectedsize); }
	~InFile();
	void Open(const std::string & filename, int64 expectedsize);
	template < typename T > T Read();
	void Seek(int64 position);
	int64 Tell();
	int64 Size();
private:
	bool opened;
	std::string fname;
#ifdef _MSC_VER
	HANDLE file;
#else
	int file;
#endif
};

// My own exception class that accepts a string

class Exception : public std::exception
{
	public:
		Exception( const std::string & message ) throw( )
		{
			try
			{
#ifdef _WIN32
				strncpy_s( m_message, BUFFSIZE, message.c_str( ), BUFFSIZE );
#else
				strncpy( m_message, message.c_str( ), BUFFSIZE );
#endif
				m_message[ BUFFSIZE - 1 ] = '\0';
			}
			catch( ... )
			{
#ifdef _WIN32
				strcpy_s( m_message, BUFFSIZE, "String copy failed while making exception." );
#else
				strcpy( m_message, "String copy failed while making exception." );
#endif

			}
		}
		virtual const char * what( ) const throw( ) { return m_message; }
		virtual ~Exception( ) throw ( ) { }
	private:
		static const int BUFFSIZE = 1024;
		char m_message[ BUFFSIZE ];
};


#endif
