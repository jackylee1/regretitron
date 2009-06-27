#include "stdafx.h"
#include "../HandIndexingV1/getindex2N.h" //The feasibility of bin storage is based on this indexing.
#include "../HandIndexingV1/constants.h"
#include "../PokerLibrary/binstorage.h" //my standard routines to pack and un-pack data.
#include <string>
#include "../utility.h"
#include "../PokerLibrary/floaterfile.h"
using namespace std;

const string BINSFOLDER = ""; //current directory


//------------- R i v e r   E V -------------------


//takes a two card "mine" an a five card "board"
//computes the EV of that hand+board against all possible opponents
//in terms of wins/total
//used by the computeriverEV1 below, it is the innerloop of that function
inline floater computeriverEV2(CardMask mine, CardMask board)
{
	CardMask minefull;
	CardMask his, hisfull;
	CardMask dead;
	HandVal r1, r2;
	int wins=0, total=0; //bounded by 45 choose 2

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

	return (floater)wins/total;
}

//compute the EV of every encounter at the river
//takes a pointer to the riverEVarr and fills it. assumes the array is
//initialized to a negative number for all values. riverEVarr is
//indexed by getindex2N for N=5.
//used only once, and called only once, by the function saveriverEV below.
void saveriverEV()
{
	cout << "generating river EV data using " << FloaterFile::gettypename() << "s ..." << endl;

	FloaterFile riverEVarr(INDEX25_MAX);
	CardMask m, b;

	int64 x=0;
	
	ENUMERATE_2_CARDS(m, 
	{
		if(x++ % 17 == 0) cout << '.' << flush;

		ENUMERATE_5_CARDS_D(b, m, 
		{
			int64 i = getindex2N(m,b,5);
			if (riverEVarr[i] < 0)
				riverEVarr.store(i, computeriverEV2(m,b));
		});
	});

	riverEVarr.savefile("riverEV");

	cout << endl;
}

//---------------- T u r n   H S S --------------------

//compute the HSS of a hand at the turn
//takes a 2 card "mine", and a 4 card "flopturn", and computes the Hand Strength
//Squared value of mine+flopturn, using also the precomputed riverEV array
//that is returned by readriverEV above. A reference to this vector, already in
//in memory, is passed in as a parameter
//used by computerturnHSS1 only, it is the inner loop of that function.
inline floater computeturnHSS2(CardMask mine, CardMask flopturn, const FloaterFile &riverEV)
{
	CardMask river, dead, board;
	//initialize counters to zero
	double hss = 0;
	int boards = 0; //bounded by 46

	//deal out a river
	CardMask_OR(dead, mine, flopturn);
	ENUMERATE_1_CARDS_D(river, dead,
	{
		//calculate the index
		int64 i;
		CardMask_OR(board, flopturn, river);
		i = getindex2N(mine, board, 5);
		//use index to look up riverEV of this situation
		hss += (double)riverEV[i] * riverEV[i];
		boards++;
	});

	if(boards != 46) 
		REPORT("wrong number of cards passed to computeturnHSS2");

	return (floater) (hss / boards);
}

//this takes a reference to arr, holding turn HSS data to be filled, and
//a reference to riverEV, containing EV data already read from read riverEV above.
//it assemes arr has been initialized to be negative everywhere. for each unique
//private 2 card hand and each public 4 card board, we compute the HSS
//using the above function and riverEV, and save it to arr.
//used only once, and called only once, by the function saveturnHSS below.
void saveturnHSS()
{
	cout << "generating turn HSS data using " << FloaterFile::gettypename() << "s ..." << endl;

	FloaterFile arr(INDEX24_MAX);
	const FloaterFile riverEV("riverEV", INDEX25_MAX);

	CardMask b,m; //board, mine

	int64 x=0; 

	ENUMERATE_2_CARDS(m, 
	{
		if(x++ % 17 == 0) cout << '.' << flush;

		ENUMERATE_4_CARDS_D(b, m, 
		{
			int64 i = getindex2N(m,b,4);
			if (arr[i] < 0)
				arr.store(i, computeturnHSS2(m,b,riverEV));
		});
	});

	cout << endl;

	arr.savefile("turnHSS");
}

