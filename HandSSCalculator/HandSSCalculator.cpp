#include "stdafx.h"
#include "../HandIndexingV1/getindex2N.h" //The feasibility of bin storage is based on this indexing.
#include "../HandIndexingV1/constants.h"
#include "../PokerLibrary/binstorage.h" //my standard routines to pack and un-pack data.
using namespace std;


//------------- R i v e r   E V -------------------

//takes a two card "mine" an a five card "board"
//computes the EV of that hand+board against all possible opponents
//in terms of wins/total
//used by the computeriverEV1 below, it is the innerloop of that function
inline double computeriverEV2(CardMask mine, CardMask board)
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

	if(total!=2 * (45*44/2)) //45 choose 2
		REPORT("wrong number of cards passed to computeriverEV2");

	return (double)wins/total;
}

//compute the EV of every encounter at the river
//takes a pointer to the riverEVarr and fills it. assumes the array is
//initialized to a negative number for all values. riverEVarr is
//indexed by getindex2N for N=5.
//used only once, and called only once, by the function saveriverEV below.
void computeriverEV1(double * riverEVarr)
{
	CardMask b,m; //board, mine

	ENUMERATE_2_CARDS(m, 
	{
		ENUMERATE_5_CARDS_D(b, m, 
		{
			int i = getindex2N(m,b,5);
			if (riverEVarr[i] < 0)
				riverEVarr[i] = computeriverEV2(m,b);
		});
	});
}

//takes nothing, and computes the EV of every possible hand+board 
//combination at the river by using the above functions. it saves
//the resulting array, indexed by the getindex25 function, to a flat
//file packed as simple doubles, eight bytes each. The file is set to
//bins/riverEV.dat.
//This is an end result function that could be called by main(). 
//it has no dependencies.
void saveriverEV()
{
	cout << "generating river EV data..." << endl;

	double * EVarr = new double[INDEX25_MAX];//962 mb

	for(int i=0; i<INDEX25_MAX; i++)//computeriverEV1 relies on us initializing EVarr to something negative
		EVarr[i] = -1;

	computeriverEV1(EVarr); // all the work

	for(int i=0; i<INDEX25_MAX; i++)
		if(EVarr[i] < 0 || EVarr[i] > 1)
			REPORT("indexing doesn't work or something failed");

	//save file

	ofstream evfile("bins/riverEV.dat",ofstream::binary);
	if(sizeof(double)!=8) REPORT("doubles have failed");
	evfile.write((char*)EVarr, INDEX25_MAX*8);
	evfile.close();
	delete [] EVarr;
}

//This function takes a pointer to an allocated array, and reads the file
//written by saveriverEV, bins/riverEV.dat, into that array.
//this is used to compute the Turn HSS and to compute the river bins
//and is currently the only use of the bins/riverEV.dat data.
void readriverEV(double * arr)
{
	ifstream f("bins/riverEV.dat",ifstream::binary);
	f.read((char *)arr, INDEX25_MAX*sizeof(double));
	if(f.gcount()!=INDEX25_MAX*sizeof(double) || f.eof() || EOF!=f.peek())
		REPORT("bins/riverEV.dat could not be found, or is corrupted.");
	f.close();
}

//---------------- T u r n   H S S --------------------

//compute the HSS of a hand at the turn
//takes a 2 card "mine", and a 4 card "flopturn", and computes the Hand Strength
//Squared value of mine+flopturn, using also the precomputed riverEV array
//that is returned by readriverEV above. A pointer to this array, already in
//in memory, is passed in as a parameter
//used by computerturnHSS1 only, it is the inner loop of that function.
inline double computeturnHSS2(CardMask mine, CardMask flopturn, double * riverEV)
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

	if(boards != 46) 
		REPORT("wrong number of cards passed to computeturnHSS2");

	return (double) (hss / boards);
}

//this takes a pointer to arr, holding turn HSS data to be filled, and
//a pointer to riverEV, containing EV data already read from read riverEV above.
//it assemes arr has been initialized to be negative everywhere. for each unique
//private 2 card hand and each public 4 card board, we compute the HSS
//using the above function and riverEV, and save it to arr.
//used only once, and called only once, by the function saveturnHSS below.
void computeturnHSS1(double * arr, double * riverEV)
{
	CardMask b,m; //board, mine

	ENUMERATE_2_CARDS(m, 
	{
		ENUMERATE_4_CARDS_D(b, m, 
		{
			int i = getindex2N(m,b,4);
			if (arr[i] < 0)
				arr[i] = computeturnHSS2(m,b,riverEV);
		});
	});
}

