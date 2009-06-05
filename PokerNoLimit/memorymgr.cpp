#include "stdafx.h"
#include "memorymgr.h"
#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/binstorage.h"
#include "../portability.h"
using namespace std;

//global data handles
dataN_t<9> * data9[4];
dataN_t<8> * data8[4];
dataN_t<7> * data7[4];
dataN_t<6> * data6[4];
dataN_t<5> * data5[4];
dataN_t<4> * data4[4];
dataN_t<3> * data3[4];
dataN_t<2> * data2[4];

//needed to be saved later for lookup table to traverse tree during real game
int actionmax[4][MAX_ACTIONS-1]; //minus one because single-action nodes don't exist

//used to obtain the correct pointers to the data from the above arrays, 
//so that other code doesn't need to worry about it.
void dataindexing(int gr, int nacts, int actioni, int cardsi, fpstore_type* &stratn, fpstore_type* &stratd, fpstore_type* &regret)
{
	switch(nacts)
	{
	case 9:
		stratn = data9[gr][cardsi*actionmax[gr][7] + actioni].stratn;
		stratd = &(data9[gr][cardsi*actionmax[gr][7] + actioni].stratd);
		regret = data9[gr][cardsi*actionmax[gr][7] + actioni].regret;
		return;
	case 8:
		stratn = data8[gr][cardsi*actionmax[gr][6] + actioni].stratn;
		stratd = &(data8[gr][cardsi*actionmax[gr][6] + actioni].stratd);
		regret = data8[gr][cardsi*actionmax[gr][6] + actioni].regret;
		return;
	case 7:
		stratn = data7[gr][cardsi*actionmax[gr][5] + actioni].stratn;
		stratd = &(data7[gr][cardsi*actionmax[gr][5] + actioni].stratd);
		regret = data7[gr][cardsi*actionmax[gr][5] + actioni].regret;
		return;
	case 6:
		stratn = data6[gr][cardsi*actionmax[gr][4] + actioni].stratn;
		stratd = &(data6[gr][cardsi*actionmax[gr][4] + actioni].stratd);
		regret = data6[gr][cardsi*actionmax[gr][4] + actioni].regret;
		return;
	case 5:
		stratn = data5[gr][cardsi*actionmax[gr][3] + actioni].stratn;
		stratd = &(data5[gr][cardsi*actionmax[gr][3] + actioni].stratd);
		regret = data5[gr][cardsi*actionmax[gr][3] + actioni].regret;
		return;
	case 4:
		stratn = data4[gr][cardsi*actionmax[gr][2] + actioni].stratn;
		stratd = &(data4[gr][cardsi*actionmax[gr][2] + actioni].stratd);
		regret = data4[gr][cardsi*actionmax[gr][2] + actioni].regret;
		return;
	case 3:
		stratn = data3[gr][cardsi*actionmax[gr][1] + actioni].stratn;
		stratd = &(data3[gr][cardsi*actionmax[gr][1] + actioni].stratd);
		regret = data3[gr][cardsi*actionmax[gr][1] + actioni].regret;
		return;
	case 2:
		stratn = data2[gr][cardsi*actionmax[gr][0] + actioni].stratn;
		stratd = &(data2[gr][cardsi*actionmax[gr][0] + actioni].stratd);
		regret = data2[gr][cardsi*actionmax[gr][0] + actioni].regret;
		return;
	default:
		REPORT("invalid number of actions");
	}
}

//helper recursive function for initmem
void walkercount(int gr, int pot, int beti)
{
	betnode mynode;
	getnode(gr, pot, beti, mynode);
	int numa = mynode.numacts; //for ease of typing

	actionmax[gr][numa-2]++;

	for(int a=0; a<numa; a++)
	{
		switch(mynode.result[a])
		{
		case NA:
			REPORT("invalid tree");
		case FD:
		case AI:
			continue;
		case GO:
			if(gr!=RIVER)
				walkercount(gr+1, pot+mynode.potcontrib[a], 0);
			continue;

		default://child node
			walkercount(gr, pot, mynode.result[a]);
		}
	}
}

//pretty formatted bytes printing
string space(long long bytes)
{
	ostringstream o;
	if(bytes < 1024)
		o << bytes << " bytes.";
	else if(bytes < 1024LL*1024)
		o << fixed << setprecision(1) << (double)bytes / 1024 << "KB";
	else if(bytes < 1024LL*1024*1024)
		o << fixed << setprecision(1) << (double)bytes / (1024LL*1024) << "MB";
	else if(bytes < 1024LL*1024*1024*1024)
		o << fixed << setprecision(1) << (double)bytes / (1024LL*1024*1024) << "GB";
	else
		o << "DAYM that's a lotta memory!";
	return o.str();
}
	