//-------------------- F l o p   H S S -----------------------

//compute the HSS of a hand at the flop
//takes a 2 card "mine", and a 4 card "flop", and computes the Hand Strength
//Squared value of mine+flop, using also the precomputed turnHSS array
//that is returned by readturnHSS above. A reference to this array, already in
//in memory, is passed in as a parameter.
//used by computerflopHSS1 only, it is the inner loop of that function.
inline floater computeflopHSS2(CardMask mine, CardMask flop, const FloaterFile &turnHSS)
{
	CardMask turn, dead, board;
	//initialize counters to zero
	double hss = 0;
	int boards = 0; //bounded by 47

	//deal out a turn
	CardMask_OR(dead, mine, flop);
	ENUMERATE_1_CARDS_D(turn, dead,
	{
		//calculate the index
		CardMask_OR(board, flop, turn);
		int64 i = getindex2N(mine, board, 4);
		//use index to look up turnHSS of this situation
		hss += (double)turnHSS[i];
		boards++;
	});

	if(boards != 47) REPORT("fatal error in computeflopHSS2");

	return (floater) (hss / boards);
}

//this takes a reference to arr, holding flop HSS data to be filled, and
//a reference to turnHSS, containing HSS data already read from readturnHSS above.
//it assumes arr has been initialized to be negative everywhere. for each unique
//private 2 card hand and each public 3 card board, we compute the HSS
//using the above function and turnHSS, and save it to arr.
//used only once, and called only once, by the function saveflopHSS below.
void saveflopHSS()
{
	cout << "generating flop HSS data using " << FloaterFile::gettypename() << "s ..." << endl;

	FloaterFile arr(INDEX23_MAX);
	const FloaterFile turnHSS("turnHSS", INDEX24_MAX);

	CardMask b,m; //board, mine

	int64 x=0;

	ENUMERATE_2_CARDS(m, 
	{
		if(x++ % 17 == 0) cout << '.' << flush;

		ENUMERATE_3_CARDS_D(b, m, 
		{
			int64 i = getindex2N(m,b,3);
			if (arr[i] < 0)
				arr.store(i, computeflopHSS2(m,b,turnHSS));
		});
	});

	cout << endl;

	arr.savefile("flopHSS");
}

//----------------P r e F l o p   H S S -----------------------

//compute the HSS of a hand at the preflop
//takes a 2 card "mine", and computes the Hand Strength Squared value of 
//that hand mine, using also the precomputed flopHSS array that is
//returned by readflopHSS above. A reference to this array, already loaded,
//is passed in as a parameter.
//used by computepreflopHSS1 only, it is the inner loop of that function.
inline floater computepreflopHSS2(CardMask mine, const FloaterFile &flopHSS)
{
	CardMask flop;
	//initialize counters to zero
	double hss = 0;
	int boards = 0; //bounded by 50 choose 3

	//deal out a flop
	ENUMERATE_3_CARDS_D(flop, mine,
	{
		//calculate the index
		int64 i = getindex2N(mine, flop, 3);
		//use index to look up turnHSS of this situation
		hss += (double)flopHSS[i];
		boards++;
	});

	if(boards != (50*49*48)/(3*2)) REPORT("fatal error in computepreflopHSS2");

	return (floater) (hss / boards);
}

//this takes a reference to arr, holding preflop HSS data to be filled, and
//a reference to flopHSS, containing HSS data already read from readflopHSS above.
//it assumes arr has been initialized to be negative everywhere. for each unique
//private 2 card hand, we compute the HSS using the above function and flopHSS,
//and save the result to arr.
//used only once, and called only once, by the function savepreflopHSS below.
void savepreflopHSS()
{
	cout << "generating preflop HSS data using " << FloaterFile::gettypename() << "s ..." << endl;
	FloaterFile arr(INDEX2_MAX);
	const FloaterFile flopHSS("flopHSS", INDEX23_MAX);

	CardMask m; //board, mine

	int64 x=0;

	ENUMERATE_2_CARDS(m, 
	{
		if(x++ % 17 == 0) cout << '.' << flush;

		int64 i = getindex2(m);
		if (arr[i] < 0)
			arr.store(i, computepreflopHSS2(m,flopHSS));
	});

	arr.savefile("preflopHSS");

	cout << endl;
}

