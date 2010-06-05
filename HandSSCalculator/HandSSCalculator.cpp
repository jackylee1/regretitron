#include "stdafx.h"
#include "../HandIndexingV1/getindex2N.h" //The feasibility of bin storage is based on this indexing.
#include "../HandIndexingV1/constants.h"
#include "../PokerLibrary/binstorage.h" //my standard routines to pack and un-pack data.
#include "../MersenneTwister.h"
#include <string>
#include <iomanip>
#include "../utility.h"
#include "../PokerLibrary/floaterfile.h"
using namespace std;

const string BINSFOLDER = ""; //current directory
const floater EPSILON = 1e-12;
const bool RIVERBUNCHHISTBINS = false;


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

inline bool fpcompare(floater x, floater y)
{
	//must only distinguish rounding error
	//things separate by only this amount will be guaranteed to bin together. 
	//this is important when a single hand has two indices and then two hss 
	//values due to rounding error.
	// bug = one hand, gets two indices (imperfect index function), those
	// two indices get separate HSS values (fp rounding error), allowing 
	// those two indices to not be recognized as teh same HSS, and then
	// they HAPPEN to fall on the edge of a bin, and go into different bins.
	//that is the bug
	return myabs(x - y) <= EPSILON * myabs(x);
}

struct RawHand
{
	RawHand() : index(-1), hss(-1) { }
	RawHand(int64 myindex, floater myhss) : index(myindex), hss(myhss) { }
	int64 index;
	floater hss;
};

inline bool operator < (const RawHand & a, const RawHand & b) 
{ 
	if(a.index < 0 || b.index < 0)
	{
		REPORT("invalid index found in sort!");
		exit(-1);
	}
	else if(a.hss != b.hss)
		//can't use fpcompare, because then get weird issue with a == b and b == c
		// but a < c, and it messes up sort sometimes. exact compare is fine, as
		// long as the HSS's that *should* be the same are very close, and they 
		// definitely will be.
		return a.hss < b.hss; 
	//turns out different indices can have the same hss value.
	//in that odd case, order by indices, so that compressbins works.
	//since what we really need, is all the like-indices grouped together,
	//then ordered by HSS.
	else 
		return a.index < b.index;
}

struct IndexNode
{
	IndexNode(int64 myindex) : index(myindex), next(NULL) { }
	int64 index;
	IndexNode* next;
};

struct CompressedHand
{
	CompressedHand() : index(-1), nextindex(NULL), weight(0), bin(-1) { }
	int64 index;
	IndexNode * nextindex;
	int weight;
	int bin;
};

