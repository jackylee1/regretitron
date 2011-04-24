#include "utility.h"
#include "stdafx.h"
#include "PokerLibrary/constants.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#ifndef _MSC_VER
#include <fcntl.h>
#endif
using namespace std;

// this is useful for debugging playoff

#if PLAYOFFDEBUG > 0
MTRand playoff_rand(PLAYOFFDEBUG);
#endif

// universal logging

ConsoleLogger::ConsoleLogger( )
{
#ifdef _WIN32
	AllocConsole();
	console = new ofstream("CONOUT$");
#endif
}

ConsoleLogger::~ConsoleLogger( )
{
#ifdef _WIN32
	delete console;
	FreeConsole();
#endif
}

void ConsoleLogger::operator( )( const string & message )
{
#ifdef _WIN32
	*console << message << endl;
#else
	cout << message << endl;
#endif
}


FileLogger::FileLogger( const boost::filesystem::path & filename, bool append )
	: m_filepath( filename )
{
	if( append )
		m_file.open( m_filepath, ios_base::out | ios_base::app | ios_base::ate );
	else
		m_file.open( m_filepath, ios_base::out );

	if( ! m_file.good( ) )
		throw Exception( "Failed to open '" + m_filepath.string( ) + "' for logging." );

	if( append && m_file.tellp( ) > 0)
		m_file << "\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
	m_file << "----------------------------------" << endl;

	time_t rawtime;
	struct tm * timeinfo;

	time ( & rawtime );
	timeinfo = localtime ( & rawtime );

	m_file << "File opened for logging at " << asctime( timeinfo ) << endl << endl;
}

FileLogger::~FileLogger( )
{
	time_t rawtime;
	struct tm * timeinfo;

	time ( & rawtime );
	timeinfo = localtime ( & rawtime );

	m_file << "File closed for logging at " << asctime( timeinfo ) << endl;
	m_file << "----------------------------------" << endl;
}

void FileLogger::operator( )( const string & message )
{
	m_file << message << endl;
}

FileLogger::StopLog::StopLog( FileLogger & filelog )
	: m_filelog ( filelog )
{
	//close it to reopen later
	m_filelog.m_file.close( );
}

FileLogger::StopLog::~StopLog( )
{
	//reopen it where we were
	m_filelog.m_file.open( m_filelog.m_filepath, ios_base::out | ios_base::app | ios_base::ate );
}


// how to get seconds with milliseconds

#ifdef _MSC_VER
#include <windows.h> // for QueryPerformanceCounter
double getdoubletime()
{
	LARGE_INTEGER ticksPerSecond, tick;
	QueryPerformanceFrequency(&ticksPerSecond);
	QueryPerformanceCounter(&tick);
	return (double)tick.QuadPart/ticksPerSecond.QuadPart;
}
#elif __GNUC__
#include <sys/time.h> //for gettimeofday
#include <stdlib.h> //needed for NULL
double getdoubletime()
{
	timeval mytv;
	gettimeofday(&mytv, NULL);
	//subtract off the bulk of the seconds, so that we get more precision from teh double
	//we gain 30M seconds per year, so this is an improvement for the next few decades or so
	return (mytv.tv_sec-1279394891) + (double)mytv.tv_usec / 1000000.0;
}
#else
#error unsupported compiler type?
#endif

// how to show text (only used below)

#if defined _MFC_VER
inline void notify(string text) { AfxMessageBox(text.c_str()); }
#elif defined _MSC_VER
#include <windows.h>
inline void notify(string text) { MessageBox(NULL, text.c_str(), TEXT("Problem."), MB_OK); }
#else
#include <iostream>
inline void notify(string text) { cout << text << endl; }
#endif

// how to handle an error

#ifdef __GNUC__
#include <stdlib.h> //needed for exit
#include <execinfo.h> //needed for backtraces
#endif

#ifdef __GNUC__
string getbacktracestring()
{
	const int maxtraces = 200;
	void * backtraces[maxtraces];
	const size_t numtraces = backtrace(backtraces, maxtraces);
	char ** strings = backtrace_symbols(backtraces, numtraces);
	string ret = "Backtrace Follows:\n";
	for(unsigned i=0; i<numtraces; i++)
		ret += string(strings[i]) + "\n";
	return ret;
}
#endif

