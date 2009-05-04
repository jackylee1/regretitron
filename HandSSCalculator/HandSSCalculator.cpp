// HandSSCalculator.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../HandIndexingV1/constants.h"
using namespace std;

#define N_BINS 32 //must be same as below
#include "../SharedCode/binstruct32.h"


//this is absolutely rediculous
#define tostring(x) #x
#define stringify(x) tostring(x)

//------------- R i v e r   E V -------------------

//given a TWO card m, and a FIVE card b
//compute the EV of a single encounter at the river
inline float computeriverEV2(CardMask mine, CardMask board)
{
	CardMask minefull;
	CardMask his, hisfull;
	CardMask dead;
	HandVal r1, r2;
	int wins=0, total=0;

	CardMask_OR(dead, mine, board); // Set dead = mine & the board
	ENUMERATE_2_CARDS_D(his, dead, // enumerate 2 cards, call tem 'his', 'dead' can't be used
	{
		CardMask_OR(minefull, board, mine); // my full 7-card hand
		CardMask_OR(hisfull, board, his); //opponent's full 7-card hand
		r1 = Hand_EVAL_N(minefull, 7);
		r2 = Hand_EVAL_N(hisfull, 7);
		if (r1 > r2) wins +=2;			//i win
		else if (r1 == r2) wins ++;		//i tie
		total+=2; 
	});

	return (float)wins/total;
}

//compute the EV of every encounter at the river
void computeriverEV1(float * riverEVarr)
{
	CardMask b,m; //board, mine, dead, his

	ENUMERATE_2_CARDS(m, 
	{
		ENUMERATE_5_CARDS_D(b, m, 
		{
			int i = getindex2N(m,b,5);
			if (riverEVarr[i] == -1)
				riverEVarr[i] = computeriverEV2(m,b);
		});
	});
}

//save the EV of every encounter at the river to a file as floats
//indexed by my getindex25 function.
void saveriverEV()
{
	float * EVarr = new float[INDEX25_MAX];//481 mb
	ofstream evfile;

	for(int i=0; i<INDEX25_MAX; i++)
		EVarr[i] = -1;

	computeriverEV1(EVarr);

	for(int i=0; i<INDEX25_MAX; i++)
		if(EVarr[i] == -1)
			REPORT("indexing doesn't work");

	evfile.open("files/riverEV.dat",ofstream::binary);
	evfile.write((char*)EVarr, INDEX25_MAX*sizeof(float));
	evfile.close();
	delete [] EVarr;
}

//reads the file written above to memory
void readriverEV(float * arr)
{
	ifstream f("files/riverEV.dat",ifstream::binary);
	f.read((char *)arr, INDEX25_MAX*sizeof(float));
	if(f.gcount()!=INDEX25_MAX*sizeof(float) || f.eof() || EOF!=f.peek())
		REPORT("files/riverEV.dat could not be found, or is corrupted.");
	f.close();
}

//---------------- T u r n   H S S --------------------

//compute the HSS of a hand at the turn
inline float computeturnHSS2(CardMask mine, CardMask flopturn, float * riverEV)
{
	CardMask river, dead, board;
	//initialize counters to zero
	double hss = 0;
	int boards = 0;

	//deal out a river
	CardMask_OR(dead, mine, flopturn);
	ENUMERATE_1_CARDS_D(river, dead,
	{
		//calculate the index
		int i;
		CardMask_OR(board, flopturn, river);
		i = getindex2N(mine, board, 5);
		//use index to look up riverEV of this situation
		hss += (double)riverEV[i] * riverEV[i];
		boards++;
	});

	if(boards != 46) { cout << "fatal error in computeturnHSS2"; system("PAUSE"); exit(1); }

	return (float) (hss / boards);
}

//compute the EV of every encounter at the river
void computeturnHSS1(float * arr, float * riverEV)
{
	CardMask b,m; //board, mine, dead, his

	ENUMERATE_2_CARDS(m, 
	{
		ENUMERATE_4_CARDS_D(b, m, 
		{
			int i = getindex2N(m,b,4);
			if (arr[i] == -1)
				arr[i] = computeturnHSS2(m,b,riverEV);
		});
	});
}