//takes nothing, and computes the HSS of every possible hand+board 
//combination at the turn by using the above two functions. it saves
//the resulting array, indexed by the getindex24 function, to a flat
//file packed as simple doubles, four bytes each. The file is set to
//bins/turnHSS.dat.
//This is an end result function that could be called by main(). 
//it depends on the file bins/riverEV.dat, as used by readriverEV().
void saveturnHSS()
{
	cout << "generating turn HSS data..." << endl;

	double * arr = new double[INDEX24_MAX];//55 mb
	double * riverEV = new double[INDEX25_MAX]; //to be read from file

	readriverEV(riverEV);

	for(int i=0; i<INDEX24_MAX; i++) //computeturnHSS1 depends on this initialization.
		arr[i] = -1;

	computeturnHSS1(arr, riverEV); //all the work
	delete [] riverEV;

	for(int i=0; i<INDEX24_MAX; i++)
		if(arr[i] < 0 || arr[i] > 1)
			REPORT("indexing or something else has failed.");

	//save the file 

	ofstream f("bins/turnHSS.dat",ofstream::binary);
	if(sizeof(double)!=8) REPORT("doubles have failed");
	f.write((char*)arr, INDEX24_MAX*8);
	f.close();
	delete [] arr;
}

//This function takes a pointer to an allocated array, and reads the file
//written by saveturnHSS, bins/turnHSS.dat, into that array.
//this is used to compute the Flop HSS and also to compute the turn bins
//and is currently the only use of the bins/turnHSS.dat data.
void readturnHSS(double * arr)
{
	ifstream f("bins/turnHSS.dat",ifstream::binary);
	f.read((char *)arr, INDEX24_MAX*sizeof(double));
	if(f.gcount()!=INDEX24_MAX*sizeof(double) || f.eof() || EOF!=f.peek())
		REPORT("bins/turnHSS.dat could not be found, or is corrupted.");
	f.close();
}

//-------------------- F l o p   H S S -----------------------

//compute the HSS of a hand at the flop
//takes a 2 card "mine", and a 4 card "flop", and computes the Hand Strength
//Squared value of mine+flop, using also the precomputed turnHSS array
//that is returned by readturnHSS above. A pointer to this array, already in
//in memory, is passed in as a parameter.
//used by computerflopHSS1 only, it is the inner loop of that function.
inline double computeflopHSS2(const CardMask mine, const CardMask flop, double const * const turnHSS)
{
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

	return (double) (hss / boards);
}

//this takes a pointer to arr, holding flop HSS data to be filled, and
//a pointer to turnHSS, containing HSS data already read from readturnHSS above.
//it assumes arr has been initialized to be negative everywhere. for each unique
//private 2 card hand and each public 3 card board, we compute the HSS
//using the above function and turnHSS, and save it to arr.
//used only once, and called only once, by the function saveflopHSS below.
void computeflopHSS1(double * const arr, double const * const turnHSS)
{
	CardMask b,m; //board, mine

	ENUMERATE_2_CARDS(m, 
	{
		ENUMERATE_3_CARDS_D(b, m, 
		{
			int i = getindex2N(m,b,3);
			if (arr[i] < 0)
				arr[i] = computeflopHSS2(m,b,turnHSS);
		});
	});
}

//takes nothing, and computes the HSS of every possible hand+board 
//combination at the flop by using the above two functions. it saves
//the resulting array, indexed by the getindex23 function, to a flat
//file packed as simple doubles, four bytes each. The file is set to
//bins/flopHSS.dat.
//This is an end result function that could be called by main(). 
//it depends on the file bins/turnHSS.dat, as used by readturnHSS().
void saveflopHSS()
{
	cout << "generating flop HSS data..." << endl;

	double * arr = new double[INDEX23_MAX];//10 mb
	double * turnHSS = new double[INDEX24_MAX]; //to be read from file

	readturnHSS(turnHSS);

	for(int i=0; i<INDEX23_MAX; i++)
		arr[i] = -1; //neccesary for computeflopHSS1

	computeflopHSS1(arr, turnHSS); //all the work
	delete [] turnHSS;

	for(int i=0; i<INDEX23_MAX; i++)
		if(arr[i] < 0 || arr[i] > 1)
			REPORT("indexing broken or other error.");

	ofstream f("bins/flopHSS.dat",ofstream::binary);
	if(sizeof(double)!=8) REPORT("doubles have failed");
	f.write((char*)arr, INDEX23_MAX*8);
	f.close();
	delete [] arr;
}

