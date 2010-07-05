#include "stdafx.h"
#include "memorymgr.h"
#include "../PokerLibrary/cardmachine.h" //for cardsi_max
#include "../PokerLibrary/treenolimit.h"
#include "../utility.h"
#include <iostream> //for allocate function
#include <fstream> //for save function
#include <algorithm> //for max_element
using namespace std;

// checks whether data on the river is allocated
bool MemoryManager::checkcompactdata(int actioni, int cardsi, int nacts)
{
	if(!usegrouping) REPORT("checkcompactdata only works when using grouping");
	return datarivercompact[ combine(actioni/nodespertreebyact(nacts), cardsi, cardmach.getcardsimax(3)) ].isallocated();
}

//used for outputting info in MemoryManager ctor only
int getsize(int r, int nacts)
{
	return (r==3?sizeof(FRivStore_type):sizeof(FStore_type)) * 2 * nacts;
}
	
//cout how much memory will use, then allocate it
MemoryManager::MemoryManager(const BettingTree &bettingtree, const CardMachine &cardmachine)
	: cardmach(cardmachine),
	tree(bettingtree),
	usegrouping( num_vertices(tree) == 6378 + 3 ), //in a full limit tree, we can use grouping
	groupmax( cardmach.getcardsimax(3) * get_property(tree, maxorderindex_tag())[3][2]/nodespertreebyact(2) ), // how many groups if we use goruping
	data( extents[3][multi_array_types::extent_range(2,10)] ),
	datariver( extents[multi_array_types::extent_range(2,10)] ),
	datarivercompact( usegrouping ? new CompactAllocatingContainer<FRivStore_type>[ groupmax ] : NULL )
{
	//make sure value from solveparams is set correctly
	if((usegrouping && USEGROUPING != 1) || (!usegrouping && USEGROUPING != 0))
		REPORT("invalid value for USEGROUPING in solveparams.h");
	if(usegrouping && get_property(tree, maxorderindex_tag())[3][2]/nodespertreebyact(2) != get_property(tree, maxorderindex_tag())[3][3]/nodespertreebyact(3))
		REPORT("differing number of groups when computed by 2 acts or 3 acts");
	if(usegrouping && getactmax(3,2)*nodespertreebyact(3) != nodespertreebyact(2)*getactmax(3,3)) //equiv to last check, cross multiplied
		REPORT("differing number of groups when computed by 2 acts or 3 acts - getactmax");

	//print out space requirements, then pause

	cout << "floating point type FStore_type is " << FSTORE_TYPENAME << " and uses " << sizeof(FStore_type) << " bytes." << endl;
	cout << "floating point type FRivStore_type is " << FRIVSTORE_TYPENAME << " and uses " << sizeof(FRivStore_type) << " bytes." << endl;
	cout << "floating point type FWorking_type is " << FWORKING_TYPENAME << " and uses " << sizeof(FWorking_type) << " bytes." << endl;

	int64 totalbytes = 0;

	for(int gr=0; gr<4; gr++)
	{
		cout << "round " << gr << " uses " << cardmach.getcardsimax(gr) << " card indexings..." << endl;

		for(int a=0; a<MAX_NODETYPES; a++)
		{
			if((int64)getactmax(gr,a+2)*(int64)cardmach.getcardsimax(gr) > 0x000000007fffffffLL)
				REPORT("your indices may overflow. fix that.");
			int64 mybytes = getactmax(gr,a+2)*getsize(gr,a+2)*cardmach.getcardsimax(gr);
			if(mybytes>0)
			{
				totalbytes += mybytes;
				cout << "round " << gr << " uses " << getactmax(gr,a+2) << " nodes with " << a+2 
					<< " members for a total of " << space(mybytes) << endl;
			}
		}
	}

	for(int gr=1; gr<4; gr++)
	{
		cout << cardmach.getparams().bin_max[gr] << " rnd" << gr << " bins use: " << space(cardmach.getparams().filesize[gr]) << endl;
		totalbytes += cardmach.getparams().filesize[gr];
	}

	cout << "total: " << space(totalbytes) << endl;

	PAUSE();

	cout << "allocating memory..." << endl;

	//now allocate memory

	//... for preflop, flop, turn...
	for (int r=0; r<3; r++)
		for(int n=2; n<10; n++)
			data[r][n] = new DataContainer<FStore_type>(n, getactmax(r,n)*cardmach.getcardsimax(r));

	//... for river.
	//when grouping, the pointers are allocated in the ctor cadence, the data itself is alloc on demand
	if(!usegrouping) //when not grouping, datariver[n] is done the same as what data[3][n] would be...
		for(int n=2; n<10; n++)
			datariver[n] = new DataContainer<FRivStore_type>(n, getactmax(3,n)*cardmach.getcardsimax(3));
}