//saves the flop HSS values to a file using the precomputed river EV
void saveturnHSS()
{
	float * arr = new float[INDEX24_MAX];//50ish mb
	float * riverEV = new float[INDEX25_MAX]; //to be read from file
	ofstream f;

	readriverEV(riverEV);

	for(int i=0; i<INDEX24_MAX; i++)
		arr[i] = -1;

	computeturnHSS1(arr, riverEV);

	for(int i=0; i<INDEX24_MAX; i++)
		if(arr[i] == -1)
			cout << "damned damned hell!" << endl;

	f.open("files/turnHSS.dat",ofstream::binary);
	f.write((char*)arr, INDEX24_MAX*sizeof(float));
	f.close();
	delete [] arr;
	delete [] riverEV;
}

//reads the file written above to memory
void readturnHSS(float * arr)
{
	ifstream f("files/turnHSS.dat",ifstream::binary);
	f.read((char *)arr, INDEX24_MAX*sizeof(float));
	if(f.gcount()!=INDEX24_MAX*sizeof(float) || f.eof() || EOF!=f.peek())
		REPORT("files/turnHSS.dat could not be found, or is corrupted.");
	f.close();
}

//-------------------- F l o p   H S S -----------------------

//compute the HSS of a hand at the flop
inline float computeflopHSS2(const CardMask mine, const CardMask flop, float const * const turnHSS)
{
	//turn is given by enumerate
	//dead is always mine and the flop
	//board is always flop and the turn
	CardMask turn, dead, board;
	//initialize counters to zero
	double hss = 0;
	int boards = 0;

	//deal out a turn
	CardMask_OR(dead, mine, flop);
	ENUMERATE_1_CARDS_D(turn, dead,
	{
		//calculate the index
		int i;
		CardMask_OR(board, flop, turn);
		i = getindex2N(mine, board, 4);
		//use index to look up turnHSS of this situation
		hss += (double)turnHSS[i];
		boards++;
	});

	if(boards != 47) REPORT("fatal error in computeflopHSS2");

	return (float) (hss / boards);
}

//compute the HSS of every flop
void computeflopHSS1(float * const arr, float const * const turnHSS)
{
	CardMask b,m; //board, mine

	ENUMERATE_2_CARDS(m, 
	{
		ENUMERATE_3_CARDS_D(b, m, 
		{
			int i = getindex2N(m,b,3);
			if (arr[i] == -1)
				arr[i] = computeflopHSS2(m,b,turnHSS);
		});
	});
}

//saves the flop HSS values to a file using the precomputed river EV
void saveflopHSS()
{
	float * arr = new float[INDEX23_MAX];//5ish mb
	float * turnHSS = new float[INDEX24_MAX]; //to be read from file
	ofstream f;

	readturnHSS(turnHSS);

	for(int i=0; i<INDEX23_MAX; i++)
		arr[i] = -1;

	computeflopHSS1(arr, turnHSS);

	for(int i=0; i<INDEX23_MAX; i++)
		if(arr[i] == -1)
			REPORT("indexing broken.");

	f.open("files/flopHSS.dat",ofstream::binary);
	f.write((char*)arr, INDEX23_MAX*sizeof(float));
	f.close();
	delete [] arr;
	delete [] turnHSS;
}

//reads the file written above to memory
void readflopHSS(float * arr)
{
	ifstream f("files/flopHSS.dat",ifstream::binary);
	f.read((char *)arr, INDEX23_MAX*sizeof(float));
	if(f.gcount()!=INDEX23_MAX*sizeof(float) || f.eof() || EOF!=f.peek())
		REPORT("files/flopHSS.dat could not be found, or is corrupted.");
	f.close();
}

//----------------P r e F l o p   H S S -----------------------

//compute the HSS of a hand at the preflop
inline float computepreflopHSS2(const CardMask mine, float const * const flopHSS)
{
	CardMask flop;
	//initialize counters to zero
	double hss = 0;
	int boards = 0;

	//deal out a flop
	ENUMERATE_3_CARDS_D(flop, mine,
	{
		//calculate the index
		int i = getindex2N(mine, flop, 3);
		//use index to look up turnHSS of this situation
		hss += (double)flopHSS[i];
		boards++;
	});

	if(boards != (50*49*48)/(3*2)) REPORT("fatal error in computepreflopHSS2");

	return (float) (hss / boards);
}

//compute the HSS of every preflop
void computepreflopHSS1(float * const arr, float const * const flopHSS)
{
	CardMask m; //board, mine

	ENUMERATE_2_CARDS(m, 
	{
		int i = getindex2(m);
		if (arr[i] == -1)
			arr[i] = computepreflopHSS2(m,flopHSS);
	});
}