//This function takes a pointer to an allocated array, and reads the file
//written by saveflopHSS, bins/flopHSS.dat, into that array.
//this would be used to compute the PreFlop HSS, if preflop HSS were needed,
//however, it is currently used to compute the flop bins, which is the only
//current use of the bins/flopHSS.dat data.
void readflopHSS(double * arr)
{
	ifstream f("bins/flopHSS.dat",ifstream::binary);
	f.read((char *)arr, INDEX23_MAX*sizeof(double));
	if(f.gcount()!=INDEX23_MAX*sizeof(double) || f.eof() || EOF!=f.peek())
		REPORT("bins/flopHSS.dat could not be found, or is corrupted.");
	f.close();
}

//----------------P r e F l o p   H S S -----------------------

//compute the HSS of a hand at the preflop
//takes a 2 card "mine", and computes the Hand Strength Squared value of 
//that hand mine+flop, using also the precomputed flopHSS array that is
//returned by readflopHSS above. A pointer to this array, already loaded,
//is passed in as a parameter.
//used by computepreflopHSS1 only, it is the inner loop of that function.
//NOTE: Preflop HSS data is currently UNUSED.
inline double computepreflopHSS2(const CardMask mine, double const * const flopHSS)
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

	return (double) (hss / boards);
}

//this takes a pointer to arr, holding preflop HSS data to be filled, and
//a pointer to flopHSS, containing HSS data already read from readflopHSS above.
//it assumes arr has been initialized to be negative everywhere. for each unique
//private 2 card hand, we compute the HSS using the above function and flopHSS,
//and save the result to arr.
//used only once, and called only once, by the function savepreflopHSS below.
//NOTE: Preflop HSS data is currently UNUSED.
void computepreflopHSS1(double * const arr, double const * const flopHSS)
{
	CardMask m; //board, mine

	ENUMERATE_2_CARDS(m, 
	{
		int i = getindex2(m);
		if (arr[i] < 0)
			arr[i] = computepreflopHSS2(m,flopHSS);
	});
}

//takes nothing, and computes the HSS of every possible private hand 
//preflop by using the above two functions. it saves the resulting
//array, indexed by the getindex2 function, to a flat file packed as
//simple doubles, four bytes each. The file is set to bins/preflopHSS.dat.
//This is an end result function that could be called by main(). 
//it depends on the file bins/flopHSS.dat, as used by readflopHSS().
//NOTE: Preflop HSS data is currently UNUSED.
void savepreflopHSS()
{
	cout << "generating preflop HSS data..." << endl;

	double * arr = new double[INDEX2_MAX]; //to be computed
	double * flopHSS = new double[INDEX23_MAX]; //to be read from file
	ofstream f;

	readflopHSS(flopHSS);

	for(int i=0; i<INDEX2_MAX; i++)
		arr[i] = -1; //neccesary for computepreflopHSS1

	computepreflopHSS1(arr, flopHSS); //all the work
	delete [] flopHSS;

	for(int i=0; i<INDEX2_MAX; i++)
		if(arr[i] < 0 || arr[i] > 1)
			REPORT("indexing broken, or other error.");

	f.open("bins/preflopHSS.dat",ofstream::binary);
	if(sizeof(double)!=8) REPORT("doubles have failed");
	f.write((char*)arr, INDEX2_MAX*8);
	f.close();
	delete [] arr;
}

//This function takes a pointer to an allocated array, and reads the file
//written by savepreflopHSS, bins/preflopHSS.dat, into that array.
//NOTE: Preflop HSS data is currently UNUSED.
void readpreflopHSS(double * arr)
{
	ifstream f("bins/preflopHSS.dat",ifstream::binary);
	f.read((char *)arr, INDEX2_MAX*sizeof(double));
	if(f.gcount()!=INDEX2_MAX*sizeof(double) || f.eof() || EOF!=f.peek())
		REPORT("bins/preflopHSS.dat could not be found, or is corrupted.");
	f.close();
}

//-------------- B i n s   C a l c u l a t i o n ---------------

struct handvalue
{
	int index;
	int weight;
	double hss;
	int bin;
};

inline bool operator < (const handvalue & a, const handvalue & b) { 
	if(a.weight == 0 || b.weight == 0) //should never sort these
		REPORT("failure to assume zero-weights have escaped sorting");
	else if(a.hss != b.hss)
		return a.hss < b.hss; 
	//turns out different indices can have the same hss value.
	//in that odd case, order by indices, so that compressbins works.
	//since what we really need, is all the like-indices grouped together,
	//then ordered by HSS.
	else 
		return a.index < b.index;
}