void REPORT(string infomsg, report_t killswitch)
{
	if( killswitch == KNOWN )
		throw Exception( infomsg );
	else
		throw Exception( infomsg + "\n\n" + getbacktracestring( ) );

//	switch(killswitch)
//	{
//	case KNOWN: 
//		infomsg = "Problem: " + infomsg + "\n";
//		break;
//
//	case KILL: 
//		infomsg = "Error: " + infomsg + "\n";
//#ifdef __GNUC__
//		infomsg += getbacktracestring();
//#endif
//		break;
//	}
//
//	//logger( "\n\nREPORTED:\n" + infomsg );
//
//    //notify(infomsg);
//
//	if(killswitch == KILL)
//	{
//		// dobreakpoint();
//		throw Exception( infomsg );
//	}
//	else if(killswitch == KNOWN) //e.g. bin file not found, cannot continue, but not a bug
//	{
//		throw Exception( infomsg );
//	}
}

// two digit money strings

string tostr2(double money)
{
	ostringstream o;
	o << fixed << setprecision( 2 ) << money;
	return o.str( );
}

// how to get a string corresponding to a CardMask

string tostring(CardMask cards)
{
	//this function returns a pointer to its own static char[]. we copy it into a string.
	return string(GenericDeck_maskString(&StdDeck, &cards));
}
string tostring(const vector<CardMask> &cards)
{
	string ret;
	if(cards.size() >= 1)
		ret = tostring(cards[0]);
	else
		ret = "[empty cardmask]";
	for(unsigned int i=1; i<cards.size(); i++)
		ret += " : " + tostring(cards[i]);
	return ret;
}

//how to get a string from a gameround number

string gameroundstring(int gr)
{
	switch(gr)
	{
	case RIVER: return "River";
	case TURN: return "Turn";
	case FLOP: return "Flop";
	case PREFLOP: return "Preflop";
	}
	return "BAD GR";
}

// how to get the path to our executable

#ifdef _WIN32
string GetOurPath( )
{
	//get the executable directory
	char pathchar[ MAX_PATH ];
	::GetModuleFileName( (HINSTANCE) & __ImageBase, pathchar, MAX_PATH );
	string pathcharstring = pathchar;
	return pathcharstring.substr( 0, pathcharstring.find_last_of( "/\\" ) + 1 );
}
#endif

// how to get and set the current working directory.

#ifndef _WIN32
#include <unistd.h> //has chdir
#endif
void setdirectory(string directory)
{
#ifdef _WIN32
	if(SetCurrentDirectory(directory.c_str()) == FALSE)
#else
	if(chdir(directory.c_str()) != 0)
#endif
		REPORT("Error changing directory");
}

string getdirectory()
{
	const int BUFSIZE = 512;
	char buf[BUFSIZE];
#ifdef _WIN32
	if(GetCurrentDirectory(BUFSIZE, buf) == 0)
#else
	if(getcwd(buf, BUFSIZE) == NULL)
#endif
		REPORT("Error getting current directory");
	return string(buf);
}

//how to print out pretty formatted bytes

string space(int64 bytes)
{
	ostringstream o;
	if(bytes < 1024)
		o << bytes << " bytes";
	else if(bytes < 1024*1024)
		o << fixed << setprecision(1) << (double)bytes / 1024 << "KB";
	else if(bytes < 1024*1024*1024)
		o << fixed << setprecision(1) << (double)bytes / (1024*1024) << "MB";
	else if(bytes < 1024LL*1024LL*1024LL*1024LL)
		o << fixed << setprecision(1) << (double)bytes / (1024*1024*1024) << "GB";
	else
		o << "DAYM that's a lotta memory!";
	return o.str();
}

//how to check if a file exists

bool file_exists(string filename)
{
	ifstream test(filename.c_str());
	return test.good() && test.is_open();
}

//comparing floating point values

inline bool issmall(double number) { return myabs(number) < 1e-9; }

bool fpequal(double x, double y, int maxUlps) //from http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
{
	if(issmall(x) && issmall(y) && x*y >= 0.0)
		return true;

	// Make sure maxUlps is non-negative and small enough that the
	// default NAN won't compare as equal to anything.
	if(!(maxUlps > 0)) REPORT("bad ulps");

	//voodoo!
	union { int64 asint; double asdouble; } xInt, yInt;
	xInt.asdouble = x;
	yInt.asdouble = y;

	// Make the ints lexicographically ordered as a twos-complement int
	if (xInt.asint < 0) xInt.asint = 0x8000000000000000 - xInt.asint;
	if (yInt.asint < 0) yInt.asint = 0x8000000000000000 - yInt.asint;

	//compare and done!
	if(myabs(xInt.asint - yInt.asint) <= maxUlps)
		return true;
	return false;
}

// Implement my magical InFile class...