//saves the preflop HSS values to a file using the precomputed flop HSS
void savepreflopHSS()
{
	float * arr = new float[INDEX2_MAX]; //to be computed
	float * flopHSS = new float[INDEX23_MAX]; //to be read from file
	ofstream f;

	readflopHSS(flopHSS);

	for(int i=0; i<INDEX2_MAX; i++)
		arr[i] = -1;

	computepreflopHSS1(arr, flopHSS);

	for(int i=0; i<INDEX2_MAX; i++)
		if(arr[i] == -1)
			REPORT("indexing broken.");

	f.open("files/preflopHSS.dat",ofstream::binary);
	f.write((char*)arr, INDEX2_MAX*sizeof(float));
	f.close();
	delete [] arr;
	delete [] flopHSS;
}

//reads the file written above to memory
void readpreflopHSS(float * arr)
{
	ifstream f("files/preflopHSS.dat",ifstream::binary);
	f.read((char *)arr, INDEX2_MAX*sizeof(float));
	if(f.gcount()!=INDEX2_MAX*sizeof(float) || f.eof() || EOF!=f.peek())
		REPORT("files/preflopHSS.dat could not be found, or is corrupted.");
	f.close();
}

//-------------- B i n s   C a l c u l a t i o n ---------------

struct handvalue
{
	int index;
	int weight;
	float hss;
	char bin;
};
inline bool operator < (const handvalue & a, const handvalue & b) { 
	if(a.weight == 0) //send the 0-weights to the end
		return false;
	else if(b.weight == 0)
		return true;
	else
		return a.hss < b.hss; 
}

#define M_MAX ((52*51)/(2*1)) //could be less tahn this

void compressbins(handvalue * hands)
{
	int currentweight=1;
	int currentindex=hands[0].index;
	int j=0,i;
	
	for(i=1; hands[i].weight && i<M_MAX; i++)
	{
		if(hands[i].index == currentindex)
			currentweight++;
		else
		{
			hands[j] = hands[i-1]; //we are updating the array in place. at end will zero the rest
			hands[j++].weight = currentweight;
			currentweight = 1;
			currentindex = hands[i].index;
		}
	}
	//we've ended the loop with:
	//  i pointing to the first invalid one
	//  i-1 pointing to the last valid one
	//  j pointing to the next open spot
	//...and we have saved all of them but the one at i-1
	//we counted the one currently at i-1 when it was at i, but we have not saved it.
	hands[j] = hands[i-1];
	hands[j++].weight = currentweight;

	for(; j<M_MAX; j++)
		hands[j].weight=0;
}


void determinebins(handvalue * hands, int totalspots)
{
	int currentbin=0;
	int currentbinsize=0;

	for(int i=0; hands[i].weight && i<M_MAX; i++)
	{
		//done wit this bin; goto next bin
		if(currentbinsize + hands[i].weight/2 > totalspots/(N_BINS-currentbin) && currentbinsize > 0)
		{
			totalspots -= currentbinsize;
			currentbin++;
			currentbinsize=0;
			//should automatically never increment currentbin past N_BINS-1
			if(currentbin==N_BINS) REPORT("determinebins has reached a fatal error");
		}
		//put in current bin; goto next hand
		hands[i].bin = currentbin;
		currentbinsize +=hands[i].weight;
	}
}

void calculatebins(int numboardcards, float * hssarr, char * binarr)
{
	handvalue hands[M_MAX];
	CardMask b,m;

	ENUMERATE_N_CARDS(b, numboardcards, //works for numboardcards = 0 !
	{

		for(int i=0; i<M_MAX; i++)
			hands[i].weight = 0;

		int i=0;

		ENUMERATE_2_CARDS_D(m, b, 
		{
			int index = getindex2N(m,b,numboardcards); //works for numboardcards = 0 now.
			hands[i].index = index;
			hands[i].weight = 1;
			hands[i].hss = hssarr[index];
			i++;
		});

		//now i have an array of hands, with hss. need to sort, compress and bin

		sort(hands, hands+M_MAX); //zero-weight, large-hss, both towards END OF ARRAY
		compressbins(hands);
		determinebins(hands, i);

		for(int i=0; hands[i].weight && i<M_MAX; i++)
			binarr[hands[i].index] = hands[i].bin;
	});
}


//---------------- B i n   S t o r i n g ------------------

