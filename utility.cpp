#include "stdafx.h"
#include "utility.h"
using namespace std;

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
#endif
void REPORT(string infomsg, report_t killswitch)
{
	switch(killswitch)
	{
	case KILL: infomsg.insert(0, "Error: "); break;
	case WARN: infomsg.insert(0, "Warning: "); break;
	case INFO: infomsg.insert(0, "It turns out: "); break;
	}

    notify(infomsg);

	if(killswitch == KILL)
	{
		dobreakpoint();
#ifndef __GNUC__
		exit(-1);
#endif
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

// perform the magic numbers test, validates data integrity on multiple systems

#include "MersenneTwister.h"
#include <fstream>

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

