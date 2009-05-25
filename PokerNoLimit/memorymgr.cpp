#include "stdafx.h"
#include "memorymgr.h"
#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/binstorage.h"
using namespace std;

//global data handles
pftree_t * pftreebase;
ftree_t * ftreebase;
ttree_t * ttreebase;
rtree_t * rtreebase;

//needed to be saved later for lookup table to traverse tree during real game
vector<unsigned char> pfexitnodes;
vector<twople> fexitnodes;
vector<threeple> texitnodes;

//helper recursive function for initmem
void walkercount(int gr, int pot, int beti)
{
	bool isvalid[9];
	betnode const * mynode = gettree(gr, beti);
	getvalidity(pot,mynode,isvalid);

	for(int a=0; a<mynode->numacts; a++)
	{
		if(!isvalid[a])
			continue;

		switch(mynode->result[a])
		{
		case NA:
			REPORT("invalid tree");
		case FD:
		case AI:
			continue;
		case GO:
			if(gr==PREFLOP)
			{
				pfexitnodes.push_back(beti);
				walkercount(gr+1, pot+mynode->potcontrib[a], 0);
			}
			else if(gr==FLOP)
			{
				fexitnodes.push_back(twople(pfexitnodes.back(), beti));
				walkercount(gr+1, pot+mynode->potcontrib[a], 0);
			}
			else if(gr==TURN)
				texitnodes.push_back(threeple(fexitnodes.back(), beti));
			else
				REPORT("error in walkercount");

			continue;

		default://child node
			walkercount(gr, pot, mynode->result[a]);
		}
	}
}

string space(long long bytes)
{
	ostringstream o;
	if(bytes < 1024)
		o << bytes << " bytes.";
	else if(bytes < 1024*1024i64)
		o << fixed << setprecision(1) << (double)bytes / 1024 << "KB";
	else if(bytes < 1024*1024*1024i64)
		o << fixed << setprecision(1) << (double)bytes / (1024*1024) << "MB";
	else if(bytes < 1024*1024*1024*1024i64)
		o << fixed << setprecision(1) << (double)bytes / (1024*1024*1024) << "GB";
	else
		o << "DAYM that's a lotta memory!";
	return o.str();
}
	

void initmem()
{
	walkercount(0,0,0);
	int preflopwalkercount = pfexitnodes.size();
	int flopwalkercount = fexitnodes.size();
	int turnwalkercount = texitnodes.size();
	long long fbinbytes = binfilesize(BIN_FLOP_MAX, INDEX23_MAX);
	long long tbinbytes = binfilesize(BIN_TURN_MAX, INDEX24_MAX);
	long long rbinbytes = binfilesize(BIN_RIVER_MAX, INDEX25_MAX);
	cout << BIN_FLOP_MAX << " flop bins use: " << space(fbinbytes) << endl;
	cout << BIN_TURN_MAX << " turn bins use: " << space(tbinbytes) << endl;
	cout << BIN_RIVER_MAX << " river bins use: " << space(rbinbytes) << endl;
	long long pftreebytes = sizeof(pftree_t);
	long long ftreebytes = (long long)preflopwalkercount*sizeof(ftree_t);
	long long ttreebytes = (long long)flopwalkercount*sizeof(ttree_t);
	long long rtreebytes = (long long)turnwalkercount*sizeof(rtree_t);
	cout << "1 preflop tree with " << CARDSI_PFLOP_MAX << " cardsi uses: " << space(pftreebytes) << endl;
	cout << preflopwalkercount << " flop trees with " << CARDSI_FLOP_MAX << " cardsi (" 
		<< space(sizeof(ftree_t)) << ") for " 
		<< space(ftreebytes) << endl;
	cout << flopwalkercount << " turn trees with " << CARDSI_TURN_MAX << " cardsi ("
		<< space(sizeof(ttree_t)) << ") for " 
		<< space(ttreebytes) << endl;
	cout << turnwalkercount << " river trees with " << CARDSI_RIVER_MAX << " cardsi ("
		<< space(sizeof(rtree_t)) << ") for " 
		<< space(rtreebytes) << endl;
	cout << "total: " << space(fbinbytes+tbinbytes+rbinbytes+pftreebytes+ftreebytes+ttreebytes+rtreebytes) << endl;
	system("pause");
	rtreebase = new rtree_t [turnwalkercount];
	ttreebase = new ttree_t [flopwalkercount];
	ftreebase = new ftree_t [preflopwalkercount];
	pftreebase = new pftree_t;
}

void closemem()
{
	delete pftreebase;
	delete[] ftreebase;
	delete[] ttreebase;
	delete[] rtreebase;
}



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


/*
void dumpstratresult(const char * const filename)
{
	ofstream f(filename, ofstream::binary);
	float *stratt, *stratn, *stratd, *regret;
	float result[8];

	//dumps out the strategy result in it's own format.
	for(int s=0; s<SCENI_MAX; s++)
	{
		int b=0;

		for(; b<BETI9_CUTOFF; b++)
		{
			int maxa = (s<SCENI_PREFLOP_MAX) ? pfloptree[b].numacts : n[b].numacts;
			getpointers(s,b,maxa,0,stratt,stratn,stratd,regret);

			//we print all values, valid or not. 
			memset(result, 0, 8*sizeof(float));
			for(int a=0; a<maxa-1; a++) result[a] = safedivide(stratn[a],stratd[a]);

			f.write((char*)result, 8*sizeof(float));
		}
		
		for(; b<BETI3_CUTOFF; b++)
		{
			int maxa = (s<SCENI_PREFLOP_MAX) ? pfloptree[b].numacts : n[b].numacts;
			getpointers(s,b,maxa,0,stratt,stratn,stratd,regret);

			//we print all values, valid or not. 
			memset(result, 0, 8*sizeof(float));
			for(int a=0; a<maxa-1; a++) result[a] = safedivide(stratn[a],stratd[a]);

			f.write((char*)result, 2*sizeof(float));
		}
		
		for(; b<BETI2_CUTOFF; b++)
		{
			int maxa = (s<SCENI_PREFLOP_MAX) ? pfloptree[b].numacts : n[b].numacts;
			getpointers(s,b,maxa,0,stratt,stratn,stratd,regret);

			//we print all values, valid or not. 
			memset(result, 0, 8*sizeof(float));
			for(int a=0; a<maxa-1; a++) result[a] = safedivide(stratn[a],stratd[a]);

			f.write((char*)result, 1*sizeof(float));
		}
	}
	f.close();
}
*/