//takes a pointer to an array of handvalue structs, and the length of that array.
//it updates, in place, the array so that hands with the same index are grouped
//together with a higher weight. the input array should all have weight==1.
//we assume, that since the hands have been sorted by HSS, and since hands with
//the same index represent identical hands, that all hands with the same index
//will be adjacent to each other in the array already.
//used only once, and called only once, by calculatebins() below.
void compressbins(handvalue * const myhands, const int n_hands)
{
	int i, j=0;

	//prime the loop

	int currentweight=myhands[0].weight; //should be 1
	int currentindex=myhands[0].index;
	
	for(i=1; i<n_hands; i++)
	{
		if(myhands[i].index == currentindex)
			currentweight++;
		else //we've found a new index, save it to j++, and update the current weight/index
		{
			myhands[j] = myhands[i-1]; //we are updating the array in place. at end will zero the rest
			myhands[j++].weight = currentweight;
			currentweight = 1;
			currentindex = myhands[i].index;
		}
	}
	//we've ended the loop with:
	//  i pointing to the first invalid one
	//  i-1 pointing to the last valid one
	//  j pointing to the next open spot
	//...and we have saved all of them but the group (one or more hands) that ends at i-1.
	//we counted the one currently at i-1 when it was at i, but we have not saved it.
	myhands[j] = myhands[i-1];
	myhands[j].weight = currentweight;

	for(j=j+1; j<n_hands; j++)
		myhands[j].weight=0;
}


//takes a pointer to an array of hands, which have been sorted by HSS, and grouped
//so that each element of the array represents a single unique hand, with a weight
//that gives the multiplicity of that hand in actual poker. it fills the 'bin' value
//of each element of the myhands array.
//I thought long and hard about the condition in the if-statement. this way works in
//in the limit of large and small n_bins, but if n_bins is very large, it will simply
//assign each unique hand a sigle bin number, and not use the higher bin numbers.
//if i ever use very high n_bins, i will need to space them out to use all bin numbers
//used only once, and called only once, by calculatebins() below.
void determinebins(handvalue * const myhands, const int n_bins, const int n_hands)
{
	int currentbin=0;
	int currentbinsize=0;
	int n_hands_left = n_hands;

	for(int i=0; i<n_hands && myhands[i].weight; i++)
	{
		//increment bins if needed

		if(currentbinsize + myhands[i].weight/2 > n_hands_left/(n_bins-currentbin) && currentbinsize > 0)
		{
			//done with this bin; goto next bin
			n_hands_left -= currentbinsize;
			currentbin++;
			currentbinsize=0;
			//should automatically never increment currentbin past N_BINS-1
			if(currentbin==n_bins) REPORT("determinebins has reached a fatal error");
		}

		//then always save hand to current bin

		myhands[i].bin = currentbin;
		currentbinsize +=myhands[i].weight;
	}

	//as mentioned in comments, this is possible

	if(currentbin != n_bins-1)
		REPORT("too many bins, could not find enough hands to fill them.");
}

//this function exists for two reasons. one, because the compiler is fucked up
//and wouldn't let me put it where it's called in the macro below. it thought my 
//brackets weren't balanced. two, i think because my getindex2N function isn't
//perfect, and occasionally returns different indices for identical hands, I
//can't do the check that I do below to see if i get the same bin for a hand
//the last time it was seen. It also means I can't do the check where if i've
//seen one hand in that inner loop, then I've seen them all, because while that
//is true, the indexing function will not indicate it is true, as it will occur
//that one of my hands was seen before bet got indexed under a different value.
//Concerning doubles vs floats for storing the EV and HSS data, I get the exact
//same output from the "doublecompare.log" and "floatcompare.log" files for 
//calculating 12 river bins.
//Comparing actual bin results for using the float-packed data and double-packed
//data: river 12 bins is identical, but flop 256, flop 128, and turn 64 differ
//it's unclear if the differences are major, but they seem to be off-by-one a
//lot. AHH, I think the reason those are off was because I was assigning the 
//new bin overwriting the old bin where the call to wtfcompiler now is.
inline void wtfcompiler(ostream &log, int ind, int binprev, int bincurr)
{
	ostringstream o1,o2;
	o1 << ind << ":";
	o2 << binprev << ", " << bincurr;
	//log the difference
	log << left << setw(12) << o1.str();
	log << left << setw(15) << o2.str();
	log << abs(binprev - bincurr);
	log << endl;
}

