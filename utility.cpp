#include "utility.h"
#include "stdafx.h"
#include "PokerLibrary/constants.h"
#include <sstream>
#include <iomanip>
#include <fstream>
using namespace std;

// this is useful for debugging playoff

#if PLAYOFFDEBUG > 0
MTRand playoff_rand(PLAYOFFDEBUG);
#endif

// universal logging

bool mylogindicator = false;
ostream * mylogfile;
void turnloggingon(bool turnon, string filename)
{
	if(turnon && !mylogindicator)
	{
		mylogindicator = true;
		mylogfile = new ofstream(filename.c_str());
	}
	else if(!turnon && mylogindicator)
	{
		mylogindicator = false;
		delete mylogfile;
	}
}
ostream& getlog()
{
	if(isloggingon())
		return *mylogfile;
	else
		REPORT("check for islogon() before using getlog()");
	exit(-1);
}
bool isloggingon() 
{ 
	return mylogindicator; 
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
	return mytv.tv_sec + (double)mytv.tv_usec / 1000000.0;
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
inline string getbacktracestring()
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
	switch(killswitch)
	{
	case KILL: 
		infomsg = "Error: " + infomsg + "\n";
#ifdef __GNUC__
		infomsg += getbacktracestring();
#endif
		break;
	case WARN: infomsg.insert(0, "Warning: "); break;
	case INFO: infomsg.insert(0, "It turns out: "); break;
	}

	if(isloggingon())
		getlog() << endl << "REPORTED:" << endl << infomsg << endl << endl;

    notify(infomsg);

	if(killswitch == KILL)
	{
		dobreakpoint();
		exit(-1);
	}
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
	if(!(maxUlps > 0 && maxUlps < 4 * 1024 * 1024)) REPORT("bad ulps");

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

// perform the magic numbers test, validates data integrity on multiple systems

template <typename T> //only used below
inline void testdata(ifstream &file, T correct1, T correct2)
{
	T mydata;
	file.read((char*)&mydata, sizeof(T));
	if(mydata != correct1) REPORT("Your system has failed the magic numbers test. You cannot play poker.");
	file.read((char*)&mydata, sizeof(T));
	if(mydata != correct2) REPORT("Your system has failed the magic numbers test. You cannot play poker.");
	T myarray[2];
	file.seekg(-2*(int)sizeof(T), ios_base::cur);
	file.read((char*)myarray, 2*sizeof(T));
	if(myarray[0] != correct1 || myarray[1] != correct2) 
		REPORT("Your system has failed the magic numbers test. You cannot play poker.");
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
	ifstream testfile2(".test.magic.numbers", ios::binary);
	testdata<char>(testfile2, 37, -21);
	testdata<unsigned char>(testfile2, 140, 72);
	testdata<signed char>(testfile2, -1, -119);
	testdata<int>(testfile2, -1068530229, 1204584848);
	testdata<int64>(testfile2, 7349334397080239341LL, -8170201615906909038LL);
	testdata<uint64>(testfile2, 16066576182318416946ULL, 4413254448756643424ULL);
	testdata<double>(testfile2, -1.2223157088418555e+116, 3.9958761729009844e+34);
	testdata<float>(testfile2, 4.35903890e-09, 5.06680665e-24);
	testdata<short>(testfile2, 11249, 6732);
	if((int)testfile2.tellg() != 74)
		REPORT("Your system has passed the magic numbers test, but it didn't read enough bytes. You lose.");
	testfile2.close();
	if(remove(".test.magic.numbers")!=0) REPORT("failure to delete!");
	//REPORT("You passed the magic numbers test!", INFO);
}