//-------------- B i n s   C a l c u l a t i o n ---------------

struct handvalue
{
	handvalue() : index(-1), weight(0), bin(-1), hss(-1) {}
	handvalue(int64 myindex, floater myhss) : index(myindex), weight(1), bin(-1), hss(myhss) {}
	int64 index;
	short weight;
	short bin;
	floater hss;
};

inline bool operator < (const handvalue & a, const handvalue & b) 
{ 
	if(a.weight == 0) //this puts zero-weight items at END
		return false;
	else if (b.weight == 0)
		return true;
	else if(a.hss != b.hss)
		return a.hss < b.hss; 
	//turns out different indices can have the same hss value.
	//in that odd case, order by indices, so that compressbins works.
	//since what we really need, is all the like-indices grouped together,
	//then ordered by HSS.
	else 
		return a.index < b.index;
}


//takes a reference to vector of handvalue structs.
//it updates, in place, the array so that hands with the same index are grouped
//together with a higher weight. the input array should all have weight==1.
//we assume, that since the hands have been sorted by HSS, and since hands with
//the same index represent identical hands, that all hands with the same index
//will be adjacent to each other in the array already.
//used only once, and called only once, by calculatebins() below.
int64 compressbins(vector<handvalue> &myhands)
{
	uint64 i, j=0;
	int64 total_hands = 0;

	//prime the loop

	int currentweight=myhands[0].weight; //should be 1
	int64 currentindex=myhands[0].index;
	
	for(i=1; i<myhands.size() && myhands[i].weight; i++) //once we get to zero weights we're done
	{
		if(myhands[i].index == currentindex)
			currentweight++;
		else //we've found a new index, save it to j++, and update the current weight/index
		{
			myhands[j] = myhands[i-1]; //we are updating the array in place. at end will kill the rest
			total_hands += myhands[j++].weight = currentweight;
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
	total_hands += myhands[j++].weight = currentweight;

	//now kill the rest of them
	for(; j<myhands.size(); j++)
		myhands[j].weight = 0;

	return total_hands;
}


//takes a reference to an array of hands, which have been sorted by HSS, and grouped
//so that each element of the array represents a single unique hand, with a weight
//that gives the multiplicity of that hand in actual poker. it fills the 'bin' value
//of each element of the myhands array.
//I thought long and hard about the condition in the if-statement. this way works in
//in the limit of large and small n_bins, but if n_bins is very large, it will simply
//assign each unique hand a sigle bin number, and not use the higher bin numbers.
//if i ever use very high n_bins, i will need to space them out to use all bin numbers
//used only once, and called only once, by calculatebins() below.
void determinebins(vector<handvalue> &myhands, const int n_bins, const int64 total_hands)
{
	const bool print = false;
	int currentbin=0;
	int currentbinsize=0;
	int64 n_hands_left = total_hands;

	if(print)
		cout << " binning " << total_hands << " into " << n_bins << " bins. expect " <<
			(float)total_hands/n_bins << " hands per bin." << endl;

	for(uint64 i=0; i<myhands.size() && myhands[i].weight; i++) //done once hit zero-weight
	{
		//increment bins if needed

		if(currentbinsize + myhands[i].weight/2 > n_hands_left/(n_bins-currentbin) && currentbinsize > 0)
		{
			//done with this bin; goto next bin
			if(print)
				cout << "  bin " << currentbin << " has " << currentbinsize << " hands (" <<
					100.0*currentbinsize*n_bins/total_hands << "% of expected)" << endl;

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

	if(currentbinsize != n_hands_left) //this would just be a bug
		REPORT("Did not bin the right amount of hands!");
	if(currentbin != n_bins-1) //as mentioned in comments, this is possible
		REPORT(tostring(n_bins)+" is too many bins; we used "+tostring(currentbin+1)+". Could not find enough hands to fill them.");
}

inline void binandstore(vector<handvalue> &hand_list, PackedBinFile &binfile, int n_bins)
{
	//hand_list will be larger than it needs to be and any unused slots will have
	//weight zero. we need sort to sort these to the END.
	sort(hand_list.begin(), hand_list.end()); //large-hss now towards END OF ARRAY
	int64 total_hands = compressbins(hand_list);
	determinebins(hand_list, n_bins, total_hands);

	for(uint64 i=0; i<hand_list.size() && hand_list[i].weight; i++)
	{
		const int64 &myindex = hand_list[i].index;
		const int mult = binfile.isstored(myindex);
		if(mult == 0)
			binfile.store(myindex, hand_list[i].bin);
		else //weighted running average
			binfile.store(myindex, (int)(((float)(hand_list[i].bin + mult*binfile.retrieve(myindex)) / (mult + 1)) + 0.5));
	}
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
//old binning routine, used internally
void calculatebins(const int numboardcards, const int n_bins, const FloaterFile &hsstable, PackedBinFile &binfile)
{
	const unsigned int n_hands = (52-numboardcards)*(51-numboardcards)/2;
	CardMask b,m;

	vector<handvalue> myhands(n_hands);

	ENUMERATE_N_CARDS(b, numboardcards, //works for numboardcards = 0 !
	{
		unsigned int i=0; //bounded by 52 choose 2

		ENUMERATE_2_CARDS_D(m, b, 
		{
			int64 index = getindex2N(m,b,numboardcards); //works for numboardcards = 0 now.
			myhands[i++] = handvalue(index, hsstable[index]); //pushes it back with weight=1, bin=-1
		});

		if(i != n_hands) REPORT("failure to enumerate correctly");

		//now i have an array of hands, with hss. need to sort, compress and bin

		binandstore(myhands, binfile, n_bins);
	});
}


//---------------- B i n   S t o r i n g ------------------

//This is an end result function that could be called by main(). 
//it depends on the file bins/flopHSS, as used by readflopHSS().
void saveflopBINS(int n_bins)
{
	cout << "generating " << n_bins << " flop bins... (no status bar)" << endl;
	PackedBinFile packedbins(n_bins, INDEX23_MAX);
	const FloaterFile hss("flopHSS", INDEX23_MAX);
	calculatebins(3, n_bins, hss, packedbins);
	cout << packedbins.save(BINSFOLDER+"flop"+tostring(n_bins));
}


//This is an end result function that could be called by main(). 
//it depends on the file bins/turnHSS, as used by readturnHSS().
void saveturnBINS(int n_bins)
{
	cout << "generating " << n_bins << " turn bins... (no status bar)" << endl;
	PackedBinFile packedbins(n_bins, INDEX24_MAX);
	const FloaterFile hss("turnHSS", INDEX24_MAX);
	calculatebins(4, n_bins, hss, packedbins);
	cout << packedbins.save(BINSFOLDER+"turn"+tostring(n_bins));
}


//This is an end result function that could be called by main(). 
//it depends on the file bins/riverEV, as used by readriverEV().
void saveriverBINS(int n_bins)
{
	cout << "generating " << n_bins << " river bins... (no status bar)" << endl;
	PackedBinFile packedbins(n_bins, INDEX25_MAX);
	const FloaterFile ev("riverEV", INDEX25_MAX);
	calculatebins(5, n_bins, ev, packedbins);
	cout << packedbins.save(BINSFOLDER+"river"+tostring(n_bins));
}

//------------------ H i s t o r y   B i n s --------------------------


//This is an end result function that could be called by main(). 
//it depends on the file bins/turnHSS, as used by readturnHSS().
void dopflopbins(int n_bins)
{
	cout << "generating " << n_bins << " preflop bins... (no status bar)" << endl;
	PackedBinFile packedbins(n_bins, INDEX2_MAX);
	const FloaterFile hss("preflopHSS", INDEX2_MAX);
	calculatebins(0, n_bins, hss, packedbins);
	cout << packedbins.save(BINSFOLDER+"preflop"+tostring(n_bins));
}

void doflophistorybins(int n_pflop, int n_flop)
{
	cout << "generating " << n_pflop << " - " << n_flop << " flop history bins..." << endl;

	const PackedBinFile pflopbins(BINSFOLDER+"preflop"+tostring(n_pflop), -1, n_pflop, INDEX2_MAX, true);
	const FloaterFile flophss("flopHSS", INDEX23_MAX);
	PackedBinFile binfile(n_flop, INDEX23_MAX);

	const float expected = 25989600.0f/n_pflop; //52C2 * 50C3 = 25989600
	vector<handvalue> hand_list((int64)(1.11f*expected)); 

	//iterate through the bins and associated hands and do bins
	int64 x=0;
	for(int mypflopbin=0; mypflopbin<n_pflop; mypflopbin++)
	{
		uint64 i=0;

		CardMask cards_pflop, cards_flop;
		ENUMERATE_2_CARDS(cards_pflop,
		{
			if(x++ % (17*n_pflop) == 0) cout << '.' << flush;

			if(pflopbins.retrieve(getindex2(cards_pflop)) != mypflopbin)
				continue;

			ENUMERATE_3_CARDS_D(cards_flop, cards_pflop, 
			{
				int64 index = getindex23(cards_pflop, cards_flop);
				hand_list[i++] = handvalue(index, flophss[index]);
			});
		});

		if(i < 0.9*expected || i > 1.1*expected)
			REPORT("found "+tostring(i)+" hands ("+tostring(100.0*i/expected)
					+"% of expected) for preflop bin "+tostring(mypflopbin), WARN);
		if(i-1>=hand_list.size())
			REPORT("...and that overflowed hand_list. make fewer bins for less granularity or make hand_list bigger.");

		for(; i<hand_list.size(); i++)
			hand_list[i].weight = 0;

		binandstore(hand_list, binfile, n_flop);
	}

	cout << endl;
	cout << binfile.save(BINSFOLDER+"flop"+tostring(n_pflop)+'-'+tostring(n_flop));
}


void doturnhistorybins(int n_pflop, int n_flop, int n_turn)
{
	cout << "generating " << n_pflop << " - " << n_flop << " - " << n_turn << " turn history bins..." << endl;

	const PackedBinFile pflopbins(BINSFOLDER+"preflop"+tostring(n_pflop), -1, n_pflop, INDEX2_MAX, true);
	const PackedBinFile flopbins(BINSFOLDER+"flop"+tostring(n_pflop)+'-'+tostring(n_flop), -1, n_flop, INDEX23_MAX, true);
	const FloaterFile turnhss("turnHSS", INDEX24_MAX);
	PackedBinFile binfile(n_turn, INDEX231_MAX);

	const float expected = 1221511200.0f/n_pflop/n_flop;
	vector<handvalue> hand_list((int64)(1.11f*expected));

	//build hand lists and bin
	int64 x=0;
	for(int mypflopbin=0; mypflopbin<n_pflop; mypflopbin++)
	{
		for(int myflopbin=0; myflopbin<n_flop; myflopbin++)
		{
			uint64 i=0;

			CardMask cards_pflop, cards_flop, card_turn;
			ENUMERATE_2_CARDS(cards_pflop,
			{
				if(x++ % (17*n_pflop*n_flop) == 0) cout << '.' << flush;

				if(pflopbins.retrieve(getindex2(cards_pflop)) != mypflopbin)
					continue;

				ENUMERATE_3_CARDS_D(cards_flop, cards_pflop,
				{
					if(flopbins.retrieve(getindex23(cards_pflop, cards_flop)) != myflopbin)
						continue;

					CardMask dead;
					CardMask_OR(dead, cards_pflop, cards_flop);
					ENUMERATE_1_CARDS_D(card_turn, dead,
					{
						CardMask board;
						CardMask_OR(board, cards_flop, card_turn);
						int64 index = getindex231(cards_pflop, cards_flop, card_turn);
						/*
						if(index==46834397)
						{
							cout << endl << "saw index231 " << index << ", "
								<< "(" << tostring(cards_pflop) << " : " << tostring(cards_flop) << " : " << tostring(card_turn) << ")"
								<< "  index2: " << getindex2(cards_pflop)
								<< "  index23: " << getindex23(cards_pflop, cards_flop)
								<< "  index24: " << getindex2N(cards_pflop, board, 4)
								<< "  pflopbin: " << pflopbins.retrieve(getindex2(cards_pflop))
								<< "  flopbin: " << flopbins.retrieve(getindex23(cards_pflop, cards_flop));
						}
						*/
						hand_list[i++] = handvalue(index, turnhss[getindex2N(cards_pflop, board, 4)]);
					});
				});
			});

			if(i < 0.9*expected || i > 1.1*expected)
				REPORT("found "+tostring(i)+" hands ("+tostring(100.0*i/expected)
						+"% of expected) for preflop/flop bin "+tostring(mypflopbin)
						+'/'+tostring(myflopbin), WARN);
			if(i-1>=hand_list.size())
				REPORT("...and that overflowed hand_list. make fewer bins for less granularity or make hand_list bigger.");

			for(; i<hand_list.size(); i++)
				hand_list[i].weight = 0;

			binandstore(hand_list, binfile, n_turn);
		}
	}
	
	cout << endl;
	cout << binfile.save(BINSFOLDER+"turn"+tostring(n_pflop)+'-'+tostring(n_flop)+'-'+tostring(n_turn));
}


void doriverhistorybins(int n_pflop, int n_flop, int n_turn, int n_river)
{
	cout << "generating " << n_pflop << " - " << n_flop << " - " << n_turn << " - " << n_river << " river history bins..." << endl;

	const PackedBinFile pflopbins(BINSFOLDER+"preflop"+tostring(n_pflop), -1, n_pflop, INDEX2_MAX, true);
	const PackedBinFile flopbins(BINSFOLDER+"flop"+tostring(n_pflop)+'-'+tostring(n_flop), -1, n_flop, INDEX23_MAX, true);
	const PackedBinFile turnbins(BINSFOLDER+"turn"+tostring(n_pflop)+'-'+tostring(n_flop)+'-'+tostring(n_turn), -1, n_turn, INDEX231_MAX, true);
	const FloaterFile riverev("riverEV", INDEX25_MAX);
	PackedBinFile binfile(n_river, INDEX2311_MAX);

	const float expected = 56189515200.0f/n_pflop/n_flop/n_turn;
	vector<handvalue> hand_list((int64)(1.11f*expected));

	//build hand lists and bin
	int64 x=0;
	for(int mypflopbin=0; mypflopbin<n_pflop; mypflopbin++)
	{
		for(int myflopbin=0; myflopbin<n_flop; myflopbin++)
		{
			for(int myturnbin=0; myturnbin<n_turn; myturnbin++)
			{
				uint64 i=0;

				CardMask cards_pflop, cards_flop, card_turn, card_river;
				ENUMERATE_2_CARDS(cards_pflop,
				{
					if(x++ % (17*n_pflop*n_flop*n_turn) == 0) cout << '.' << flush;

					if(pflopbins.retrieve(getindex2(cards_pflop)) != mypflopbin)
						continue;

					ENUMERATE_3_CARDS_D(cards_flop, cards_pflop,
					{
						if(flopbins.retrieve(getindex23(cards_pflop, cards_flop)) != myflopbin)
							continue;

						CardMask dead;
						CardMask_OR(dead, cards_pflop, cards_flop);
						ENUMERATE_1_CARDS_D(card_turn, dead,
						{
							if(turnbins.retrieve(getindex231(cards_pflop, cards_flop, card_turn)) != myturnbin)
								continue;

							CardMask dead2;
							CardMask_OR(dead2, dead, card_turn);
							ENUMERATE_1_CARDS_D(card_river, dead2,
							{
								CardMask board;
								CardMask_OR(board, cards_flop, card_turn);
								CardMask_OR(board, board, card_river);
								int64 index = getindex2311(cards_pflop, cards_flop, card_turn, card_river);
								hand_list[i++] = handvalue(index, riverev[getindex2N(cards_pflop, board, 5)]);
							});
						});
					});
				});
			
				if(i < 0.9*expected || i > 1.1*expected)
					REPORT("found "+tostring(i)+" hands ("+tostring(100.0*i/expected)
							+"% of expected) for preflop/flop/turn bin "+tostring(mypflopbin)
							+'/'+tostring(myflopbin)+'/'+tostring(myturnbin), WARN);
				if(i-1>=hand_list.size())
					REPORT("...and that overflowed hand_list. make fewer bins for less granularity or make hand_list bigger.");

				for(; i<hand_list.size(); i++)
					hand_list[i].weight = 0;

				binandstore(hand_list, binfile, n_river);
			}
		}
	}

	cout << endl;
	cout << binfile.save(BINSFOLDER+"river"+tostring(n_pflop)+'-'+tostring(n_flop)+'-'+tostring(n_turn)+'-'+tostring(n_river));
}

void makehistbin(int a, int b, int c, int d)
{
	dopflopbins(a);
	doflophistorybins(a,b);
	doturnhistorybins(a,b,c);
	doriverhistorybins(a,b,c,d);
}

void makehistbin(int pf, int ftr)
{
	dopflopbins(pf);
	doflophistorybins(pf, ftr);
	doturnhistorybins(pf, ftr, ftr);
	doriverhistorybins(pf, ftr, ftr, ftr);
}



	/*
	saveriverEV(); //No dependencies.				 Generates "bins/riverEV".
	saveturnHSS(); //Depends on "bins/riverEV". Generates "bins/turnHSS".
	saveflopHSS(); //Depends on "bins/turnHSS". Generates "bins/flopHSS".
	savepreflopHSS(); //Deps on "bins/flopHSS". Generates "bins/preflopHSS".
	saveflopBINS(N_BINS); //Deps on "bins/flopHSS". Generates "bins/flopNBINS".
	saveflopBINS(N_BINS); //Deps on "bins/flopHSS". Generates "bins/flopNBINS".
	saveturnBINS(N_BINS);  //Deps on "bins/turnHSS". Generates "bins/turnNBINS".
	saveriverBINS(N_BINS); //Deps on "bins/riverEV". Generates "bins/riverNBINS".
	dopflopbins(#bins);     //Deps on "bins/preflopHSS" Gnerates "bins/preflopNHistBins"
	doflophistorybins(#bins); //Deps on flopHSS and preflopHistBins
	doturnhistorybins(#bins); //Deps on turnHSS and preflop and flop HistBins
	doriverhistorybins(#bins); //Deps on riverEV and preflop, flop, and turn HistBins
	makehistbin(pfbins, ftrbins); //calls previous 4 functions in order. only needs EV & HSS data
	makehistbin(pf, f, t, r);     //calls previous 4 functions in order. only needs EV & HSS data
	*/



//--------------------- M a i n -----------------------

#include <stdio.h> //atol
#include <stdlib.h> //atol
#include <string.h> //strcmp

//where the magic happens
int main(int argc, char *argv[])
{
	checkdataformat();

	if((argc>=2) && (strcmp("histbin", argv[1]) == 0 || strcmp("oldbins", argv[1]) == 0))
	{
		cout << "Current data type: Using " << FloaterFile::gettypename() << endl;
		cout << endl;
	}

	//these checks must be before other histbin checks
	//generate one round of histbins
	if(argc == 4 && strcmp("histbin", argv[1])==0 && strcmp("preflop", argv[2])==0)
		dopflopbins(atoi(argv[3]));
	else if(argc == 5 && strcmp("histbin", argv[1])==0 && strcmp("flop", argv[2])==0)
		doflophistorybins(atoi(argv[3]), atoi(argv[4]));
	else if(argc == 6 && strcmp("histbin", argv[1])==0 && strcmp("turn", argv[2])==0)
		doturnhistorybins(atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
	else if(argc == 7 && strcmp("histbin", argv[1])==0 && strcmp("river", argv[2])==0)
		doriverhistorybins(atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));

	//generate all rounds of histbins, different ways to specify number of bins
	else if(argc == 6 && strcmp("histbin", argv[1])==0)
		makehistbin(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
	else if(argc == 4 && strcmp("histbin", argv[1])==0)
		makehistbin(atoi(argv[2]), atoi(argv[3]));
	else if(argc == 3 && strcmp("histbin", argv[1])==0)
		makehistbin(atoi(argv[2]), atoi(argv[2]));

	//generate one round of oldbins
	else if(argc == 4 && strcmp("oldbins", argv[1])==0 && strcmp("flop", argv[2])==0)
		saveflopBINS(atoi(argv[3]));
	else if(argc == 4 && strcmp("oldbins", argv[1])==0 && strcmp("turn", argv[2])==0)
		saveturnBINS(atoi(argv[3]));
	else if(argc == 4 && strcmp("oldbins", argv[1])==0 && strcmp("river", argv[2])==0)
		saveriverBINS(atoi(argv[3]));

#ifdef COMPILE_TESTCODE
	//test
	else if(argc==2 && strcmp("test", argv[1])==0)
		testindex231();
#endif

	//diff
	else if((argc==6 || argc==7) && strcmp("diff", argv[1]) == 0)
	{
		int64 indexmax;
		switch(atoi(argv[5]))
		{
			case 23: indexmax = INDEX23_MAX; break;
			case 24: indexmax = INDEX24_MAX; break;
			case 25: indexmax = INDEX25_MAX; break;
			case 231: indexmax = INDEX231_MAX; break;
			case 2311: indexmax = INDEX2311_MAX; break;
			default: 
			   cout << "Usage: indextype must be 23, 24, 25, 231, or 2311" << endl; 
			   exit(-1);
		}

		bool preload = !(argc==7 && strcmp("-f", argv[6]) == 0);
		cout << "Preloading: " << preload << endl;
		const PackedBinFile f1(argv[2], -1, atol(argv[4]), indexmax, preload);
		const PackedBinFile f2(argv[3], -1, atol(argv[4]), indexmax, preload);
		int errors = 0;
		int err_max = (argc==7 && preload) ? atoi(argv[6]) : 10;
		for(int64 i=0; i<indexmax; ++i)
		{
			if(f1.retrieve(i) != f2.retrieve(i))
			{
				if(errors++==err_max)
				{
					cout << "printed first " << err_max << " errors, there are more, exiting." << endl;
					exit(-1);
				}
				cout << "files differ at " << i << " ... " << f1.retrieve(i) << " vs " << f2.retrieve(i) << endl; 
			}
		}

		if(errors==0)
		{
			cout << "files are the same!" << endl;
			exit(0);
		}
		else 
		{
			cout << "all errors reported." << endl;
			exit(-1);
		}
	}

	//usage
	else
	{
		cout << "Create history bins:           " << argv[0] << " histbin n_pflop n_flop n_turn n_river" << endl;
		cout << "Create history bins:           " << argv[0] << " histbin n_pflop n_flop-turn-river" << endl;
		cout << "Create history bins:           " << argv[0] << " histbin n_bins" << endl;
		cout << "Create one round of hist bins: " << argv[0] << " histbin (preflop | flop | turn | river) n_bins ... n_bins" << endl;
		cout << "Create old-style bins:         " << argv[0] << " oldbins (flop | turn | river) n_bins" << endl;
#ifdef COMPILE_TESTCODE
		cout << "Test indexing function:        " << argv[0] << " test" << endl;
#endif
		cout << "Diff two bin files:            " << argv[0] << " diff file1 file2 binmax indextype [-f | errmax]" << endl;
		exit(-1);
	}

#ifdef _WIN32
	PAUSE();
#endif
	return 0;
}