//takes a pointer to an array of HSS or EV data (hsstable), the number of board cards,
//number of bins to create, and a pointer to an array of ints (bintable) to be the bin
//data. The HSS/EV data comes from readxxxHSS or readriverEV. each array is indexed 
//by the getindex2N() function and should be INDEX2N_MAX long.
//it assumes bintable is initialized to a negative number for error checking.
//
//it creates an array of "handvalue" structs, and initializes it to the 
//(52-numboardcards choose 2) possible hands. it sets the hand+board index,
//the weight to 1, and the HSS value - all values in the "handvalue" struct.
//it then sorts the array of "handvalue"'s by HSS using overloaded < operator 
//above. it then calls compressbins, which combines hands with the same index 
//(because they only differ by suit rotation - note this varies by the the number 
//of suits on a particular board), and then calls determinebins, which tries 
//to assign an equal number of non-unique hands to each possible bin. lastly, 
//it stores the "bin" result for each unique hand to the bintable array by the 
//index of each hand. then it repeats for each possible enumeration of the board cards.
//
//used by the saveXXXbins() functions below. called only once by each.
void calculatebins(const int numboardcards, const int n_bins, double const * const hsstable, int * const bintable)
{
	const int n_hands=(52-numboardcards)*(51-numboardcards)/2;
	handvalue * myhands = new handvalue[n_hands]; //~20k depending
	CardMask b,m;

	ofstream log("doublecompare.log",ios_base::app);
	log << endl << endl << "computing bins for " << numboardcards
		<< " board cards, and " << n_bins << " bins." << endl;
	log << "index: prevbin, currentbin      difference" << endl;

	ENUMERATE_N_CARDS(b, numboardcards, //works for numboardcards = 0 !
	{
		int i=0;

		ENUMERATE_2_CARDS_D(m, b, 
		{
			int index = getindex2N(m,b,numboardcards); //works for numboardcards = 0 now.
			myhands[i].index = index;
			myhands[i].weight = 1;
			myhands[i].hss = hsstable[index];
			i++;
		});

		if (i != n_hands) REPORT("failure to enumerate correctly");

		//now i have an array of hands, with hss. need to sort, compress and bin

		sort(myhands, myhands+n_hands); //large-hss now towards END OF ARRAY
		compressbins(myhands, n_hands); //now have some with zero weight
		determinebins(myhands, n_bins, n_hands);

		//copy the calculated values from handvalue array to bintable array

		for(i=0; i<n_hands && myhands[i].weight; i++)
		{
			if(bintable[myhands[i].index] < 0)
				bintable[myhands[i].index] = myhands[i].bin;
			else if(bintable[myhands[i].index] != myhands[i].bin)
				wtfcompiler(log, myhands[i].index, bintable[myhands[i].index], myhands[i].bin);
		}
	});
	log.close();
	delete[] myhands;
}


//---------------- B i n   S t o r i n g ------------------

/* There are no preflop bins...
void savepreflopBINS()
{
	char * bins = new char[INDEX2_MAX];
	double * hss = new double[INDEX2_MAX];
	ofstream f("bins/preflop" stringify(N_BINS) "BINS.dat",ofstream::binary);

	readpreflopHSS(hss);
	calculatebins(0, hss, bins);

	f.write(bins, INDEX2_MAX);
	f.close();
	delete [] bins;
	delete [] hss;
}

*/

//This is an end result function that could be called by main(). 
//it depends on the file bins/flopHSS.dat, as used by readflopHSS().
void saveflopBINS(int n_bins)
{
	cout << "generating " << n_bins << " flop bins..." << endl;
	int * bins = new int[INDEX23_MAX]; //to be computed
	double * hss = new double[INDEX23_MAX]; //to be read from file
	readflopHSS(hss);
	for(int i=0; i<INDEX23_MAX; i++) bins[i]=-1; //for error checking only
	calculatebins(3, n_bins, hss, bins);
	delete [] hss;
	for(int i=0; i<INDEX23_MAX; i++) 
		if(bins[i]<0 || bins[i]>=n_bins)
			REPORT("failure to bin correctly on the flop.");

	//this packed data will become our file

	if (8!=sizeof(unsigned long long)) REPORT("64 bit failure.");
	unsigned long long * packedbins = new unsigned long long[binfilesize(n_bins, INDEX23_MAX)/8];
	for(int i=0; i<INDEX23_MAX; i++)
		store(packedbins, i, bins[i], n_bins);
	delete [] bins;

	//now, save the file

	ostringstream filename;
	filename << "bins/flop" << n_bins << "BINS.dat";
	ofstream f(filename.str().c_str(),ofstream::binary);
	f.write((char*)packedbins, binfilesize(n_bins, INDEX23_MAX));
	f.close();
	delete [] packedbins;
}