//totalhands is the length of the hand_list array
int64 compressbins(const vector<RawHand> &hand_list, vector<CompressedHand> &hand_list_compressed, int64 totalhands, bool bunchsimilarhands)
{
	//prime the loop

	int64 currentindex = hand_list_compressed[0].index = hand_list[0].index;
	hand_list_compressed[0].weight = 1;
	IndexNode ** p_current_next_pointer = &(hand_list_compressed[0].nextindex); //whoa

	int64 i=1, j=0;
	for(; i<totalhands; i++)
	{
		if(hand_list[i].index == currentindex) //hss equal, same index
		{
			if(hand_list[i].hss != hand_list[i-1].hss) //hss differ, same index
				REPORT("same index => different hss!");
			hand_list_compressed[j].weight++;
		}
		else if(bunchsimilarhands && fpcompare(hand_list[i].hss, hand_list[i-1].hss)) //hss equal, and indices differ
		{
			hand_list_compressed[j].weight++;
			(*p_current_next_pointer) = new IndexNode(hand_list[i].index);
			p_current_next_pointer = &((*p_current_next_pointer)->next);
			currentindex = hand_list[i].index;
		}
		else //index differs, and either we're not bunching similar hands, or the hss also differs
		{
			j++;
			hand_list_compressed[j].index = currentindex = hand_list[i].index;
			hand_list_compressed[j].weight = 1;
			p_current_next_pointer = &(hand_list_compressed[j].nextindex);
		}
	}

	//we've ended the loop with:
	//  i pointing to the first invalid one
	//  j pointing to the last open spot
	return j+1; //return length of hand_list_compressed array
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
void determinebins(vector<CompressedHand> &myhands, const int n_bins, const int64 total_hands, const int64 compressed_size)
{
	const bool print = false;
	bool chunky = false;
	float expected = (float)total_hands/n_bins; //expected bins per hand
	float tolerance = 1.05f;
	ostringstream status;
	int currentbin=0;
	int currentbinsize=0;
	int64 n_hands_left = total_hands;

	status << " binning " << total_hands << " hands (" << compressed_size
	   << " compressed) into " << n_bins << " bins. expect " << expected << " hands per bin "
	   << "(tolerance = " << tolerance << ")..." << endl;

	for(int64 i=0; i<compressed_size; i++)
	{
		//increment bins if needed

		if(currentbinsize + myhands[i].weight/2 > n_hands_left/(n_bins-currentbin) && currentbinsize > 0)
		{
			//done with this bin; go to next bin
			status << "  bin " << currentbin << " has " << currentbinsize << " hands (" <<
				100.0*currentbinsize/expected << "% of expected)" << endl;

			if(currentbinsize > expected * tolerance || currentbinsize < expected / tolerance)
				chunky = true;

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

	status << "  bin " << currentbin << " has " << currentbinsize << " hands (" <<
		100.0*currentbinsize/expected << "% of expected)";

	if(currentbinsize != n_hands_left) //this would just be a bug
		REPORT("Did not bin the right amount of hands!");
	if(currentbin != n_bins-1) //as mentioned in comments, this is possible
		REPORT("\n"+tostring(n_bins)+" is too many bins; we used "+tostring(currentbin+1)+". Could not find enough hands to fill them. "
				+"We were binning "+tostring(total_hands)+" hands. ("+tostring(compressed_size)+" compressed).",WARN);
	if(chunky)
		REPORT("\nThis binning process has produced chunky bins, report follows:\n"+status.str(),WARN);
	if(print)
		cout << endl << status.str() << flush;
}

inline void store(PackedBinFile &binfile, int64 myindex, int newbinvalue)
{
	const int mult = binfile.isstored(myindex);
	if(mult == 0)
		binfile.store(myindex, newbinvalue);
	else //weighted running average
		binfile.store(myindex, (int)(((float)(newbinvalue + mult*binfile.retrieve(myindex)) / (mult + 1)) + 0.5));
}

inline void binandstore(vector<RawHand> &hand_list, PackedBinFile &binfile, int n_bins, int64 total_hands, bool bunchsimilarhands = true)
{
	//hand_list will be larger than it needs to be and any unused slots will have
	//weight zero. we need sort to sort these to the END.

	sort(hand_list.begin(), hand_list.begin()+total_hands); //large-hss now towards END OF ARRAY

	//compress the hands, getting the new size

	static vector<CompressedHand> hand_list_compressed(0); //massive hack so we don't reallocate all the time
	if((int64)hand_list_compressed.size() < total_hands/8) hand_list_compressed.resize(total_hands/7);
	int64 compressed_size = compressbins(hand_list, hand_list_compressed, total_hands, bunchsimilarhands);
	if(compressed_size > (int64)hand_list_compressed.size())
		REPORT("overflowed hand_list_compressed - "+tostring(total_hands)+" "+tostring(compressed_size));

	//do the binning

	determinebins(hand_list_compressed, n_bins, total_hands, compressed_size);

	//logging

	if(false) //logging
	{
		ofstream log("binlog.txt");
		log << "Raw Hand List:" << endl;
		log << setw(10) << right << "i" << ":  " << setw(13) << right << "index2xx" << "   " << "hss" << endl;
		log << setw(10) << right << 0 << ":  " << setw(13) << right << hand_list[0].index 
			<< "   " << setprecision(20) << hand_list[0].hss << endl;
		for(int64 i=1; i<total_hands; i++)
		{
			if(!fpcompare(hand_list[i].hss, hand_list[i-1].hss))
				log << "\nFPCOMPARE BREAK\n\n";
			else if(hand_list[i].hss != hand_list[i-1].hss)
				log << "\nSTRICT COMPARE BREAK\n\n";

			log << setw(10) << right << i << ":  " << setw(13) << right << hand_list[i].index 
				<< "   " << setprecision(20) << hand_list[i].hss << '\n';
		}

		log << endl << endl << "Compressed Hand List:" << endl;
		log << setw(5) << right << "i" << ":  " << setw(13) << right << "1st-index" 
			<< "   " << setw(13) << right << "weight"
		 	<< "   " << "bin#\n";
		for(int64 i=0; i<compressed_size; i++)
		{
			log << setw(5) << right << i << ":  " << setw(13) << right << hand_list_compressed[i].index
				<< "   " << setw(13) << right << hand_list_compressed[i].weight
				<< "   " << hand_list_compressed[i].bin << '\n';
		}
		log.close();
	}

	//store the bins to the packed bin file, and delete the IndexNode's
	
	for(int64 i=0; i<compressed_size; i++)
	{
		store(binfile, hand_list_compressed[i].index, hand_list_compressed[i].bin);
		IndexNode * nextindex = hand_list_compressed[i].nextindex;
		hand_list_compressed[i].nextindex = NULL;
		while(nextindex != NULL)
		{
			store(binfile, nextindex->index, hand_list_compressed[i].bin);
			IndexNode * temp = nextindex->next;
			delete nextindex;
			nextindex = temp;
		}
	}
}


//takes a pointer to an array of HSS or EV data (hsstable), the number of board cards,
//number of bins to create, and a pointer to an array of ints (bintable) to be the bin
//data. The HSS/EV data comes from readxxxHSS or readriverEV. each array is indexed 
//by the getindex2N() function and should be INDEX2N_MAX long.
//it assumes bintable is initialized to a negative number for error checking.
//
//it creates an array of "RawHand" structs, and initializes it to the 
//(52-numboardcards choose 2) possible hands. it sets the hand+board index,
//the weight to 1, and the HSS value - all values in the "RawHand" struct.
//it then sorts the array of "RawHand"'s by HSS using overloaded < operator 
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

	vector<RawHand> myhands(n_hands);

	ENUMERATE_N_CARDS(b, numboardcards, //works for numboardcards = 0 !
	{
		unsigned int i=0; //bounded by 52 choose 2

		ENUMERATE_2_CARDS_D(m, b, 
		{
			int64 index = getindex2N(m,b,numboardcards); //works for numboardcards = 0 now.
			myhands[i++] = RawHand(index, hsstable[index]); //pushes it back with weight=1, bin=-1
		});

		if(i != n_hands) REPORT("failure to enumerate correctly");

		//now i have an array of hands, with hss. need to sort, compress and bin

		binandstore(myhands, binfile, n_bins, n_hands);
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
	vector<RawHand> hand_list((int64)(1.11f*expected)); 

	//iterate through the bins and associated hands and do bins
	int64 x=0;
	//floater previous=0;
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
				/*
				if(index==1209947)
				{
					cout << endl << "saw index23 " << index << ", "
					<< "(" << tostring(cards_pflop) << " : " << tostring(cards_flop) << ")"
					<< "  index2: " << getindex2(cards_pflop)
					<< "  index23: " << getindex23(cards_pflop, cards_flop)
					<< "  pflopbin: " << pflopbins.retrieve(getindex2(cards_pflop))
					<< "  hss: " << flophss[index];
					if(!fpcompare(flophss[index], previous))
						cout << ", NOT EQUAL";
					previous = flophss[index];
				}
				*/
				hand_list[i++] = RawHand(index, flophss[index]);
			});
		});

		if(i < 0.9*expected || i > 1.1*expected)
			REPORT("found "+tostring(i)+" hands ("+tostring(100.0*i/expected)
					+"% of expected) for preflop bin "+tostring(mypflopbin), WARN);
		if(i-1>=hand_list.size())
			REPORT("...and that overflowed hand_list. make fewer bins for less granularity or make hand_list bigger.");

		binandstore(hand_list, binfile, n_flop, i);
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
	vector<RawHand> hand_list((int64)(1.11f*expected));

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
						if(index==49062272)
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
						hand_list[i++] = RawHand(index, turnhss[getindex2N(cards_pflop, board, 4)]);
					});
				});
			});

			if(i < 0.9*expected || i > 1.1*expected)
				REPORT("found "+tostring(i)+" hands ("+tostring(100.0*i/expected)
						+"% of expected) for preflop/flop bin "+tostring(mypflopbin)
						+'/'+tostring(myflopbin), WARN);
			if(i-1>=hand_list.size())
				REPORT("...and that overflowed hand_list. make fewer bins for less granularity or make hand_list bigger.");

			binandstore(hand_list, binfile, n_turn, i);
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
	vector<RawHand> hand_list((int64)(1.11f*expected));

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
								hand_list[i++] = RawHand(index, riverev[getindex2N(cards_pflop, board, 5)]);
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

				binandstore(hand_list, binfile, n_river, i, RIVERBUNCHHISTBINS); //true = DO bunch similar hands!
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