void InFile::Open(const std::string &filename, int64 expectedsize)
{
	opened = true;
#ifdef _MSC_VER
	file = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if(file == INVALID_HANDLE_VALUE)
	{
		if(GetLastError() == ERROR_FILE_NOT_FOUND)
			REPORT("File "+filename+" was not found...", KNOWN);
		else
			REPORT("Problem opening "+filename);
	}
#else
	file = open(filename.c_str(), O_RDONLY);
	if(file == -1)
	{
		if(errno == ENOENT)
			REPORT("File "+filename+" was not found...", KNOWN);
		else
			REPORT("Problem opening "+filename);
	}
#endif
	if(expectedsize != Size())
		REPORT(filename+" was opened but was the wrong size (file:"+tostring(Size())+" expected:"+tostring(expectedsize)+")");
}

InFile::~InFile()
{
#ifdef _MSC_VER
	if(opened) CloseHandle(file);
#else
	if(opened) close(file);
#endif
}

template < typename T >
T InFile::Read()
{
	if(!opened) REPORT("not opened..");
	T val;
#ifdef _MSC_VER
	DWORD bytesread;
	if(ReadFile(file, (LPVOID)&val, sizeof(T), &bytesread, NULL) == 0 || bytesread != sizeof(T))
		REPORT("Problem reading file: '"+fname+"'");
#else
	if(read(file, (void*)&val, sizeof(T)) != sizeof(T))
		REPORT("Problem reading file: '"+fname+"'");
#endif
	return val;
}

void InFile::Seek(int64 position)
{
	if(!opened) REPORT("not opened..");
#ifdef _MSC_VER
	LARGE_INTEGER myposition;
	myposition.QuadPart = position;
	LARGE_INTEGER newpos;
	if(SetFilePointerEx(file, myposition, &newpos, FILE_BEGIN) == 0 || newpos.QuadPart != position)
		REPORT("Problem seeking "+fname+" to "+tostring(position));
#else
	if(lseek64(file, position, SEEK_SET) != position)
		REPORT("Problem seeking "+fname+" to "+tostring(position));
#endif
}

int64 InFile::Tell()
{
#ifdef _MSC_VER
	LARGE_INTEGER zero, pos;
	zero.QuadPart = 0;
	if(SetFilePointerEx(file, zero, &pos, FILE_CURRENT) == 0)
		REPORT("Problem getting "+fname+" to seek zero.");
	return pos.QuadPart;
#else
	off64_t position = lseek64(file, 0, SEEK_CUR);
	if(position == (off64_t)-1)
		REPORT("Problem getting "+fname+" to seek zero.");
	return position;
#endif
}

int64 InFile::Size()
{
	if(!opened) REPORT("not opened..");
#ifdef _MSC_VER
	LARGE_INTEGER size;
	if(GetFileSizeEx(file, &size) == 0)
		REPORT("Problem getting size of "+fname);
	return size.QuadPart;
#else
	struct stat statstruct;
	if(fstat(file, &statstruct) != 0)
		REPORT("Problem getting size of "+fname);
	return statstruct.st_size;
#endif
}

// perform the magic numbers test, validates data integrity on multiple systems

template <typename T> //only used below
inline void testdata(InFile &file, T correct1, T correct2)
{
	T mydata = file.Read<T>();
	if(mydata != correct1) REPORT("Your system has failed the magic numbers test. You cannot play poker.");
	mydata = file.Read<T>();
	if(mydata != correct2) REPORT("Your system has failed the magic numbers test. You cannot play poker.");
}

void checkdataformat()
{
	//test binary file output (or internal memory layout) 
	//if any of the tested data types have a different sizeof() or if they
	//are layed out in memory differently than they are now, this will detect it.
	//should ensure my binary files are safe to use. yay!

	MTRand crazydata(1); //seed with some number to ensure results are deterministic
	ofstream testfile1(".test.magic.numbers", ios::binary);
	for(int i=0; i<256; i++)
	{
		//char writing is defined by the C++ standard. this should create the same file every time no matter what
		char mychar = crazydata.randInt();
		testfile1.write(&mychar, 1);
	}
	testfile1.close();
	{
		InFile testfile2(".test.magic.numbers", 256);
		testdata<char>(testfile2, 37, -21);
		testdata<unsigned char>(testfile2, 140, 72);
		testdata<signed char>(testfile2, -1, -119);
		testdata<int>(testfile2, -1068530229, 1204584848);
		testdata<int64>(testfile2, 7349334397080239341LL, -8170201615906909038LL);
		testdata<uint64>(testfile2, 16066576182318416946ULL, 4413254448756643424ULL);
		testdata<double>(testfile2, -1.2223157088418555e+116, 3.9958761729009844e+34);
		testdata<float>(testfile2, 4.35903890e-09, 5.06680665e-24);
		testdata<short>(testfile2, 11249, 6732);
	}
	if(remove(".test.magic.numbers")!=0) REPORT("failure to delete!");
	//REPORT("You passed the magic numbers test!", INFO);
}