MemoryManager::~MemoryManager()
{
	for (int r=0; r<3; r++)
		for(int n=2; n<10; n++)
			delete data[r][n];

	if(!usegrouping)
		for(int n=2; n<10; n++)
			delete datariver[n];
	else
		delete[] datarivercompact;
}

template <typename FStore>
void convert(int numa, FStore const * const stratn, unsigned char * buffer, unsigned char &checksum)
{
	checksum = 0;

	FStore max_value = *max_element(stratn, stratn+numa);

	if(*min_element(stratn, stratn+numa) < 0)
		REPORT("stratn has negative values...");

	if(max_value==0.0) //algorithm never reached this node.
		for(int a=0; a<numa; a++)
			checksum += buffer[a]=1; //assigns equal probability weighting to each
	else
	{
		for(int a=0; a<numa; a++)
		{
			//we have values in the range [0.0, max_value]
			//map them to [0.0, 256.0]
			//then cast to int. almost always all values will get rounded down,
			//But max_value will never round down. we change 256 to 255.

			int temp = int(256.0 * stratn[a] / max_value);
			if(temp == 256) temp = 255;
			if(temp < 0 || temp > 255)
				REPORT("failure to divide");
			checksum += buffer[a] = (unsigned char)temp;
		}
	}
}

void MemoryManager::readcounts(const string &filename)
{
	if(usegrouping)
	{
		cout << "Reading counts..." << endl;

		ofstream fall((filename + "-allcounts.txt").c_str());
		ofstream fmax((filename + "-maxcounts.txt").c_str());

		for(int i=0; i<groupmax; i++)
		{
			int maxcount = 0;
			for(int j=0; j<10; j++)
			{
				maxcount = mymax<int>(maxcount, datarivercompact[i].getcount(j));
				fall << datarivercompact[i].getcount(j) << (j<9?", ":"\n");
			}
			fmax << maxcount << '\n';
		}

		fall.close();
		fmax.close();

		cout << "Counts written to " + filename + "-xxxcounts.txt" + "." << endl;
	}
	else
		cout << "Cannot readcouts: not using grouping" << endl;
}

int64 MemoryManager::save(const string &filename)
{
	cout << "saving strategy data file..." << endl;

	ofstream f(("strategy/" + filename + ".strat").c_str(), ofstream::binary);

	//this starts at zero, the beginning of the file
	int64 tableoffset=0;

	//this starts at the size of the table: one 8-byte offset integer per data-array
	int64 dataoffset= 4 * MAX_NODETYPES * 8;

	for(int r=0; r<4; r++)
	{
		for(int n=9; n>=2; n--)
		{
			// seek to beginning and write the offset
			f.seekp(tableoffset); 
			f.write((char*)&dataoffset, 8); 
			tableoffset += 8; 

			// seek to end and write the data
			f.seekp(dataoffset); 
			for(int i=0; i<getactmax(r,n)*cardmach.getcardsimax(r); i++) 
			{ 
				unsigned char buffer[MAX_ACTIONS];
				unsigned char checksum = 0;
				if(r == 3)
				{
					FRivStore_type * stratn;
					FRivStore_type * regret; //unused
					if(usegrouping)
					{
						int actioni = i % getactmax(r,n);
						int cardsi = i / getactmax(r,n);
						if(checkcompactdata(actioni, cardsi, n)) // grouped allocated river bins
						{
							getcompactdata<FRivStore_type>(actioni, cardsi, n, stratn, regret);
							convert<FRivStore_type>(n, stratn, buffer, checksum);
						}
						else // grouped unallocated river bins
						{
							for(int i=0; i<n; i++)
								checksum += buffer[i] = 1;
						}
					}
					else // regular ungrouped river bins
					{
						datariver[n]->getdata(i, stratn, regret, n);
						convert<FRivStore_type>(n, stratn, buffer, checksum);
					}
				}
				else // regular preflop/flop/turn bins
				{
					FStore_type * stratn;
					FStore_type * regret; //unused
					data[r][n]->getdata(i, stratn, regret, n);
					convert<FStore_type>(n, stratn, buffer, checksum);
				}
				f.write((char*)buffer, n); 
				f.write((char*)&checksum, 1); 
			} 
			dataoffset = f.tellp(); 
		}
	}

	f.close();

	return dataoffset;
}