//cout how much memory will use, then allocate it
void initmem()
{
	clock_t c = clock();
	walkercount(0,0,0);
	clock_t diff = clock()-c;
	cout << "...took " << (float)diff/CLOCKS_PER_SEC << " seconds." << endl;

	cout << "floating point type fpstore_type uses " << sizeof(fpstore_type) << " bytes." << endl;

	long long actionbytes = 0;
	long long size[MAX_ACTIONS-1];
	size[7] = sizeof(dataN_t<9>);
	size[6] = sizeof(dataN_t<8>);
	size[5] = sizeof(dataN_t<7>);
	size[4] = sizeof(dataN_t<6>);
	size[3] = sizeof(dataN_t<5>);
	size[2] = sizeof(dataN_t<4>);
	size[1] = sizeof(dataN_t<3>);
	size[0] = sizeof(dataN_t<2>);
	long long cards[4];
	cards[0] = CARDSI_PFLOP_MAX;
	cards[1] = CARDSI_FLOP_MAX;
	cards[2] = CARDSI_TURN_MAX;
	cards[3] = CARDSI_RIVER_MAX;

	for(int gr=PREFLOP; gr<=RIVER; gr++)
	{
		cout << "round " << gr << " uses " << cards[gr] << " card indexings..." << endl;

		for(int a=0; a<MAX_ACTIONS-1; a++)
		{
			long long mybytes = actionmax[gr][a]*size[a]*cards[gr];
			actionbytes += mybytes;
			cout << "round " << gr << " uses " << actionmax[gr][a] << " nodes with " << a+2 
				<< " members for a total of " << space(mybytes) << endl;
		}
	}

	long long fbinbytes = binfilesize(BIN_FLOP_MAX, INDEX23_MAX);
	long long tbinbytes = binfilesize(BIN_TURN_MAX, INDEX24_MAX);
	long long rbinbytes = binfilesize(BIN_RIVER_MAX, INDEX25_MAX);
	cout << BIN_FLOP_MAX << " flop bins use: " << space(fbinbytes) << endl;
	cout << BIN_TURN_MAX << " turn bins use: " << space(tbinbytes) << endl;
	cout << BIN_RIVER_MAX << " river bins use: " << space(rbinbytes) << endl;

	cout << "total: " << space(fbinbytes+tbinbytes+rbinbytes+actionbytes) << endl;

	PAUSE();

	for(int gr=PREFLOP; gr<=RIVER; gr++)
	{
#define ALLOC(i) data##i[gr] = (actionmax[gr][i-2]>0) ? \
	new dataN_t<i>[actionmax[gr][i-2]*cards[gr]] : NULL
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

void closemem()
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



/*
void savestratresult(const char * const filename)
{
	ofstream f(filename, ofstream::binary);
	int a;
	
	//length of preflop exit node table
	a = pfexitnodes.size();
	f.write((char*)&a, sizeof(int));
	//the preflop exit node table
	for(unsigned int i=0; i<pfexitnodes.size(); i++)
		f.put(pfexitnodes[i]);

	//length of flop exit node table
	a = fexitnodes.size();
	f.write((char*)&a, sizeof(int));
	//the flop exit node table
	for(unsigned int i=0; i<fexitnodes.size(); i++)
	{
		f.put(fexitnodes[i].x);
		f.put(fexitnodes[i].y);
	}

	//length of turn exit node table
	a = texitnodes.size();
	f.write((char*)&a, sizeof(int));
	//the flop exit node table
	for(unsigned int i=0; i<texitnodes.size(); i++)
	{
		f.put(texitnodes[i].x);
		f.put(texitnodes[i].y);
		f.put(texitnodes[i].z);
	}

	//the preflop betting tree
	for(int c=0; c<CARDSI_PFLOP_MAX; c++)
	{
		treestrat_t mystrat(pftreebase->treedata[c]);
		f.write((char*)&mystrat, sizeof(treestrat_t));
	}

	//the flop betting trees
	for(unsigned int b=0; b<pfexitnodes.size(); b++)
	{
		for(int c=0; c<CARDSI_FLOP_MAX; c++)
		{
			treestrat_t mystrat(ftreebase[b].treedata[c]);
			f.write((char*)&mystrat, sizeof(treestrat_t));
		}
	}

	//the turn betting trees
	for(unsigned int b=0; b<fexitnodes.size(); b++)
	{
		for(int c=0; c<CARDSI_TURN_MAX; c++)
		{
			treestrat_t mystrat(ttreebase[b].treedata[c]);
			f.write((char*)&mystrat, sizeof(treestrat_t));
		}
	}

	//the river betting trees
	for(unsigned int b=0; b<texitnodes.size(); b++)
	{
		for(int c=0; c<CARDSI_RIVER_MAX; c++)
		{
			treestrat_t mystrat(rtreebase[b].treedata[c]);
			f.write((char*)&mystrat, sizeof(treestrat_t));
		}
	}
}
*/