/* There are no preflop bins...
void savepreflopBINS()
{
	char * bins = new char[INDEX2_MAX];
	float * hss = new float[INDEX2_MAX];
	ofstream f("files/preflop" stringify(N_BINS) "BINS.dat",ofstream::binary);

	readpreflopHSS(hss);
	calculatebins(0, hss, bins);

	f.write(bins, INDEX2_MAX);
	f.close();
	delete [] bins;
	delete [] hss;
}

void readpreflopBINS(char * arr)
{
	ifstream f("files/preflop" stringify(N_BINS) "BINS.dat",ifstream::binary);
	f.read(arr, INDEX2_MAX);
	if(f.gcount()!=INDEX2_MAX || f.eof() || EOF!=f.peek())
		REPORT("files/preflop" stringify(N_BINS) "BINS.dat could not be found, or is corrupted.");
	f.close();
}
*/

void saveflopBINS()
{
	char * bins = new char[INDEX23_MAX];
	float * hss = new float[INDEX23_MAX];
	binstruct * binstr = new binstruct[INDEX23_MAX/BINSPERSTRUCT+1];
	ofstream f("files/flop" stringify(N_BINS) "BINS.dat",ofstream::binary);

	readflopHSS(hss);
	calculatebins(3, hss, bins);

	for(int i=0; i<INDEX23_MAX; i++)
		store(binstr[i/BINSPERSTRUCT], bins[i], i%BINSPERSTRUCT);

	f.write((char*)binstr, (INDEX23_MAX/BINSPERSTRUCT+1)*sizeof(binstruct));
	f.close();
	delete [] bins;
	delete [] hss;
}

void readflopBINS(char * arr)
{
	ifstream f("files/flop" stringify(N_BINS) "BINS.dat",ifstream::binary);
	f.read(arr, INDEX23_MAX);
	if(f.gcount()!=INDEX23_MAX || f.eof() || EOF!=f.peek())
		REPORT("files/flop" stringify(N_BINS) "BINS.dat could not be found, or is corrupted.");
	f.close();
}


void saveturnBINS()
{
	char * bins = new char[INDEX24_MAX];
	float * hss = new float[INDEX24_MAX];
	binstruct * binstr = new binstruct[INDEX24_MAX/BINSPERSTRUCT+1];
	ofstream f("files/turn" stringify(N_BINS) "BINS.dat",ofstream::binary);

	readturnHSS(hss);
	calculatebins(4, hss, bins);

	for(int i=0; i<INDEX24_MAX; i++)
		store(binstr[i/BINSPERSTRUCT], bins[i], i%BINSPERSTRUCT);

	f.write((char*)binstr, (INDEX24_MAX/BINSPERSTRUCT+1)*sizeof(binstruct));
	f.close();
	delete [] bins;
	delete [] hss;
}

void readturnBINS(char * arr)
{
	ifstream f("files/turn" stringify(N_BINS) "BINS.dat",ifstream::binary);
	f.read(arr, INDEX24_MAX);
	if(f.gcount()!=INDEX24_MAX || f.eof() || EOF!=f.peek())
		REPORT("files/turn" stringify(N_BINS) "BINS.dat could not be found, or is corrupted.");
	f.close();
}


void saveriverBINS()
{
	char * bins = new char[INDEX25_MAX];
	float * ev = new float[INDEX25_MAX];
	binstruct * binstr = new binstruct[INDEX25_MAX/BINSPERSTRUCT+1];
	ofstream f("files/river" stringify(N_BINS) "BINS.dat",ofstream::binary);

	readriverEV(ev);
	calculatebins(5, ev, bins);

	for(int i=0; i<INDEX25_MAX; i++)
		store(binstr[i/BINSPERSTRUCT], bins[i], i%BINSPERSTRUCT);

	f.write((char*)binstr, (INDEX25_MAX/BINSPERSTRUCT+1)*sizeof(binstruct));
	f.close();
	delete [] bins;
	delete [] ev;
}

void readriverBINS(char * arr)
{
	ifstream f("files/river" stringify(N_BINS) "BINS.dat",ifstream::binary);
	f.read(arr, INDEX25_MAX);
	if(f.gcount()!=INDEX25_MAX || f.eof() || EOF!=f.peek())
		REPORT("files/river" stringify(N_BINS) "BINS.dat could not be found, or is corrupted.");
	f.close();
}


//--------------------- M a i n -----------------------

//where the magic happens
int _tmain(int argc, _TCHAR* argv[])
{
	cout << "flop bins..." << endl;
	saveflopBINS();
	cout << "turn bins..." << endl;
	saveturnBINS();
	cout << "river bins..." << endl;
	saveriverBINS();

	system("PAUSE");
	return 0;
}