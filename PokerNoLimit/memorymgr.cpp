#include "stdafx.h"
#include "memorymgr.h"
#include "../PokerLibrary/cardmachine.h" //for cardsi_max
#include "../PokerLibrary/treenolimit.h"
#include "../utility.h"
#include <iostream> //for allocate function
#include <fstream> //for save function
#include <sstream> //for space function
#include <iomanip> //for space function
#include <numeric> //for accumulate
using namespace std;

template <int N>
dataN_t<N>::dataN_t() //initialize all to zero
{
	for(int a=0; a<N; a++)
		regret[a] = stratn[a] = 0;
#if STORE_DENOM
	stratd = 0;
#endif
}

//probability 0 is mapped to 0
//probability 1 is mapped to 256
//then is rounded down, to yeild number 0-255, with all number used equally.
template <int N>
stratN_t<N>::stratN_t(const dataN_t<N> &data)
{
	fpworking_type denominator = accumulate(data.stratn, data.stratn+N, (fpworking_type)0);

	for(int a=0; a<N-1; a++)
	{
		//if we just didn't get there, then the numerator would also be zero
		if (denominator==0 && data.stratn[a]!=0) 
			REPORT("zero denominator!");
		//could have just never gotten there, will happen for short iteration runs
		else if(denominator==0 && data.stratn[a]==0) 
			strat[a] = 0; //would evaluate to infinity otherwise
		else
		{
			int temp = int((fpworking_type)256.0 * data.stratn[a] / denominator);
			if(temp == 256) temp = 255; //will happen if ratio is exactly one
			if(temp < 0 || temp > 255)
				REPORT("failure to divide");
			strat[a] = (unsigned char)temp;
		}
	}
}

//pretty formatted bytes printing
//used by below function
string space(int64 bytes)
{
	ostringstream o;
	if(bytes < 1024)
		o << bytes << " bytes.";
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
	
//cout how much memory will use, then allocate it
MemoryManager::MemoryManager(const BettingTree &tree, const CardMachine &cardmach)
{
	GetTreeSize(tree, actionmax); //fills actionmax from the tree

	//print out space requirements, then pause

	cout << "floating point type fpstore_type uses " << sizeof(fpstore_type) << " bytes." << endl;

	const int64 size[MAX_NODETYPES] =  //for ease of coding
	{
		sizeof(dataN_t<2>),
		sizeof(dataN_t<3>),
		sizeof(dataN_t<4>),
		sizeof(dataN_t<5>),
		sizeof(dataN_t<6>),
		sizeof(dataN_t<7>),
		sizeof(dataN_t<8>),
		sizeof(dataN_t<9>)
	};

	int64 totalbytes = 0;

	for(int gr=0; gr<4; gr++)
	{
		cout << "round " << gr << " uses " << cardmach.getcardsimax(gr) << " card indexings..." << endl;

		for(int a=0; a<MAX_NODETYPES; a++)
		{
			int64 mybytes = actionmax[gr][a]*size[a]*cardmach.getcardsimax(gr);
			if(mybytes>0)
			{
				totalbytes += mybytes;
				cout << "round " << gr << " uses " << actionmax[gr][a] << " nodes with " << a+2 
					<< " members for a total of " << space(mybytes) << endl;
			}
		}
	}

	for(int gr=1; gr<4; gr++)
	{
		cout << CARDSETTINGS.bin_max[gr] << " rnd" << gr << " bins use: " << space(CARDSETTINGS.filesize[gr]) << endl;
		totalbytes += CARDSETTINGS.filesize[gr];
	}

	cout << "total: " << space(totalbytes) << endl;

	PAUSE();

	//now allocate memory

	for(int gr=PREFLOP; gr<=RIVER; gr++)
	{
#define ALLOC(i) data##i[gr] = (actionmax[gr][i-2]>0) ? \
	new dataN_t<i>[actionmax[gr][i-2]*cardmach.getcardsimax(gr)] : NULL
		ALLOC(9);
		ALLOC(8);
		ALLOC(7);
		ALLOC(6);
		ALLOC(5);
		ALLOC(4);
		ALLOC(3);
		ALLOC(2);
#undef ALLOC
	}
}

MemoryManager::~MemoryManager()
{
	for(int gr=PREFLOP; gr<=RIVER; gr++)
	{
#define DEALLOC(i) if(data##i[gr] != NULL) delete[] data##i[gr]
		DEALLOC(9);
		DEALLOC(8);
		DEALLOC(7);
		DEALLOC(6);
		DEALLOC(5);
		DEALLOC(4);
		DEALLOC(3);
		DEALLOC(2);
#undef DEALLOC
	}
}



#define WRITEDATA(numa) do{ \
	/* seek to beginning and write the offset */ \
	f.seekp(tableoffset); \
	f.write((char*)&dataoffset, 8); \
 \
	/* seek to end and write the data */ \
	f.seekp(dataoffset); \
	for(int k=0; k<actionmax[gr][numa-2]*cardmach.getcardsimax(gr); k++) \
	{ \
		stratN_t<numa> thisnode(data##numa[gr][k]); \
		f.write((char*)thisnode.strat, numa-1); \
	} \
	dataoffset = f.tellp(); \
}while(0);

int64 MemoryManager::save(const string &filename, const CardMachine &cardmach)
{
	ofstream f(("strategy/" + filename + ".strat").c_str(), ofstream::binary);

	//this starts at zero, the beginning of the file
	int64 tableoffset=0;

	//this starts at the size of the table: one 8-byte offset integer per data-array
	int64 dataoffset= 4 * MAX_NODETYPES * 8;

	for(int gr=0; gr<4; gr++)
	{
		WRITEDATA(9);
		WRITEDATA(8);
		WRITEDATA(7);
		WRITEDATA(6);
		WRITEDATA(5);
		WRITEDATA(4);
		WRITEDATA(3);
		WRITEDATA(2);
	}

	f.close();

	return dataoffset;
}