//This is an end result function that could be called by main(). 
//it depends on the file bins/turnHSS.dat, as used by readturnHSS().
void saveturnBINS(int n_bins)
{
	cout << "generating " << n_bins << " turn bins..." << endl;
	int * bins = new int[INDEX24_MAX];
	double * hss = new double[INDEX24_MAX];
	readturnHSS(hss);
	for(int i=0; i<INDEX24_MAX; i++) bins[i]=-1; //for error checking only
	calculatebins(4, n_bins, hss, bins);
	delete [] hss;
	for(int i=0; i<INDEX24_MAX; i++) 
		if(bins[i]<0 || bins[i]>=n_bins)
			REPORT("failure to bin correctly on the turn.");

	//this packed data will become our file

	if (8!=sizeof(unsigned long long)) REPORT("64 bit failure.");
	unsigned long long * packedbins = new unsigned long long[binfilesize(n_bins, INDEX24_MAX)/8];
	for(int i=0; i<INDEX24_MAX; i++)
		store(packedbins, i, bins[i], n_bins);
	delete [] bins;

	//now, save the file

	ostringstream filename;
	filename << "bins/turn" << n_bins << "BINS.dat";
	ofstream f(filename.str().c_str(),ofstream::binary);
	f.write((char*)packedbins, binfilesize(n_bins, INDEX24_MAX));
	f.close();
	delete [] packedbins;
}


//This is an end result function that could be called by main(). 
//it depends on the file bins/riverEV.dat, as used by readriverEV().
void saveriverBINS(int n_bins)
{
	cout << "generating " << n_bins << " river bins..." << endl;
	int * bins = new int[INDEX25_MAX];
	double * ev = new double[INDEX25_MAX];
	readriverEV(ev);
	for(int i=0; i<INDEX25_MAX; i++) bins[i]=-1; //for error checking only
	calculatebins(5, n_bins, ev, bins);
	delete [] ev;
	for(int i=0; i<INDEX25_MAX; i++) 
		if(bins[i]<0 || bins[i]>=n_bins)
			REPORT("failure to bin correctly on the river.");

	//this packed data will become our file

	if (8!=sizeof(unsigned long long)) REPORT("64 bit failure.");
	unsigned long long * packedbins = new unsigned long long[binfilesize(n_bins, INDEX25_MAX)/8];
	for(int i=0; i<INDEX25_MAX; i++)
		store(packedbins, i, bins[i], n_bins);
	delete [] bins;

	//now, save the file

	ostringstream filename;
	filename << "bins/river" << n_bins << "BINS.dat";
	ofstream f(filename.str().c_str(),ofstream::binary);
	f.write((char*)packedbins, binfilesize(n_bins, INDEX25_MAX));
	f.close();
	delete [] packedbins;
}



//--------------------- M a i n -----------------------

//where the magic happens
int _tmain(int argc, _TCHAR* argv[])
{
	/*
	saveriverEV(); //No dependencies.				 Generates "bins/riverEV.dat".
	saveturnHSS(); //Depends on "bins/riverEV.dat". Generates "bins/turnHSS.dat".
	saveflopHSS(); //Depends on "bins/turnHSS.dat". Generates "bins/flopHSS.dat".
	savepreflopHSS(); //Deps on "bins/flopHSS.dat". Generates "bins/preflopHSS.dat".
	saveflopBINS(N_BINS); //Deps on "bins/flopHSS.dat". Generates "bins/flopNBINS.dat".
	saveflopBINS(N_BINS); //Deps on "bins/flopHSS.dat". Generates "bins/flopNBINS.dat".
	saveturnBINS(N_BINS);  //Deps on "bins/turnHSS.dat". Generates "bins/turnNBINS.dat".
	saveriverBINS(N_BINS); //Deps on "bins/riverEV.dat". Generates "bins/riverNBINS.dat".
	*/

	//USAGE: any of the above functions can be called from main and will work as expected,
	//given the dependencies. be sure to replace N_BINS above with the number of bins;
	//N_BINS is just a placeholder.

	system("PAUSE");
	return 0;
}