//utility function used by winrate due to horrible lack of multidimensional array capability.
inline int oneindex(int a, int b, int c, int d, int e, int f, int g, int h, const int n_bins)
{
	return 
		 a 
		+b*n_bins
		+c*n_bins*n_bins
		+d*n_bins*n_bins*n_bins
		+e*n_bins*n_bins*n_bins*n_bins
		+f*n_bins*n_bins*n_bins*n_bins*n_bins
		+g*n_bins*n_bins*n_bins*n_bins*n_bins*n_bins
		+h*n_bins*n_bins*n_bins*n_bins*n_bins*n_bins*n_bins;
}

//where the magic happens
int main(int argc, char *argv[])
{
	checkdataformat();

	if((argc>=2) && (strcmp("histbin", argv[1]) == 0 || strcmp("oldbins", argv[1]) == 0 || strcmp("ramtest", argv[1])==0))
	{
		cout << "Current data type: Using " << FloaterFile::gettypename() << endl;
		cout << "fpcompare uses " << EPSILON << "." << endl;
		cout << "river bunches hands by HSS using fpcompare? " 
			<< (RIVERBUNCHHISTBINS ? "Yes." : "No.") << endl;
		cout << "(everything bunches hands by HSS using fpcompare, with possible exception of river.)" << endl;
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

	//ram test
	else if(argc == 3 && strcmp("ramtest", argv[1])==0)
	{
		int n_pflop, n_flop, n_turn, n_river;
		n_pflop = n_flop = n_turn = n_river = atoi(argv[2]);
		int64 temp, total=0;

		total += temp = 8*PackedBinFile::numwordsneeded(n_pflop, INDEX2_MAX);
		cout << "Preflop bins take " << space(temp) << endl;

		total += temp = 8*PackedBinFile::numwordsneeded(n_flop, INDEX23_MAX);
		cout << "Flop bins take " << space(temp) << endl;

		total += temp = 8*PackedBinFile::numwordsneeded(n_turn, INDEX231_MAX);
		cout << "Turn bins take " << space(temp) << endl;

		total += temp = 8*PackedBinFile::numwordsneeded(n_river, INDEX2311_MAX);
		cout << "River bins take " << space(temp) << endl;

		total += temp = INDEX2311_MAX * sizeof(unsigned char);
		cout << "Counter for river bin file takes " << space(temp) << endl;

		total += temp = sizeof(floater)*INDEX25_MAX; //riverEV
		cout << "RiverEV takes " << space(temp) << endl;

		int64 rawhandlength = 1.11f*56189515200.0f/n_pflop/n_flop/n_turn;
		total += temp = sizeof(RawHand)*rawhandlength;
		cout << "RawHand(" << space(sizeof(RawHand)) << ") list takes " << space(temp) << endl;

		total += temp = sizeof(CompressedHand)*(rawhandlength/1.11f/7);
		cout << "CompressedHands(" << space(sizeof(CompressedHand)) << ") takes " << space(temp) << endl;

		total += temp = sizeof(IndexNode)*(INDEX2311_MAX/n_pflop/n_flop/n_turn-500);
		cout << "IndexNodes(" << space(sizeof(IndexNode)) << ") takes around " << space(temp) << endl;

		cout << endl;
		cout << "Total: " << space(total) << endl;
	}
	
	//winrate
	else if(argc == 4 && strcmp("winrate", argv[1])==0)
	{

		//variables parsed from the command line

		const int n_bins = atoi(argv[2]);
		const int n_bins_tothe8 = n_bins*n_bins*n_bins*n_bins*n_bins*n_bins*n_bins*n_bins;
		const int n_bins_tothe4 = n_bins*n_bins*n_bins*n_bins;
		const int64 num_iter = (int64)atoi(argv[3])*n_bins_tothe8;

		//variables used as the large count arrays

		int64* totals = new int64[n_bins_tothe8];
		int64* wins = new int64[n_bins_tothe8];
		int64* buckettotals = new int64[n_bins_tothe4];
		memset(totals, 0, n_bins_tothe8*sizeof(int64));
		memset(wins, 0, n_bins_tothe8*sizeof(int64));
		memset(buckettotals, 0, n_bins_tothe4*sizeof(int64));

		//variables used to enumerate the cards and compare hands

		CardMask m, h, f, t, r, d, mine, his;
		HandVal r0, r1;
		MTRand mersenne; //used for card dealing, default initialized...that uses clock and time
	
		//variables used (classes) that represent the bin (bucket) files.

		const PackedBinFile binpf(BINSFOLDER+"preflop"+tostring(n_bins), -1, n_bins, INDEX2_MAX, true);
		const PackedBinFile binf(BINSFOLDER+"flop"+tostring(n_bins)+'-'+tostring(n_bins), -1, n_bins, INDEX23_MAX, true);
		const PackedBinFile bint(BINSFOLDER+"turn"+tostring(n_bins)+'-'+tostring(n_bins)+'-'+tostring(n_bins), -1, n_bins, INDEX231_MAX, true);
		const PackedBinFile binr(BINSFOLDER+"river"+tostring(n_bins)+'-'+tostring(n_bins)+'-'+tostring(n_bins)+'-'+tostring(n_bins), -1, n_bins, INDEX2311_MAX, true);
		

		//initialization done, begin code

		cout << "Starting to calculate winrate..." << endl;
		double starttime = getdoubletime();

		//outer loop: do num_iter iterations and write log, repeat.

		for(int k=1;; k++)
		{

			//inner loop: do 1 iteration

			for(int64 i=0; i<num_iter; i++)
			{

				//deal cards randomly

				CardMask_RESET(d);
				MONTECARLO_N_CARDS_D(m, d, 2, 1, );
				MONTECARLO_N_CARDS_D(h, m, 2, 1, );
				CardMask_OR(d,m,h);
				MONTECARLO_N_CARDS_D(f, d, 3, 1, );
				CardMask_OR(d,d,f);
				MONTECARLO_N_CARDS_D(t, d, 1, 1, );
				CardMask_OR(d,d,t);
				MONTECARLO_N_CARDS_D(r, d, 1, 1, );

				//evaluate the hands -- who won.

				CardMask_OR(mine,m,f);
				CardMask_OR(mine,mine,t);
				CardMask_OR(mine,mine,r);
				CardMask_OR(his,h,f);
				CardMask_OR(his,his,t);
				CardMask_OR(his,his,r);
				r0 = Hand_EVAL_N(mine, 7);
				r1 = Hand_EVAL_N(his, 7);

				//retreive the index for this deal of cards and tally up totals and wins
				
				const int totalindex = oneindex(
						binpf.retrieve(getindex2(h)),
						binf.retrieve(getindex23(h,f)),
						bint.retrieve(getindex231(h,f,t)),
						binr.retrieve(getindex2311(h,f,t,r)),
						binpf.retrieve(getindex2(m)),
						binf.retrieve(getindex23(m,f)),
						bint.retrieve(getindex231(m,f,t)),
						binr.retrieve(getindex2311(m,f,t,r)),
						n_bins);

				if(r0 > r1) //I win
					wins[totalindex]+=2;
				else if (r0 == r1) //I tie
					wins[totalindex]++;

				totals[totalindex]++;
				buckettotals[totalindex/n_bins_tothe4]++;

				//end of 1 iteration
			}

			//write the status update to the screen and the logs to disk

			cout << "winrate: finished " << k*num_iter/1000000 << "M iterations." << endl;
			cout << "          = " << k*num_iter/n_bins_tothe8 << " average iterations per bin combo" << endl;
			cout << "         time = " << getdoubletime()-starttime << endl;

			ofstream humanlog("winrate-humans.txt");
			ofstream bucketcoverage("winrate-bucketcoverage.txt");
			ofstream mathematicalog("winrate-winrates.txt");
			ofstream mathematicatotals("winrate-totals.txt");

			for(int m1=0; m1<n_bins; m1++) for(int m2=0; m2<n_bins; m2++) for(int m3=0; m3<n_bins; m3++) for(int m4=0; m4<n_bins; m4++) 
			{
				bucketcoverage << m1 << '/' << m2 << '/' << m3 << '/' << m4 << ":  "
					<< setw(10) << buckettotals[oneindex(0, 0, 0, 0, m1, m2, m3, m4, n_bins)/n_bins_tothe4]
					<< " = " << (float)buckettotals[oneindex(0,0,0,0,m1,m2,m3,m4,n_bins)/n_bins_tothe4]*n_bins_tothe4/(k*num_iter)
				   	<< "x" << endl; 

				for(int h1=0; h1<n_bins; h1++) for(int h2=0; h2<n_bins; h2++) for(int h3=0; h3<n_bins; h3++) for(int h4=0; h4<n_bins; h4++) 
				{
					const int totalindex = oneindex(h1, h2, h3, h4, m1, m2, m3, m4, n_bins); 
					humanlog << m1 << '/' << m2 << '/' << m3 << '/' << m4 
						<< " vs " << h1 << '/' << h2 << '/' << h3 << '/' << h4 << ":  "
						<< setw(5) << wins[totalindex]/2 << " / " << setw(4) << totals[totalindex] 
						<< "   = " << setw(8) << (float)100*wins[totalindex]/(2*totals[totalindex]) << '%' 
						<< "   (" << (float)totals[totalindex]*n_bins_tothe8/(k*num_iter) << "x)" << endl; //relative strength
					mathematicalog << (int)((float)1000*wins[totalindex]/(2*totals[totalindex])) << endl;
					mathematicatotals << totals[totalindex] << endl;
				}
			}

			humanlog.close();
			bucketcoverage.close();
			mathematicalog.close();
			mathematicatotals.close();

			cout << "         log has been written." << endl << endl;

			//end of infinite outer loop
		}
		
		//end of winrate code
	}

	//graph
	else if(argc == 3 && strcmp("graph", argv[1])==0)
	{
		CardMask m,f,tr,b,d;

		//open all the bins and HSS and EV data that we need

		const PackedBinFile pflopbins(BINSFOLDER+"preflop5", -1, 5, INDEX2_MAX, true);
		const PackedBinFile flopbins(BINSFOLDER+"flop5-5", -1, 5, INDEX23_MAX, true);
		const FloaterFile flophss("flopHSS", INDEX23_MAX);
		const FloaterFile riverev("riverEV", INDEX25_MAX);
		const int pflopbinnum = atoi(argv[2]); //offset by 0
		cout << "Using preflop bin " << pflopbinnum << " offset by 0." << endl;

		//prepare the horses

		double starttime = getdoubletime();
		cout << "begining graph creation. Please standby. (One hour left.)" << endl;
		
		ofstream graphlog(("graph-data-bin"+tostring(pflopbinnum)+".txt").c_str());

		//find all Bin 2 preflop hands

		ENUMERATE_2_CARDS(m,
		{
			if(pflopbins.retrieve(getindex2(m)) == pflopbinnum)
			{

				//for each Bin 2 preflop hand, find all possible flops

				ENUMERATE_3_CARDS_D(f,m, 
				{

					//for each flop, find the Hand Strength, not squared

					double strengthsum=0;
					int64 total=0;
					CardMask_OR(d,m,f);
					ENUMERATE_2_CARDS_D(tr,d,
					{
						total++;
						CardMask_OR(b,f,tr);
						strengthsum += riverev[getindex2N(m,b,5)];
					});

					if(total != 47*46/2) REPORT("Enumeration failed count check");

					//for each flop, Print out the Hand Strength, Hand Strength Squared, and the flop bin number (1-5).

					int64 index23 = getindex23(m,f);
					graphlog
						<< flopbins.retrieve(index23) + 1 << ", "
						<< (float)strengthsum / total << ", "
						<< flophss[index23] << endl;

				});
			}
		});

		//epilog. post-mortem.

		graphlog.close();
		cout << "graph log finished. Should have taken about an hour (took " << getdoubletime()-starttime << " seconds)." << endl;
		cout << "format of log, at graph-data.txt, is: " << endl;
		cout << "[flop bin # on a scale of 1-5], [E(hand strength)], [E(hand strength at river ^ 2)]" << endl;
		cout << "for all hands that shared preflop bin # " << pflopbinnum << " of 4 (so #" << pflopbinnum+1 << " if one offset)." << endl;
		cout << "that's (52 choose 2)(1/5)(50 choose 3) hands, count them." << endl;
		cout << "this is for graphing in mathematica to match p.27 of Billings right hand chart" << endl;
	}

	//usage
	else
	{
		cout << "RAM usage of n-n-n-n river histbins: " << argv[0] << " ramtest n_bins" << endl;
		cout << "Create history bins:           " << argv[0] << " histbin n_pflop n_flop n_turn n_river" << endl;
		cout << "Create history bins:           " << argv[0] << " histbin n_pflop n_flop-turn-river" << endl;
		cout << "Create history bins:           " << argv[0] << " histbin n_bins" << endl;
		cout << "Create one round of hist bins: " << argv[0] << " histbin (preflop | flop | turn | river) n_bins ... n_bins" << endl;
		cout << "Create old-style bins:         " << argv[0] << " oldbins (flop | turn | river) n_bins" << endl;
#ifdef COMPILE_TESTCODE
		cout << "Test indexing function:        " << argv[0] << " test" << endl;
#endif
		cout << "Diff two bin files:            " << argv[0] << " diff file1 file2 binmax indextype [-f | errmax]" << endl;
		cout << "Create winrate (histbin only): " << argv[0] << " winrate n_bins num_iter" << endl;
		cout << "Create graph (p.27 Billings):  " << argv[0] << " graph n_preflop_bin" << endl;
		exit(-1);
	}

#ifdef _WIN32
	PAUSE();
#endif
	return 0;
}

