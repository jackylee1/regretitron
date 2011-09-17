#include "stdafx.h"
#include "../HandIndexingV1/getindex2N.h" //The feasibility of bin storage is based on this indexing.
#include "../HandIndexingV1/constants.h"
#include "../PokerLibrary/binstorage.h" //my standard routines to pack and un-pack data.
#include "../MersenneTwister.h"
#include <string>
#include <list>
#include <set>
#include <bitset>
#include <map>
#include <iomanip>
#include <limits>
#include "../utility.h"
#include "../PokerLibrary/floaterfile.h"
#include <boost/math/special_functions/round.hpp>
using namespace std;

const string BINSFOLDER = ""; //current directory
const floater EPSILON = 1e-12;
const bool RIVERBUNCHHISTBINS = false;
const float hand_list_multiplier = 1.3; //to keep it from overflowing (nominally 1.11 for regular bin sizes larger if using 169 PF bins)
const float chunky_bin_tolerance = hand_list_multiplier*0.95;
MTRand binrand;
const bool ACTUALLYDOKMEANS = true;
const int RIVER_HANDTYPE_MAX = 8;
const double HSSKMEANSFACTOR = 2.1;

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
inline floater computeturnHSS2(bool dohss, CardMask mine, CardMask flopturn, const FloaterFile &riverEV)
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
		if( dohss )
			hss += (double)riverEV[i] * riverEV[i];
		else
			hss += (double)riverEV[i];
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
void saveturnHSS( bool = true );
void saveturnEV() { saveturnHSS( false ); }
void saveturnHSS( bool dohss )
{
	cout << "generating turn " << ( dohss ? "HSS" : "EV" ) << " data using " << FloaterFile::gettypename() << "s ..." << endl;

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
				arr.store(i, computeturnHSS2(dohss,m,b,riverEV));
		});
	});

	cout << endl;

	arr.savefile( dohss ? "turnHSS" : "turnEV" );
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
void saveflopHSS( bool = true );
void saveflopEV() { saveflopHSS( false ); }
void saveflopHSS( bool dohss )
{
	cout << "generating flop " << ( dohss ? "HSS" : "EV" ) << " data using " << FloaterFile::gettypename() << "s ..." << endl;

	FloaterFile arr(INDEX23_MAX);
	const FloaterFile turnHSS( ( dohss ? "turnHSS" : "turnEV" ) , INDEX24_MAX);

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

	arr.savefile( dohss ? "flopHSS" : "flopEV" );
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

//utility function used for nesting
int getnumbins( string binname )
{
	const size_t xpos = binname.find( "x" );
	if( xpos == string::npos )
		return fromstr< int >( binname );
	else
		return fromstr< int >( binname.substr( 0, xpos ) )
			* fromstr< int >( binname.substr( xpos + 1 ) );
}

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
	float tolerance = 1 + (hand_list_multiplier-1)/2;
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
		cout << "\nWarning: "+tostring(n_bins)+" is too many bins; we used "+tostring(currentbin+1)+". Could not find enough hands to fill them. "
				+"We were binning "+tostring(total_hands)+" hands. ("+tostring(compressed_size)+" compressed)." << endl;
	if(chunky)
		cout << endl << "Warning: This binning process has produced chunky bins, report follows:\n"
			<< status.str() << endl;
	if(print)
		cout << endl << status.str() << flush;
}

inline void binandstore(vector<RawHand> &hand_list, PackedBinFile &binfile, int n_bins, int64 total_hands, bool bunchsimilarhands = true)
{
	//hand_list will be larger than it needs to be and any unused slots will have
	//weight zero. we need sort to sort these to the END.

	sort(hand_list.begin(), hand_list.begin()+total_hands); //large-hss now towards END OF ARRAY

	//compress the hands, getting the new size

	static vector<CompressedHand> hand_list_compressed(0); //massive hack so we don't reallocate all the time
	if((int64)hand_list_compressed.size() < total_hands/8) hand_list_compressed.resize(total_hands/7);
	if( total_hands < 5000 ) hand_list_compressed.resize( 5000 );
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
		binfile.store( hand_list_compressed[i].index, hand_list_compressed[i].bin );
		IndexNode * nextindex = hand_list_compressed[i].nextindex;
		hand_list_compressed[i].nextindex = NULL;
		while(nextindex != NULL)
		{
			binfile.store( nextindex->index, hand_list_compressed[i].bin );
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

//-------------- N e w   B i n s   C a l c u l a t i o n ---------------

struct FPLess
{
	bool operator( )( const floater & a, const floater & b )
	{
		return a < b && ! fpcompare( a, b );
	}
};

class Binner
{
public:
	Binner( )
		: m_totalhands( 0 )
	{ }

	void Add( int64 index, floater quality )
	{
		HandValue & myval = m_map[ quality ];
		myval.m_weight++;
		m_totalhands++;
		myval.m_indices.insert( index );
	}

	void Bin( PackedBinFile & output, int numbins, const bool qualityisev )
	{
		if( numbins == 0 )
		{
			if( m_totalhands || m_map.size( ) )
				REPORT( "binning hands into zero bins" );
			return;
		}

		DetermineBins( numbins, qualityisev );

		for( HandMap::iterator i = m_map.begin( ); i != m_map.end( ); i++ )
			for( set< int64 >::iterator j = i->second.m_indices.begin( ); j != i->second.m_indices.end( ); j++ )
				output.store( *j, i->second.m_bin );
	}

	int64 GetTotalSize( ) { return m_totalhands; }
	int64 GetCompressedSize( ) { return m_map.size( ); }

private:
	void DetermineBins( int numbins, const bool qualityisev )
	{
		const bool print = false;
		bool chunky = false;
		float expected = (float)m_totalhands/numbins; //expected bins per hand
		float tolerance = 1 + (hand_list_multiplier-1)/2;
		ostringstream status;
		int currentbin=0;
		int currentbinsize=0;
		int64 n_hands_left = m_totalhands;

		status << " binning " << m_totalhands << " hands (" << m_map.size( )
			<< " compressed) into " << numbins << " bins. expect " << expected << " hands per bin "
			<< "(tolerance = " << tolerance << ")..." << endl;

		if( numbins == 1 )
		{
			for( HandMap::iterator i = m_map.begin( ); i != m_map.end( ); i++ )
				i->second.m_bin = 0;
		}
		else if( qualityisev || (int)m_map.size( ) < 5 * numbins )
		{
#if 0 /* old slow method O(n^2) */
			// each pass through the loop removes one bin
			while( m_map.size( ) > (unsigned)numbins )
			{
				double bestscore = numeric_limits< double >::infinity( );
				HandMap::iterator bestelleft;
				HandMap::iterator bestelright;
				for( HandMap::iterator i = m_map.begin( ), j = ++m_map.begin( ); 
						j != m_map.end( ); i++, j++ )
				{
					double thisscore = calcscore( i, j );
					if( thisscore < bestscore )
					{
						bestelleft = i;
						bestelright = j;
						bestscore = thisscore;
					}
				}

				//condense the two into one
				pair< floater, HandValue > condensed;
				condensed.second.m_weight = bestelleft->second.m_weight + bestelright->second.m_weight;
				condensed.second.m_indices.insert( bestelleft->second.m_indices.begin( ), bestelleft->second.m_indices.end( ) );
				condensed.second.m_indices.insert( bestelright->second.m_indices.begin( ), bestelright->second.m_indices.end( ) );
				condensed.first = 
					( bestelleft->first * bestelleft->second.m_weight + 
					bestelright->first * bestelright->second.m_weight ) /
					condensed.second.m_weight;
				const int oldsize = m_map.size( );
				if( bestelleft == m_map.begin( ) )
				{
					m_map.erase( bestelleft );
					m_map.erase( bestelright );
					m_map.insert( m_map.begin( ), condensed );
				}
				else
				{
					m_map.erase( bestelleft-- );
					m_map.erase( bestelright );
					m_map.insert( bestelleft, condensed );
				}
				if( m_map.size( ) != static_cast< unsigned >( oldsize - 1 ) )
					REPORT( "condensed was annilated\n" );
			}
#else /* new fast method O(n) */
			typedef multimap< double, HandMap::iterator > SpaceMap;
			SpaceMap spacemap;
			typedef map< floater, SpaceMap::iterator, FPLess > ElementMap;
			ElementMap elementmap;

			//prime the workings
			if( ! m_map.empty( ) )
				for( HandMap::iterator i = m_map.begin( ), j = ++m_map.begin( ); 
						j != m_map.end( ); i++, j++ )
				{
					double thisscore = calcscore( i, j );
					SpaceMap::iterator spaceiter = spacemap.insert( SpaceMap::value_type( thisscore, i ) );
					pair< ElementMap::iterator, bool > elementresult = elementmap.insert( ElementMap::value_type( i->first, spaceiter ) );
					if( ! elementresult.second )
						REPORT( "faulty insertion!" );
				}

			// each pass through the loop removes one bin
			while( m_map.size( ) > (unsigned)numbins )
			{
				SpaceMap::iterator spacemapbest = spacemap.begin( );
				HandMap::iterator handmapleft = spacemapbest->second;
				HandMap::iterator handmapright = handmapleft;
				handmapright++;
				const bool moreleft = ( handmapleft != m_map.begin( ) );
				const bool moreright = ( handmapright != --m_map.end( ) );
				ElementMap::iterator elementleft = elementmap.find( handmapleft->first );
				ElementMap::iterator elementright = elementmap.find( handmapright->first );
				if( elementleft->second != spacemapbest )
					REPORT( "bad spacemap/elementmap back ptr" );
				if( elementleft == elementright || elementleft == elementmap.end( ) || ( elementright == elementmap.end( ) ) == moreright )
					REPORT( "bad elementmap lookup" );
				HandMap::iterator handmapleftleft = m_map.end( );
				ElementMap::iterator elementleftleft = elementmap.end( );
				SpaceMap::iterator spacetoleft = spacemap.end( );
				if( moreleft )
				{
					--( handmapleftleft = handmapleft );
					--( elementleftleft = elementleft );
					spacetoleft = elementleftleft->second;
				}
				HandMap::iterator handmaprightright = m_map.end( );
				SpaceMap::iterator spacetoright = spacemap.end( );
				if( moreright )
				{
					++( handmaprightright = handmapright );
					spacetoright = elementright->second;
				}

				//condense the two into one in HandMap
				pair< floater, HandValue > handmapcondensed;
				handmapcondensed.second.m_weight = handmapleft->second.m_weight + handmapright->second.m_weight;
				handmapcondensed.second.m_indices.insert( handmapleft->second.m_indices.begin( ), handmapleft->second.m_indices.end( ) );
				handmapcondensed.second.m_indices.insert( handmapright->second.m_indices.begin( ), handmapright->second.m_indices.end( ) );
				handmapcondensed.first = 
					( handmapleft->first * handmapleft->second.m_weight + 
					  handmapright->first * handmapright->second.m_weight ) /
					handmapcondensed.second.m_weight;
				const uint64 oldsize = m_map.size( );
				if( m_map.size( ) != elementmap.size( ) + 1 )
					REPORT( "bad element map size" );
				if( m_map.size( ) != spacemap.size( ) + 1 )
					REPORT( "bad space map size" );
				m_map.erase( handmapleft );
				m_map.erase( handmapright );
				HandMap::iterator condensediter = m_map.insert( handmapcondensed ).first;
				elementmap.erase( elementleft );
				spacemap.erase( spacemapbest );
				if( moreright )
				{
					elementmap.erase( elementright );
					spacemap.erase( spacetoright );
					//recalculate score of spacetoright and put it back in (assign spacetoright to be the new iterator)
					spacetoright = spacemap.insert( SpaceMap::value_type( calcscore( condensediter, handmaprightright ), condensediter ) );
					elementmap.insert( ElementMap::value_type( handmapcondensed.first, spacetoright ) );
				}
				if( moreleft )
				{
					spacemap.erase( spacetoleft );
					//recalculate score of spacetoleft and put it back in (assign spacetoleft to be the new iterator)
					spacetoleft = spacemap.insert( SpaceMap::value_type( calcscore( handmapleftleft, condensediter ), handmapleftleft ) );
					elementleftleft->second = spacetoleft;
				}
				if( m_map.size( ) != oldsize - 1 )
					REPORT( "handmapcondensed was annilated\n" );
				if( m_map.size( ) != elementmap.size( ) + 1 )
					REPORT( "bad element map size" );
				if( m_map.size( ) != spacemap.size( ) + 1 )
					REPORT( "bad space map size" );
			}
#endif

			for( HandMap::iterator i = m_map.begin( ); i != m_map.end( ); i++ )
			{
				status << "  bin " << currentbin << " has avg quality " << i->first << " with " << i->second.m_weight << " hands (" << 100.0*i->second.m_weight/expected << "% of expected)" << endl;

				if(i->second.m_weight > expected * tolerance || i->second.m_weight < expected / tolerance)
					chunky = true;

				i->second.m_bin = currentbin++;
			}

			currentbin--; //matches how other algo leaves currentbin as last bin used
		}
		else
		{
			for( HandMap::iterator i = m_map.begin( ); i != m_map.end( ); i++ )
			{
				//increment bins if needed

				if(currentbinsize + i->second.m_weight/2 > n_hands_left/(numbins-currentbin) && currentbinsize > 0)
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
					if(currentbin==numbins) REPORT("determinebins has reached a fatal error");
				}

				//then always save hand to current bin

				i->second.m_bin = currentbin;
				currentbinsize +=i->second.m_weight;
			}

			status << "  bin " << currentbin << " has " << currentbinsize << " hands (" <<
				100.0*currentbinsize/expected << "% of expected)";
			if(currentbinsize != n_hands_left) //this would just be a bug
				REPORT("Did not bin the right amount of hands!");
		}

		if(currentbin != numbins-1) //as mentioned in comments, this is possible
			cerr << "\nWarning: "+tostring(numbins)+" is too many bins; we used "+tostring(currentbin+1)+". Could not find enough hands to fill them. "
				+"We were binning "+tostring(m_totalhands)+" hands. ("+tostring(m_map.size( ))+" compressed)." << endl;
		if(chunky)
			cerr << endl << "Warning: This binning process has produced chunky bins, report follows:\n"
				<< status.str() << endl;
		if(print)
			cout << endl << status.str() << flush;
	}


	struct HandValue
	{
		HandValue( ) 
		  : m_weight( 0 )
		  , m_bin( -1 ) 
		{ }
		int32 m_weight;
		int32 m_bin;
		set< int64 > m_indices;
	};

	typedef std::map< floater, HandValue, FPLess > HandMap;
	HandMap m_map;
	int64 m_totalhands;

	double calcscore( HandMap::const_iterator left, HandMap::const_iterator right )
	{
		return
			static_cast< double >( left->second.m_weight ) * 
			static_cast< double >( right->second.m_weight ) * 
			static_cast< double >( right->first - left->first ) *
			static_cast< double >( right->first - left->first );
	}
};

//------------ B o a r d   C l u s t e r i n g -------------

const int CHARCOUNT = 10;

template< typename T, int DIMS >
struct ClusterChar
{
	ClusterChar( ) { zero( ); }
	T c[ DIMS ];
	void zero( ) { memset( c, 0, sizeof( c ) ); } 

	template < typename U >
	ClusterChar< T, DIMS > & operator=( const ClusterChar< U, DIMS > & that )
	{
		for( int i = 0; i < DIMS; i++ )
			c[ i ] = that.c[ i ];
		return * this;
	}

	template < typename U >
	ClusterChar< T, DIMS > & operator+=( const ClusterChar< U, DIMS > & that )
	{
		for( int i = 0; i < DIMS; i++ )
			c[ i ] += that.c[ i ];
		return * this;
	}

	template < typename U >
	ClusterChar< T, DIMS > & operator/=( const U & that )
	{
		for( int i = 0; i < DIMS; i++ )
			c[ i ] /= that;
		return * this;
	}

	template < typename U >
	ClusterChar< T, DIMS > & operator*=( const U & that )
	{
		for( int i = 0; i < DIMS; i++ )
			c[ i ] *= that;
		return * this;
	}

	template < typename U >
	void add( int & elements, int & count, const ClusterChar< U, DIMS > & that, int weight )
	{
		for( int i = 0; i < DIMS; i++ )
			c[ i ] = ( c[ i ] * count + that.c[ i ] * weight ) / ( count + weight );
		count += weight;
		elements++;
	}

	template < typename U >
	void subtract( int & elements, int & count, const ClusterChar< U, DIMS > & that, int weight )
	{
		for( int i = 0; i < DIMS; i++ )
			c[ i ] = ( c[ i ] * count - that.c[ i ] * weight ) / ( count - weight );
		count -= weight;
		elements--;
	}
};
// can't use commas in a macro so i need these typedefs for the macros that go through cards ( i use pokereval's macros )
typedef ClusterChar< int, CHARCOUNT * CHARCOUNT > BoardChar;
typedef ClusterChar< double, 1 > Hand1Char;
typedef ClusterChar< double, 2 > Hand2Char;

template < int DIMS >
bool operator < ( const ClusterChar< int, DIMS > & left, const ClusterChar< int, DIMS > & right )
{
	for( int i = 0; i < DIMS; i++ )
		if( left.c[ i ] == right.c[ i ] )
			continue;
		else if( left.c[ i ] < right.c[ i ] )
			return true;
		else if( left.c[ i ] > right.c[ i ] )
			return false;
	return false; // all dims are equal
}

template < int DIMS >
bool operator < ( const ClusterChar< double, DIMS > & left, const ClusterChar< double, DIMS > & right )
{
	for( int i = 0; i < DIMS; i++ )
		if( fpcompare( left.c[ i ], right.c[ i ] ) )
			continue;
		else if( left.c[ i ] < right.c[ i ] )
			return true;
		else if( left.c[ i ] > right.c[ i ] )
			return false;
	return false; // all dims are equal
}

inline double square( double x ) { return x * x; }

template< typename T1, typename T2, int DIMS >
double get_distance( const ClusterChar< T1, DIMS > & b1, const ClusterChar< T2, DIMS > & b2 )
{
	double total = 0;
	for( int i = 0; i < DIMS; i++ )
		total += square( b1.c[ i ] - b2.c[ i ] );
	return sqrt( total );
}



//const string KMEANSINITMETHOD = "index groupings";
const string KMEANSINITMETHOD = "randomly selected boards";
template < typename T, int DIMS >
class Clusterer
{
public:
	Clusterer( )
		: m_totalhands( 0 )
	{ }

	void Add( int64 index, const ClusterChar< T, DIMS > & quality )
	{
		HandValue & myval = m_map[ quality ];
		myval.m_weight++;
		m_totalhands++;
		myval.m_indices.insert( index );
	}

	void Cluster( PackedBinFile & output, int numclusters, const string & name )
	{
		if( numclusters == 0 )
		{
			if( m_totalhands || m_map.size( ) )
				REPORT( "clustering hands into zero clusters" );
			return;
		}

		DetermineClusters( numclusters, name );

		for( typename HandMap::iterator i = m_map.begin( ); i != m_map.end( ); i++ )
			for( set< int64 >::iterator j = i->second.m_indices.begin( ); j != i->second.m_indices.end( ); j++ )
				output.store( *j, i->second.m_bin );
	}

	int64 GetTotalSize( ) { return m_totalhands; }
	int64 GetCompressedSize( ) { return m_map.size( ); }

private:

#if 0
	// assign each board to a cluster, then compute the initial centroids from that
	// this board assignment seems to be pretty good on its own (just uses index)
	void initialize_kmeans( vector< ClusterStruct< T, DIMS > > & boards, vector< ClusterChar< double, DIMS > > & centroids )
	{
		// assign the clusters by grouping near indices together
		// the way I do it, each cluster gets an equal number of boards (up to rounding)

		int count = 0;
		sort( boards.begin( ), boards.end( ), IndexSorter( ) );
		const int stride = ( boards.size( ) + centroids.size( ) - 1 ) / centroids.size( );

		// assign the cluster and sum up the centroids in one go

		vector< int > centroidcounts( centroids.size( ), 0 );
		for( typename vector< ClusterChar< double, DIMS > >::iterator c = centroids.begin( ); c != centroids.end( ); c++ )
			c->zero( ); // zero out the centroid vectors
		for( typename vector< ClusterStruct< T, DIMS > >::iterator b = boards.begin( ); b != boards.end( ); b++ )
		{
			b->cluster = ( ( count++ / stride ) % centroids.size( ) );
			centroids[ b->cluster ] += b->characteristic;
			centroidcounts[ b->cluster ]++;
		}

		// make sure we assigned everything and calculate the centroids
		
		for( unsigned i = 0; i < centroids.size( ); i++ )
		{
			if( centroidcounts[ i ] == 0 )
				REPORT( "kmeans fault not enough boards" );
			centroids[ i ] /= centroidcounts[ i ];
		}
	}
#endif

#if 1
	// assign each centroid to be a randomly selected board
	// this algorithm only assigns the centroids, not the boards
	void initialize_kmeans( vector< ClusterChar< double, DIMS > > & centroids )
	{
		// this algorithm is the same as my card shuffling algorithm

		vector< int > boardstouse( m_map.size( ) );
		for( unsigned i = 0; i < m_map.size( ); i++ )
			boardstouse[ i ] = i;
		for( unsigned i = 0; i < centroids.size( ); i++ )
			swap( boardstouse[ i ], boardstouse[ i + binrand.randInt( m_map.size( ) - 1 - i ) ] );
		sort( boardstouse.begin( ), boardstouse.begin( ) + centroids.size( ) );
		int last = 0;
		typename HandMap::iterator boardtouse = m_map.begin( );
		for( unsigned i = 0; i < centroids.size( ); i++ )
		{
			for( int j = last; j < boardstouse[ i ]; j++ )
				boardtouse++;
			last = boardstouse[ i ];
			centroids[ i ] = boardtouse->first;
		}
	}
#endif

	// I ported this code myself from some Fortran code I found
	// I hope it is still correct, I don't fully understand it's workings...
	// some portions are opaque, and the original had 1-character variables.
	// Still, I adapted it to use a weighted approach, i.e., not to just put
	// 24 of the same thing in there, but to put it in once with a 24 weight.
	// I hope I did the weighting correctly... It seemed releatively straight-forward
	// but if I could remove it I would. It turned out not to even solve the problem
	// I was trying to solve (I think there was a different bug, an int that should
	// have been a double).
	void DetermineClusters( int n_clusters, const string & name )
	{
		vector< typename HandMap::iterator > elements( m_map.size( ) );
		{
			typename HandMap::iterator iter = m_map.begin( );
			for( unsigned i = 0; i < m_map.size( ); i++ )
				elements[ i ] = iter++;
		}

		cerr << "For " << name << " clusters:" << endl;
		cerr << "clustering " << m_totalhands << " total (" << m_map.size( ) << " compressed) into " << n_clusters << " clusters." << endl;

		if( (unsigned)n_clusters >= elements.size( ) )
			REPORT( "k >= n in kmeans" );
		if( n_clusters <= 1 )
			REPORT( "k <= 1 in kmeans" );

		list< vector< int > > trialsclusters;
		list< double > trialserrors;

beginclustering:

		vector< ClusterChar< double, DIMS > > centroids( n_clusters );
		initialize_kmeans( centroids );
		vector< int > clustercounts( n_clusters, 0 );
		vector< int > clusterelements( n_clusters, 0 );
		vector< double > clustererrors( n_clusters, 0 );
		int n_iter = 0;
		const int MAX_ITER_ALLOWED = 100;

		// loop through to find the first and second closest clusters for each point
		// assign each board to its nearest centroid

		vector< int > secondcluster( elements.size( ) ); //second closest cluster
		for( unsigned i = 0; i < elements.size( ); i++ )
		{
			// prime the loop with the first two centroids

			elements[ i ]->second.m_bin = 0;
			secondcluster[ i ] = 1;
			double distance1 = get_distance( elements[ i ]->first, centroids[ 0 ] );
			double distance2 = get_distance( elements[ i ]->first, centroids[ 1 ] );
			if( distance2 < distance1 )
			{
				swap( elements[ i ]->second.m_bin, secondcluster[ i ] );
				swap( distance1, distance2 );
			}

			// loop through the rest of the clusters to finish finding the best and second best clusters

			for( int j = 2; j < n_clusters; j++ )
			{
				double newdistance = get_distance( elements[ i ]->first, centroids[ j ] );
				if( newdistance < distance1 )
				{
					distance2 = distance1;
					distance1 = newdistance;
					secondcluster[ i ] = elements[ i ]->second.m_bin;
					elements[ i ]->second.m_bin = j;
				}
				else if( newdistance < distance2 )
				{
					distance2 = newdistance;
					secondcluster[ i ] = j;
				}
			}
		}

		// recompute the cluster centroids by averaging the elements that were assigned to them

		for( int i = 0; i < n_clusters; i++ )
			centroids[ i ].zero( );

		for( unsigned i = 0; i < elements.size( ); i++ )
		{
			clusterelements[ elements[ i ]->second.m_bin ]++;
			clustercounts[ elements[ i ]->second.m_bin ] += elements[ i ]->second.m_weight;

			ClusterChar< T, DIMS > temp = elements[ i ]->first;
			temp *= elements[ i ]->second.m_weight;
			centroids[ elements[ i ]->second.m_bin ] += temp;
		}


		// initialize for the optimal transfer stage
		// if there are any empty clusters, we produce fault 1 

		vector< double > an1( n_clusters ); // n / ( n - 1 )
		vector< double > an2( n_clusters ); // n / ( n + 1 )
		vector< bool > itran( n_clusters, true );
		vector< int > ncp( n_clusters, -1 );
		unsigned indx = 0;
		vector< double > d( elements.size( ), 0 );
		vector< int > live( n_clusters, 0 );

		for( int i = 0; i < n_clusters; i++ )
		{
			if( clustercounts[ i ] == 0 )
				REPORT( "clusterfault1" );

			centroids[ i ] /= clustercounts[ i ];

			if( clustercounts[ i ] == 1 )
				an1[ i ] = numeric_limits< double >::infinity( );
			else
				an1[ i ] = static_cast< double >( clustercounts[ i ] ) / ( clustercounts[ i ] - 1 );

			an2[ i ] = static_cast< double >( clustercounts[ i ] ) / ( clustercounts[ i ] + 1 );
		}

		for( n_iter = 1; n_iter <= MAX_ITER_ALLOWED; n_iter++ ) 
		{

			// this is the optimal transfer stage

			for( int i = 0; i < n_clusters; i++ )
				if( itran[ i ] )
					live[ i ] = elements.size( ) + 1;

			for( int i = 0; static_cast< unsigned >( i ) < elements.size( ); i++ )
			{
				indx++;
				int L1 = elements[ i ]->second.m_bin;
				int L2 = secondcluster[ i ];
				int LL = L2;

				if( clusterelements[ L1 ] > 1 )
				{
					if( ncp[ L1 ] != 0 )
						d[ i ] = an1[ L1 ] * get_distance( elements[ i ]->first, centroids[ L1 ] );

					double R2 = an2[ L2 ] * get_distance( elements[ i ]->first, centroids[ L2 ] );

					for( int j = 0; j < n_clusters; j++ )
					{
						if( ( i < live[ L1 ] || i < live[ L2 ] ) && j != L1 && j != LL )
						{
							const double newdist = get_distance( elements[ i ]->first, centroids[ j ] );
							if( newdist < R2 / an2[ j ] )
							{
								R2 = newdist * an2[ j ];
								L2 = j;
							}
						}
					}

					if( d[ i ] <= R2 )
					{
						secondcluster[ i ] = L2;
					}
					else
					{
						indx = 0;
						live[ L1 ] = elements.size( ) + i;
						live[ L2 ] = elements.size( ) + i;
						ncp[ L1 ] = i;
						ncp[ L2 ] = i;
						centroids[ L1 ].subtract( clusterelements[ L1 ], clustercounts[ L1 ], elements[ i ]->first, elements[ i ]->second.m_weight );
						centroids[ L2 ].add( clusterelements[ L2 ], clustercounts[ L2 ], elements[ i ]->first, elements[ i ]->second.m_weight );
						if( clustercounts[ L1 ] == 1 )
							an1[ L1 ] = numeric_limits< double >::infinity( );
						else
							an1[ L1 ] = static_cast< double >( clustercounts[ L1 ] ) / ( clustercounts[ L1 ] - 1 );
						an2[ L1 ] = static_cast< double >( clustercounts[ L1 ] ) / ( clustercounts[ L1 ] + 1 );
						an1[ L2 ] = static_cast< double >( clustercounts[ L2 ] ) / ( clustercounts[ L2 ] - 1 );
						an2[ L2 ] = static_cast< double >( clustercounts[ L2 ] ) / ( clustercounts[ L2 ] + 1 );
						elements[ i ]->second.m_bin = L2;
						secondcluster[ i ] = L1;
					}
				}

				if( indx == elements.size( ) )
					goto endoptra;

			}

			for( int j = 0; j < n_clusters; j++ )
			{
				itran[ j ] = false;
				live[ j ] -= elements.size( );
			}

endoptra:

			if( indx == elements.size( ) )
				break;

			// this is the quick transfer stage

			unsigned icoun = 0;
			int istep = 0;

			while( true )
			{
				for( unsigned i = 0; i < elements.size( ); i++ )
				{
					icoun++;
					istep++;
					int L1 = elements[ i ]->second.m_bin;
					int L2 = secondcluster[ i ];

					if( clusterelements[ L1 ] > 1 )
					{
						if( ncp[ L1 ] >= istep )
							d[ i ] = an1[ L1 ] * get_distance( elements[ i ]->first, centroids[ L1 ] );

						if( ncp[ L1 ] > istep || ncp[ L2 ] > istep )
						{
							const double newdist = get_distance( elements[ i ]->first, centroids[ L2 ] );
							if( newdist < d[ i ] / an2[ L2 ] )
							{
								icoun = 0;
								indx = 0;
								itran[ L1 ] = true;
								itran[ L2 ] = true;
								ncp[ L1 ] = istep + elements.size( );
								ncp[ L2 ] = istep + elements.size( );
								centroids[ L1 ].subtract( clusterelements[ L1 ], clustercounts[ L1 ], elements[ i ]->first, elements[ i ]->second.m_weight );
								centroids[ L2 ].add( clusterelements[ L2 ], clustercounts[ L2 ], elements[ i ]->first, elements[ i ]->second.m_weight );
								if( clustercounts[ L1 ] == 1 )
									an1[ L1 ] = numeric_limits< double >::infinity( );
								else
									an1[ L1 ] = static_cast< double >( clustercounts[ L1 ] ) / ( clustercounts[ L1 ] - 1 );
								an2[ L1 ] = static_cast< double >( clustercounts[ L1 ] ) / ( clustercounts[ L1 ] + 1 );
								an1[ L2 ] = static_cast< double >( clustercounts[ L2 ] ) / ( clustercounts[ L2 ] - 1 );
								an2[ L2 ] = static_cast< double >( clustercounts[ L2 ] ) / ( clustercounts[ L2 ] + 1 );
								elements[ i ]->second.m_bin = L2;
								secondcluster[ i ] = L1;
							}
						}
					}

					if( icoun == elements.size( ) )
						goto endqtran;
				}
			}

endqtran:

			if( n_clusters == 2 )
				break;

			// initialize for optra again
			for( int i = 0; i < n_clusters; i++ )
				ncp[ i ] = 0;
		}

		for( int i = 0; i < n_clusters; i++ )
		{
			clusterelements[ i ] = 0;
			clustercounts[ i ] = 0;
			clustererrors[ i ] = 0;
			centroids[ i ].zero( );
		}

		for( typename HandMap::iterator i = m_map.begin( ); i != m_map.end( ); i++ )
		{
			clusterelements[ i->second.m_bin ]++;
			clustercounts[ i->second.m_bin ] += i->second.m_weight;

			ClusterChar< T, DIMS > temp = i->first;
			temp *= i->second.m_weight;
			centroids[ i->second.m_bin ] += temp;
		}

		for( int i = 0; i < n_clusters; i++ )
			centroids[ i ] /= clustercounts[ i ];

		double totalerror = 0;
		for( typename HandMap::iterator i = m_map.begin( ); i != m_map.end( ); i++ )
		{
			const double newerror = square( get_distance( i->first, centroids[ i->second.m_bin ] ) );
			clustererrors[ i->second.m_bin ] += newerror;
			totalerror +=newerror;
		}

		cerr << "Final error: " << totalerror << endl;
		cerr << "Iterations: " << n_iter;
		if( n_iter > MAX_ITER_ALLOWED )
			cerr << "(MAX ITER)";
		cerr << endl << "Cluster number: size of cluster (wss error)" << endl;
		for( int i = 0; i < n_clusters; i++ )
		{
			cerr << i << ": " << clustercounts[ i ] << " (" << clustererrors[ i ] << ")" << endl;
		}
		cerr << endl;

		trialserrors.push_back( totalerror );
		trialsclusters.push_back( vector< int >( ) );
		vector< int > & lasttrial = trialsclusters.back( );
		lasttrial.resize( elements.size( ) );
		for( unsigned i = 0; i < elements.size( ); i++ )
			lasttrial[ i ] = elements[ i ]->second.m_bin;

		if( trialserrors.size( ) < 20 )
			goto beginclustering;

		const list< double >::iterator besterror = min_element( trialserrors.begin( ), trialserrors.end( ) );
		list< double >::iterator err = trialserrors.begin( ); 
		list< vector< int > >::iterator vec = trialsclusters.begin( ); 
		for( ; err != besterror; err++, vec++ );

		cerr << ">>> Of " << trialserrors.size( ) << " trials, chose the one with error " << * besterror << endl;
		for( unsigned i = 0; i < elements.size( ); i++ )
			elements[ i ]->second.m_bin = vec->operator[]( i );
	}

	struct HandValue
	{
		HandValue( ) 
			: m_weight( 0 )
			  , m_bin( -1 ) 
		{ }
		int32 m_weight;
		int32 m_bin;
		set< int64 > m_indices;
	};

	typedef std::map< ClusterChar< T, DIMS >, HandValue > HandMap;
	HandMap m_map;
	int64 m_totalhands;
};

void saveboardflopclusters( int n_new )
{
	cout << "generating b" << n_new << " flop clusters..." << endl;

	//the final list of boards we are trying to construct
	Clusterer< int, CHARCOUNT * CHARCOUNT > boards;

	//the bin file that will receive the output
	PackedBinFile output(n_new, INDEX3_MAX);

	if( n_new == 1 )
	{
		for( int i = 0; i < INDEX3_MAX; i++ )
			output.store( i, 0 );
	} else { // bracket is closed near end of function

	//the bin information used to characterize the boards
	const PackedBinFile prebins(
			BINSFOLDER + "preflop" + tostr( CHARCOUNT ), 
			-1, CHARCOUNT, INDEX2_MAX, true );
	const PackedBinFile postbins(
			BINSFOLDER + "flop" + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ), 
			-1, CHARCOUNT, INDEX23_MAX, true);

	//enumerate through all flops
	int x = 0;
	CardMask cards_flop;
	ENUMERATE_3_CARDS(cards_flop,
	{
		if(x++ % (277) == 0) cout << '.' << flush;

		//create a new BoardStruct with its index
		const int64 index = getindex3( cards_flop );
		BoardChar charac;

		//characterize it:
		//for each private hand
		CardMask cards_hole;
		ENUMERATE_2_CARDS_D( cards_hole, cards_flop,
		{
			//lookup that hand's preflop 10-bin and flop 10-bin
			const int pre = prebins.retrieve( getindex2( cards_hole ) );
			const int post = postbins.retrieve( getindex23( cards_hole, cards_flop ) );

			//increment that element of the array
			charac.c[ combine( pre, post, CHARCOUNT ) ]++;
		});
	
		boards.Add( index, charac );
	});
	cout << endl;

	//pass the whole thing to clustering and storing to output
	boards.Cluster( output, n_new, "flopb" + tostr( n_new ) );

	} // end of n_new == 1 case

	//save the file
	cout << output.save( BINSFOLDER + "flopb" + tostr( n_new ) );
}

void saveboardturnclusters( const int n_flop, const int n_new )
{
	cout << "generating b" << n_flop << " - b" << n_new << " turn clusters..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_new, INDEX31_MAX );

	if( n_new == 1 )
	{
		for( int i = 0; i < INDEX31_MAX; i++ )
			output.store( i, 0 );
	} else { // bracket is closed near end of function

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flop ),
			-1, n_flop, INDEX3_MAX, true );

	//the bin information used to characterize the boards
	const PackedBinFile prebins(
			BINSFOLDER + "flop" + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ), 
			-1, CHARCOUNT, INDEX23_MAX, true );
	const PackedBinFile postbins(
			BINSFOLDER + "turn" + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ), 
			-1, CHARCOUNT, INDEX231_MAX, true);

	//for each flop cluster
	int x = 0;
	for( int myflopcluster = 0; myflopcluster < n_flop; myflopcluster++ )
	{
		//the final list of boards we are trying to construct
		Clusterer< int, CHARCOUNT * CHARCOUNT > boards;

		//enumerate through all flops, 
		CardMask cards_flop;
		ENUMERATE_3_CARDS(cards_flop,
		{
			if(x++ % (277 * n_flop) == 0) cout << '.' << flush;

			// reject the ones not in the current flop cluster of interest
			if( flopclusters.retrieve( getindex3( cards_flop ) ) != myflopcluster )
				continue;

			//enumerate through all turns
			CardMask card_turn;
			ENUMERATE_1_CARDS_D(card_turn, cards_flop,
			{
				//create a new BoardStruct with its index
				const int64 index = getindex31( cards_flop, card_turn );
				BoardChar board;

				//characterize it:
				//for each private hand
				CardMask cards_hole;
				CardMask cards_used;
				CardMask_OR( cards_used, cards_flop, card_turn );
				ENUMERATE_2_CARDS_D( cards_hole, cards_used,
				{
					//lookup that hand's preflop 10-bin and flop 10-bin
					const int pre = prebins.retrieve( getindex23( cards_hole, cards_flop ) );
					const int post = postbins.retrieve( getindex231( cards_hole, cards_flop, card_turn ) );

					//increment that element of the array
					board.c[ combine( pre, post, CHARCOUNT ) ]++;
				});

				boards.Add( index, board );
			});
		});

		//pass the whole thing to clustering and storing to output
		boards.Cluster( output, n_new, "turnb" + tostr( n_flop ) + "-b" + tostr( n_new ) + " (doing " + tostr( myflopcluster ) + ")" );
	}
	cout << endl;

	} // end of n_new == 1 case

	//save the file
	cout << output.save( BINSFOLDER + "turnb" + tostr( n_flop ) + "-b" + tostr( n_new ) );
}

void saveboardriverclusters( const int n_flop, const int n_turn, const int n_new )
{
	cout << "generating b" << n_flop << " - b" << n_turn << " - b" << n_new << " river clusters..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_new, INDEX311_MAX );

	if( n_new == 1 )
	{
		for( int i = 0; i < INDEX311_MAX; i++ )
			output.store( i, 0 );
	} else { // bracket is closed near end of function

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flop ),
			-1, n_flop, INDEX3_MAX, true );

	const PackedBinFile turnclusters(
			BINSFOLDER + "turnb" + tostr( n_flop ) + "-b" + tostr( n_turn ),
			-1, n_turn, INDEX31_MAX, true );

	//the bin information used to characterize the boards
	const PackedBinFile prebins(
			BINSFOLDER + "turn" + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ), 
			-1, CHARCOUNT, INDEX231_MAX, true );
	const PackedBinFile postbins(
			BINSFOLDER + "river" + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ) + '-' + tostr( CHARCOUNT ), 
			-1, CHARCOUNT, INDEX2311_MAX, true);

	//for each flop + turn cluster
	int x = 0;
	for( int myflopcluster = 0; myflopcluster < n_flop; myflopcluster++ )
	{
		for( int myturncluster = 0; myturncluster < n_turn; myturncluster++ )
		{
			Clusterer< int, CHARCOUNT * CHARCOUNT > boards;

			//enumerate through all flops, 
			CardMask cards_flop;
			ENUMERATE_3_CARDS(cards_flop,
			{

				if(x++ % (277 * n_flop * n_turn) == 0) cout << '.' << flush;

				// reject the ones not in the current flop cluster of interest
				if( flopclusters.retrieve( getindex3( cards_flop ) ) != myflopcluster )
					continue;

				//enumerate through all turns
				CardMask card_turn;
				ENUMERATE_1_CARDS_D(card_turn, cards_flop,
				{
					// reject the ones not in the current turn cluster of interest
					if( turnclusters.retrieve( getindex31( cards_flop, card_turn ) ) != myturncluster )
						continue;

					//enumerate through all rivers
					CardMask card_river;
					CardMask cards_used;
					CardMask_OR( cards_used, cards_flop, card_turn );
					ENUMERATE_1_CARDS_D(card_river, cards_used,
					{

						//create a new BoardStruct with its index
						const int64 index = getindex311( cards_flop, card_turn, card_river );
						BoardChar board;

						//characterize it:
						//for each private hand
						CardMask cards_hole;
						CardMask cards_used2;
						CardMask_OR( cards_used2, cards_used, card_river );
						ENUMERATE_2_CARDS_D( cards_hole, cards_used2,
						{
							//lookup that hand's preflop 10-bin and flop 10-bin
							const int pre = prebins.retrieve( getindex231( cards_hole, cards_flop, card_turn ) );
							const int post = postbins.retrieve( getindex2311( cards_hole, cards_flop, card_turn, card_river ) );

							//increment that element of the array
							board.c[ combine( pre, post, CHARCOUNT ) ]++;
						});

						boards.Add( index, board );
					});
				});
			});

			//pass the whole thing to clustering and storing to output
			boards.Cluster( output, n_new, "riverb" + tostr( n_flop ) + "-b" + tostr( n_turn ) + "-b" + tostr( n_new ) + " (doing " + tostr( myflopcluster) + ", " + tostr( myturncluster ) + ")" );
		}
	}
	cout << endl;

	} // end of n_new == 1 case

	//save the file
	cout << output.save( BINSFOLDER + "riverb" + tostr( n_flop ) + "-b" + tostr( n_turn ) + "-b" + tostr( n_new ) );
}

//---------------- B i n   S t o r i n g ------------------

void saveflopbinclusters( int n_flopclusters, int n_flopnew )
{
	cout << "generating b" << n_flopclusters << " - " << n_flopnew << " flop bins via K-Means method..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_flopnew, INDEX23_MAX );

	//the data used to bin the hands
	const FloaterFile flophss( "flopHSS", INDEX23_MAX );
	const FloaterFile flopev( "flopEV", INDEX23_MAX );

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flopclusters ),
			-1, n_flopclusters, INDEX3_MAX, true );

	//for each flop cluster
	int x = 0;
	for( int myflopcluster = 0; myflopcluster < n_flopclusters; myflopcluster++ )
	{
		//the final list of boards we are trying to construct
		Clusterer< double, 2 > hands;

		//enumerate through all flops, 
		CardMask cards_flop;
		ENUMERATE_3_CARDS(cards_flop,
		{

			if(x++ % (277 * n_flopclusters) == 0) cout << '.' << flush;

			// reject the ones not in the current flop cluster of interest
			if( flopclusters.retrieve( getindex3( cards_flop ) ) != myflopcluster )
				continue;

			//enumerate through all private cards
			CardMask cards_hole;
			ENUMERATE_2_CARDS_D(cards_hole, cards_flop,
			{
				//create a new RawHand with its index
				const int64 index = getindex23( cards_hole, cards_flop );
				Hand2Char hand;
				hand.c[ 0 ] = flopev[ index ];
				hand.c[ 1 ] = flophss[ index ] * HSSKMEANSFACTOR;
				hands.Add( index, hand );
			});
		});
		//pass the whole thing to clustering and storing to output
		hands.Cluster( output, n_flopnew, "flopb" + tostr( n_flopclusters ) + "-" + tostr( n_flopnew ) );
	}
	cout << endl;

	//save the file
	cout << output.save( BINSFOLDER + "flopb" + tostr( n_flopclusters ) + "-" + tostr( n_flopnew ) );
}

void saveturnbinclusters( int n_flopclusters, int n_turnclusters, int n_turnnew )
{
	cout << "generating b" << n_flopclusters << " - b" << n_turnclusters << " - " << n_turnnew << " turn bins via K-Means method..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_turnnew, INDEX231_MAX );

	//the data used to bin the hands
	const FloaterFile turnhss( "turnHSS", INDEX24_MAX );
	const FloaterFile turnev( "turnEV", INDEX24_MAX );

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flopclusters ),
			-1, n_flopclusters, INDEX3_MAX, true );

	//the bin information that will have the history
	const PackedBinFile turnclusters(
			BINSFOLDER + "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ),
			-1, n_turnclusters, INDEX31_MAX, true );

	//for each flop + turn cluster
	int x = 0;
	for( int myflopcluster = 0; myflopcluster < n_flopclusters; myflopcluster++ )
	{
		for( int myturncluster = 0; myturncluster < n_turnclusters; myturncluster++ )
		{
			//the final list of boards we are trying to construct
			Clusterer< double, 2 > hands;

			//enumerate through all flops, 
			CardMask cards_flop;
			ENUMERATE_3_CARDS(cards_flop,
			{

				if(x++ % (277 * n_flopclusters * n_turnclusters) == 0) cout << '.' << flush;

				// reject the ones not in the current flop cluster of interest
				if( flopclusters.retrieve( getindex3( cards_flop ) ) != myflopcluster )
					continue;

				//enumerate through all turns
				CardMask card_turn;
				ENUMERATE_1_CARDS_D(card_turn, cards_flop,
				{
					// reject the ones not in the current turn cluster of interest
					if( turnclusters.retrieve( getindex31( cards_flop, card_turn ) ) != myturncluster )
						continue;

					//enumerate through all hole cards
					CardMask cards_hole;
					CardMask cards_used;
					CardMask_OR( cards_used, cards_flop, card_turn );
					ENUMERATE_2_CARDS_D(cards_hole, cards_used,
					{

						//create a new RawHand with its index
						const int64 index231 = getindex231( cards_hole, cards_flop, card_turn );
						CardMask cards_board;
						CardMask_OR( cards_board, cards_flop, card_turn );
						const int64 index24 = getindex2N( cards_hole, cards_board, 4 );
						Hand2Char hand;
						hand.c[ 0 ] = turnev[ index24 ];
						hand.c[ 1 ] = turnhss[ index24 ] * HSSKMEANSFACTOR;
						hands.Add( index231, hand );

					});
				});
			});

			//pass the whole thing to clustering and storing to output
			hands.Cluster( output, n_turnnew, "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ) + "-" + tostr( n_turnnew ) );
		}
	}
	cout << endl;

	//save the file
	cout << output.save( BINSFOLDER + "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ) + "-" + tostr( n_turnnew ) );
}


/*
  for(fcl)
	foreach(flop)
	  if(!fcl) continue
	  foreach(priv)
	    takenote
	binandstore
*/
void saveflopbins( int n_flopclusters, int n_flopnew )
{
	cout << "generating b" << n_flopclusters << " - " << n_flopnew << " flop bins..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_flopnew, INDEX23_MAX );

	//the strength by which we will be binning the hands
	const FloaterFile flophss( "flopHSS", INDEX23_MAX );

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flopclusters ),
			-1, n_flopclusters, INDEX3_MAX, true );

	//for each flop cluster
	int x = 0;
	for( int myflopcluster = 0; myflopcluster < n_flopclusters; myflopcluster++ )
	{
		//the final list of boards we are trying to construct
		Binner hands;

		//enumerate through all flops, 
		CardMask cards_flop;
		ENUMERATE_3_CARDS(cards_flop,
		{

			if(x++ % (277 * n_flopclusters) == 0) cout << '.' << flush;

			// reject the ones not in the current flop cluster of interest
			if( flopclusters.retrieve( getindex3( cards_flop ) ) != myflopcluster )
				continue;

			//enumerate through all private cards
			CardMask cards_hole;
			ENUMERATE_2_CARDS_D(cards_hole, cards_flop,
			{
				//create a new RawHand with its index
				const int64 index = getindex23( cards_hole, cards_flop );
				hands.Add( index, flophss[ index ] );
			});
		});

		//pass the whole thing to my binandstore routine
		hands.Bin( output, n_flopnew, false );
	}
	cout << endl;

	//save the file
	cout << output.save( BINSFOLDER + "flopb" + tostr( n_flopclusters ) + "-" + tostr( n_flopnew ) );
}

/*
  for(fcl)
    for(tcl)
	  foreach(flop)
		if(!fcl) continue
		foreach(turn)
		  if(!tcl) continue
			foreach(priv)
	          takenote
	  binandstore
*/
void saveturnbins( const int n_flopclusters, const int n_turnclusters, const int n_turnnew )
{
	cout << "generating b" << n_flopclusters << " - b" << n_turnclusters << " - " << n_turnnew << " turn bins..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_turnnew, INDEX231_MAX );

	//the data used to bin the hands
	const FloaterFile turnhss( "turnHSS", INDEX24_MAX );

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flopclusters ),
			-1, n_flopclusters, INDEX3_MAX, true );

	const PackedBinFile turnclusters(
			BINSFOLDER + "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ),
			-1, n_turnclusters, INDEX31_MAX, true );

	//for each flop + turn cluster
	int x = 0;
	for( int myflopcluster = 0; myflopcluster < n_flopclusters; myflopcluster++ )
	{
		for( int myturncluster = 0; myturncluster < n_turnclusters; myturncluster++ )
		{
			//the final list of boards we are trying to construct
			Binner hands;

			//enumerate through all flops, 
			CardMask cards_flop;
			ENUMERATE_3_CARDS(cards_flop,
			{

				if(x++ % (277 * n_flopclusters * n_turnclusters) == 0) cout << '.' << flush;

				// reject the ones not in the current flop cluster of interest
				if( flopclusters.retrieve( getindex3( cards_flop ) ) != myflopcluster )
					continue;

				//enumerate through all turns
				CardMask card_turn;
				ENUMERATE_1_CARDS_D(card_turn, cards_flop,
				{
					// reject the ones not in the current turn cluster of interest
					if( turnclusters.retrieve( getindex31( cards_flop, card_turn ) ) != myturncluster )
						continue;

					//enumerate through all hole cards
					CardMask cards_hole;
					CardMask cards_used;
					CardMask_OR( cards_used, cards_flop, card_turn );
					ENUMERATE_2_CARDS_D(cards_hole, cards_used,
					{

						//create a new RawHand with its index
						const int64 index231 = getindex231( cards_hole, cards_flop, card_turn );
						CardMask cards_board;
						CardMask_OR( cards_board, cards_flop, card_turn );
						const int64 index24 = getindex2N( cards_hole, cards_board, 4 );
						hands.Add( index231, turnhss[ index24 ] );

					});
				});
			});

			//pass the whole thing to my binandstore routine
			hands.Bin( output, n_turnnew, false );
		}
	}
	cout << endl;

	//save the file
	cout << output.save( BINSFOLDER + "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ) + "-" + tostr( n_turnnew ) );
}

/*
flopnest

  for(fcl)
	for(privbin)
	  foreach(flop)
	    if(!fcl) continue
	    foreach(priv)
	      if(!privbin) continue
	      takenote
	  binandstore
*/
void nestflopbins( const int n_flopclusters, const int n_flopbins, const int n_flopnew )
{
	cout << "generating b" << n_flopclusters << " - " << n_flopbins << "x" << n_flopnew << " nested flop bins..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_flopbins * n_flopnew, INDEX23_MAX );

	//the data used to bin the hands
	const FloaterFile flopev( "flopEV", INDEX23_MAX );

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flopclusters ),
			-1, n_flopclusters, INDEX3_MAX, true );

	const PackedBinFile flopbins(
			BINSFOLDER + "flopb" + tostr( n_flopclusters ) + "-" + tostr( n_flopbins ),
			-1, n_flopbins, INDEX23_MAX, true );

	//for each flop cluster + flop bin
	int x = 0;
	for( int myflopcluster = 0; myflopcluster < n_flopclusters; myflopcluster++ )
	{
		for( int myflopbin = 0; myflopbin < n_flopbins; myflopbin++ )
		{
			//the final list of boards we are trying to construct
			Binner hands;

			//enumerate through all flops, 
			CardMask cards_flop;
			ENUMERATE_3_CARDS(cards_flop,
			{

				if(x++ % (277 * n_flopclusters * n_flopbins) == 0) cout << '.' << flush;

				// reject the ones not in the current flop cluster of interest
				if( flopclusters.retrieve( getindex3( cards_flop ) ) != myflopcluster )
					continue;

				//enumerate through all hole cards
				CardMask cards_hole;
				ENUMERATE_2_CARDS_D(cards_hole, cards_flop,
				{
					// reject the ones not in the current flop bin of interest
					if( flopbins.retrieve( getindex23( cards_hole, cards_flop ) ) != myflopbin )
						continue;

					//create a new RawHand with its index
					const int64 index = getindex23( cards_hole, cards_flop );
					hands.Add( index, flopev[ index ] );

				});
			});

			//pass the whole thing to my binandstore routine
			hands.Bin( output, n_flopnew, true );

			//I don't know which ones I just stored. A super hack is to only 
			// re-set the ones that have been stored exactly once.
			for( int64 i = 0; i < INDEX23_MAX; i++ )
				if( output.isstored( i ) == 1 )
					output.store( i, combine( myflopbin, output.retrieve( i ), n_flopnew ) );
		}
	}
	cout << endl;

	//save the file
	cout << output.save( BINSFOLDER + "flopb" + tostr( n_flopclusters ) + "-" + tostr( n_flopbins ) + "x" + tostr( n_flopnew ) );
}

/* this is the correct algorithm. I think what i do below is exactly the 
   same. or better in the places where it differs

  for(fcl)
    for(tcl)
	  for(rcl)
	    foreach(flop)
		  if(!fcl) continue
		  foreach(turn)
		    if(!tcl) continue
		    foreach(river)
		      if(!rcl) continue
			  foreach(priv)
	            takenote
	    binandstore
		*/
/* this is the correct algorithm. I think what i do below is exactly the 
   same. or better in the places where it differs

  for(fcl)
    for(tcl)
	  for(rcl)
	    foreach(flop)
		  if(!fcl) continue
		  foreach(turn)
		    if(!tcl) continue
		    foreach(river)
		      if(!rcl) continue
			  foreach(priv)
	            takenote
	    binandstore
		*/
void saveriverbins( const int n_flopclusters, const int n_turnclusters, const int n_riverclusters, const int n_rivernew )
{
#if 0
// INDEX25 easy way, no memory, runs in seconds. choose divisions in riverEV numbers for 
// river bin divisions, possible since all hole cards are in computation.

	cout << "generating b" << n_flopclusters << " - b" << n_turnclusters << " - b" << n_riverclusters << " - " << n_rivernew << " river bins..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_rivernew, INDEX25_MAX );

	//the data used to bin the hands
	const FloaterFile riverev( "riverEV", INDEX25_MAX );

	for( int index25 = 0; index25 < INDEX25_MAX; index25++ )
	{
		const floater value = riverev[ index25 ];
		int bin = static_cast< int >( value * n_rivernew );
		if( bin == n_rivernew )
			bin--;
		output.store( index25, bin );
	}

	//save the file
	cout << output.save( BINSFOLDER + "riverb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ) + "-b" + tostr( n_riverclusters ) + "-" + tostr( n_rivernew ) );

#endif 

// ...................................

#if 1
// hand way, takes four days to run. Idea is to build several buckets of hands based on
// their hand type, and then sort and bin within each bucket. Uses INDEX2311.

	cout << "generating b" << n_flopclusters << " - b" << n_turnclusters << " - b" << n_riverclusters << " - " << n_rivernew << " river bins..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_rivernew, INDEX2311_MAX );

	//the data used to bin the hands
	const FloaterFile riverev( "riverEV", INDEX25_MAX );

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flopclusters ),
			-1, n_flopclusters, INDEX3_MAX, true );

	const PackedBinFile turnclusters(
			BINSFOLDER + "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ),
			-1, n_turnclusters, INDEX31_MAX, true );

	const PackedBinFile riverclusters(
			BINSFOLDER + "riverb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ) + "-b" + tostr( n_riverclusters ),
			-1, n_riverclusters, INDEX311_MAX, true );

	//for each flop + turn + river cluster
	int x = 0;
	for( int myflopcluster = 0; myflopcluster < n_flopclusters; myflopcluster++ )
	{
		for( int myturncluster = 0; myturncluster < n_turnclusters; myturncluster++ )
		{
			for( int myrivercluster = 0; myrivercluster < n_riverclusters; myrivercluster++ )
			{
				//the final list of boards we are trying to construct
				vector< Binner > hands( RIVER_HANDTYPE_MAX );

				//enumerate through all flops, 
				CardMask cards_flop;
				ENUMERATE_3_CARDS(cards_flop,
				{

					if(x++ % (277 * n_flopclusters * n_turnclusters * n_riverclusters) == 0) cout << '.' << flush;

					// reject the ones not in the current flop cluster of interest
					if( flopclusters.retrieve( getindex3( cards_flop ) ) != myflopcluster )
						continue;

					//enumerate through all turns
					CardMask card_turn;
					ENUMERATE_1_CARDS_D(card_turn, cards_flop,
					{
						// reject the ones not in the current turn cluster of interest
						if( turnclusters.retrieve( getindex31( cards_flop, card_turn ) ) != myturncluster )
							continue;

						//enumerate through all rivers
						CardMask card_river;
						CardMask cards_used;
						CardMask_OR( cards_used, cards_flop, card_turn );
						ENUMERATE_1_CARDS_D(card_river, cards_used,
						{
							// reject the ones not in the current turn cluster of interest
							if( riverclusters.retrieve( getindex311( cards_flop, card_turn, card_river ) ) != myrivercluster )
								continue;

							//enumerate through all hole cards
							CardMask cards_hole;
							CardMask cards_used2;
							CardMask_OR( cards_used2, cards_used, card_river );
							ENUMERATE_2_CARDS_D(cards_hole, cards_used2,
							{

								//create a new RawHand with its index
								const int64 index2311 = getindex2311( cards_hole, cards_flop, card_turn, card_river );
								CardMask cards_board;
								CardMask_OR( cards_board, cards_flop, card_turn );
								CardMask_OR( cards_board, cards_board, card_river );
								const int64 index25 = getindex2N( cards_hole, cards_board, 5 );

								CardMask full_hand;
								CardMask_OR( full_hand, cards_board, cards_hole );
								HandVal handvalue = Hand_EVAL_N( full_hand, 7 );
								int handtype = HandVal_HANDTYPE( handvalue );
								if( handtype < 0 || handtype >= 9 )
									REPORT( "invalid handtype return from pokereval" );
								if( handtype >= RIVER_HANDTYPE_MAX )
									handtype = RIVER_HANDTYPE_MAX - 1;
								// handtype values are:
								// StdRules_HandType_NOPAIR    0
								// StdRules_HandType_ONEPAIR   1
								// StdRules_HandType_TWOPAIR   2
								// StdRules_HandType_TRIPS     3
								// StdRules_HandType_STRAIGHT  4
								// StdRules_HandType_FLUSH     5
								// StdRules_HandType_FULLHOUSE 6
								// StdRules_HandType_QUADS     7
								// StdRules_HandType_STFLUSH   8

								hands[ handtype ].Add( index2311, riverev[ index25 ] );
							});
						});
					});
				});

				//figure out how many bins for each handtype

				double totalweight = 0;
				//I tested this parameter at 0.75 and 1.0 as well, this worked best
				// 0.75 beat 1.0 by about 1.2 mb/h
				// 0.60 beat 0.75 by about 0.06 mb/h
				const double powerfactor = 0.6;
				vector< double > weights( RIVER_HANDTYPE_MAX );
				for( int i = 0; i < RIVER_HANDTYPE_MAX; i++ )
					totalweight += weights[ i ] = pow( static_cast< double >( hands[ i ].GetTotalSize( ) ), powerfactor );

				vector< int > numbins( RIVER_HANDTYPE_MAX );
				bitset< RIVER_HANDTYPE_MAX > bincapped( 0 );
				int binsleft = n_rivernew;
				int totalbins = 0;
				int maxbins = 0;
				int maxbinsindex = -1;
				while( true )
				{
					for( int i = 0; i < RIVER_HANDTYPE_MAX; i++ )
						if( ! bincapped[ i ] )
						{
							numbins[ i ] = mymax( 1L, boost::math::lround( binsleft * weights[ i ] / totalweight ) );
							if( numbins[ i ] >= maxbins )
							{
								maxbins = numbins[ i ];
								maxbinsindex = i;
							}
						}

					bool didcapping = false;
					for( int i = 0; i < RIVER_HANDTYPE_MAX; i++ )
						if( numbins[ i ] > hands[ i ].GetCompressedSize( ) )
						{
							numbins[ i ] = hands[ i ].GetCompressedSize( );
							binsleft -= numbins[ i ];
							totalweight -= weights[ i ];
							didcapping = true;
							bincapped[ i ] = true;
						}
					if( ! didcapping )
						break;
					if( bincapped.all( ) )
						goto skipadjustment;
				}

				for( int i = 0; i < RIVER_HANDTYPE_MAX; i++ )
					totalbins += numbins[ i ];
				if( totalbins > n_rivernew - 4 )
					numbins[ maxbinsindex ] -= totalbins - n_rivernew;
skipadjustment:

				//print info

				cerr << endl << "For b" << myflopcluster << " - b" << myturncluster << " - b" << myrivercluster << endl;
				if( totalbins > n_rivernew - 4 )
					cerr << "handtype " << maxbinsindex << " was changed by " << n_rivernew - totalbins << " to make " << n_rivernew << " total bins" << endl;
				for( int i = 0; i < RIVER_HANDTYPE_MAX; i++ )
					cerr << i << ": weight=" << weights[ i ] << " compsize=" << hands[ i ].GetCompressedSize( ) << " numbins=" << numbins[ i ] << endl;

				//finally calculate the bins

				int binoffsetnumber = 0;
				for( int i = 0; i < RIVER_HANDTYPE_MAX; i++ )
				{
					hands[ i ].Bin( output, numbins[ i ], true );
					if( numbins[ i ] )
						for( int64 j = 0; j < INDEX2311_MAX; j++ )
							if( output.isstored( j ) == 1 )
							{
								const int mynum = output.retrieve( j );
								if( mynum < 0 || mynum >= numbins[ i ] )
									REPORT( "found a " + tostr( mynum ) + " stored once." );
								output.store( j, output.retrieve( j ) + binoffsetnumber );
							}
					binoffsetnumber += numbins[ i ];
				}
			}
		}
	}
	cout << endl;

	//save the file
	cout << output.save( BINSFOLDER + "riverb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ) + "-b" + tostr( n_riverclusters ) + "-" + tostr( n_rivernew ) );

#endif
}	            

/*

turnnest

  for(fcl)
	for(tcl)
	  for(privbin)
	    foreach(flop)
	      if(!fcl) continue
		  foreach(turn)
	        if(!tcl) continue
	        foreach(priv)
	          if(!privbin) continue
	          takenote
	    binandstore
*/
void nestturnbins( const int n_flopclusters, const int n_turnclusters, const int n_turnbins, const int n_turnnew )
{
	cout << "generating b" << n_flopclusters << " - b" << n_turnclusters << " - " << n_turnbins << "x" << n_turnnew << " nested turn bins..." << endl;

	//the bin file that will receive the output
	PackedBinFile output( n_turnbins * n_turnnew, INDEX231_MAX );

	//the data used to bin the hands
	const FloaterFile turnev( "turnEV", INDEX24_MAX );

	//the bin information that will have the history
	const PackedBinFile flopclusters(
			BINSFOLDER + "flopb" + tostr( n_flopclusters ),
			-1, n_flopclusters, INDEX3_MAX, true );

	const PackedBinFile turnclusters(
			BINSFOLDER + "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ),
			-1, n_turnclusters, INDEX31_MAX, true );

	const PackedBinFile turnbins(
			BINSFOLDER + "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ) + "-" + tostr( n_turnbins ),
			-1, n_turnbins, INDEX231_MAX, true );

	//for each flop + turn + river cluster

	//the final list of boards we are trying to construct
	vector< Binner > hands( n_flopclusters * n_turnclusters * n_turnbins );

	int x = 0;

	//enumerate through all flops, 
	CardMask cards_flop;
	ENUMERATE_3_CARDS(cards_flop,
	{

		if(x++ % (277) == 0) cout << '.' << flush;

		// reject the ones not in the current flop cluster of interest
		int myflopcluster = flopclusters.retrieve( getindex3( cards_flop ) );

		//enumerate through all turns
		CardMask card_turn;
		ENUMERATE_1_CARDS_D(card_turn, cards_flop,
		{
			// reject the ones not in the current turn cluster of interest
			int myturncluster = turnclusters.retrieve( getindex31( cards_flop, card_turn ) );

			//enumerate through all rivers
			CardMask cards_hole;
			CardMask cards_used;
			CardMask_OR( cards_used, cards_flop, card_turn );
			ENUMERATE_2_CARDS_D(cards_hole, cards_used,
			{
				// reject the ones not in the current turn cluster of interest
				int myturnbin = turnbins.retrieve( getindex231( cards_hole, cards_flop, card_turn ) );

				//create a new RawHand with its index
				const int64 index231 = getindex231( cards_hole, cards_flop, card_turn );
				CardMask cards_board;
				CardMask_OR( cards_board, cards_flop, card_turn );
				const int64 index24 = getindex2N( cards_hole, cards_board, 4 );
				hands[ combine( myflopcluster, myturncluster, n_turnclusters, myturnbin, n_turnbins ) ].Add( index231, turnev[ index24 ] );

			});
		});
	});

	//pass the whole thing to my binandstore routine
	for( int k = 0; k < n_flopclusters * n_turnclusters * n_turnbins; k++ )
	{
		hands[ k ].Bin( output, n_turnnew, true );

		//I don't know which ones I just stored. A super hack is to only 
		// re-set the ones that have been stored exactly once.
		for( int64 i = 0; i < INDEX231_MAX; i++ )
			if( output.isstored( i ) == 1 )
				output.store( i, combine( k % n_turnbins, output.retrieve( i ), n_turnnew ) );
	}

	cout << endl;

	//save the file
	cout << output.save( BINSFOLDER + "turnb" + tostr( n_flopclusters ) + "-b" + tostr( n_turnclusters ) + "-" + tostr( n_turnbins ) + "x" + tostr( n_turnnew ) );
}


//------------------ H i s t o r y   B i n s --------------------------


//This is an end result function that could be called by main(). 
//it depends on the file bins/turnHSS, as used by readturnHSS().
void dopflopbins(int n_bins)
{
	cout << "generating " << n_bins << " preflop bins... (no status bar)" << endl;
	PackedBinFile packedbins(n_bins, INDEX2_MAX);
	if(n_bins == INDEX2_MAX) // that's all of 'em!
	{
		cout << "Bypassing calculatebins, setting preflop bin to it's index..." << endl;
		for(int i=0; i<INDEX2_MAX; i++)
			packedbins.store(i, i);
	}
	else
	{
		const FloaterFile hss("preflopHSS", INDEX2_MAX);
		calculatebins(0, n_bins, hss, packedbins);
	}
	cout << packedbins.save(BINSFOLDER+"preflop"+tostring(n_bins));
}

void doflophistorybins(string pflopname, int n_newbins)
{
	cout << "generating " << pflopname << " - " << n_newbins << " flop history bins..." << endl;

	const int n_pflop = getnumbins( pflopname );

	const PackedBinFile pflopbins(BINSFOLDER+"preflop"+pflopname, -1, n_pflop, INDEX2_MAX, true);
	const FloaterFile flophss("flopHSS", INDEX23_MAX);
	PackedBinFile binfile(n_newbins, INDEX23_MAX);

	const float expected = 25989600.0f/n_pflop; //52C2 * 50C3 = 25989600
	vector<RawHand> hand_list((int64)(hand_list_multiplier*expected)); 

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

		if(i < (1/chunky_bin_tolerance)*expected || i > chunky_bin_tolerance*expected)
			cout << "\nWarning: found "+tostring(i)+" hands ("+tostring(100.0*i/expected)
					+"% of expected) for preflop bin "+tostring(mypflopbin) << endl;
		if(i-1>=hand_list.size())
			REPORT("...and that overflowed hand_list. make fewer bins for less granularity or make hand_list bigger.");

		binandstore(hand_list, binfile, n_newbins, i);
	}

	cout << endl;
	cout << binfile.save(BINSFOLDER+"flop"+pflopname+'-'+tostring(n_newbins));
}

/*** comments on nesting
  Easier than expected. I adapted the turn binning method to also nest flop bins, and
  the river binning method to also nest turn bins. The process to create NxM nested bins,
  is to first create N regular bins that are sorted by HSS, then for each of the N bins,
  to further sort into M bins based on EV. Finally, the two bin numbers must be combined. I
  correctly used the N-number as the major and the M-number as the minor in the combine.
  Clearly the process for creating flop nested bins is the same as creating turn regular
  bins, except I don't iterate through the possible turn cards.
***/

//just so i don't die of naming
void doturnhistorybins(string, string, int, bool = false);
void nestflophistorybins(string pflopname, string flopname, int n_newbins)
{ doturnhistorybins(pflopname, flopname, n_newbins, true); }

void doturnhistorybins(string pflopname, string flopname, int n_newbins, bool nestingflop)
{
	if( nestingflop )
	{
		if( flopname != tostr( getnumbins( flopname ) ) ) REPORT( "bad flopname" );
		cout << "generating " << pflopname << " - " << flopname << "x" << n_newbins << " flop nested history bins..." << endl;
	}
	else
		cout << "generating " << pflopname << " - " << flopname << " - " << n_newbins << " turn history bins..." << endl;

	const int n_pflop = getnumbins( pflopname );
	const int n_flop = getnumbins( flopname );

	const PackedBinFile pflopbins(BINSFOLDER+"preflop"+pflopname, -1, n_pflop, INDEX2_MAX, true);
	const PackedBinFile flopbins(BINSFOLDER+"flop"+pflopname+'-'+flopname, -1, n_flop, INDEX23_MAX, true);
	const FloaterFile binmeasure(
			( nestingflop ? "flopEV" : "turnHSS" ),
			( nestingflop ? INDEX23_MAX : INDEX24_MAX ) );
	PackedBinFile binfile(
			( nestingflop ? n_flop * n_newbins : n_newbins ), 
			( nestingflop ? INDEX23_MAX : INDEX231_MAX ) );

	const float expected = (nestingflop?25989600.0f:1221511200.0f)/n_pflop/n_flop;
	vector<RawHand> hand_list((int64)(hand_list_multiplier*expected));

	//build hand lists and bin
	int64 x=0;
	for(int mypflopbin=0; mypflopbin<n_pflop; mypflopbin++)
	{
		for(int myflopbin=0; myflopbin<n_flop; myflopbin++)
		{
			uint64 i=0;

			CardMask cards_pflop, cards_flop;
			ENUMERATE_2_CARDS(cards_pflop,
			{
				if(x++ % (17*n_pflop*n_flop) == 0) cout << '.' << flush;

				if(pflopbins.retrieve(getindex2(cards_pflop)) != mypflopbin)
					continue;

				ENUMERATE_3_CARDS_D(cards_flop, cards_pflop,
				{
					if(flopbins.retrieve(getindex23(cards_pflop, cards_flop)) != myflopbin)
						continue;

					if(nestingflop)
					{
						int64 index = getindex23(cards_pflop, cards_flop);
						hand_list[i++] = RawHand(index, binmeasure[index]);
					}
					else
					{
						CardMask dead;
						CardMask card_turn;
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
							hand_list[i++] = RawHand(index, binmeasure[getindex2N(cards_pflop, board, 4)]);
						});
					}
				});
			});

			if(i < (1/chunky_bin_tolerance)*expected || i > chunky_bin_tolerance*expected)
				cout << "\nWarning: found "+tostring(i)+" hands ("+tostring(100.0*i/expected)
						+"% of expected) for preflop/flop bin "+tostring(mypflopbin)
						+'/'+tostring(myflopbin) << endl;
			if(i-1>=hand_list.size())
				REPORT("...and that overflowed hand_list. make fewer bins for less granularity or make hand_list bigger.");

			binandstore(hand_list, binfile, n_newbins, i);
			if( nestingflop )
			{
				//I don't know which ones I just stored. A super hack is to only 
				// re-set the ones that have been stored exactly once.
				for( int64 i = 0; i < INDEX23_MAX; i++ )
					if( binfile.isstored( i ) == 1 )
						binfile.store( i, combine( myflopbin, binfile.retrieve( i ), n_newbins ) );
			}
		}
	}
	
	cout << endl;
	if( nestingflop )
		cout << binfile.save(BINSFOLDER+"flop"+pflopname+'-'+flopname+'x'+tostring(n_newbins));
	else
		cout << binfile.save(BINSFOLDER+"turn"+pflopname+'-'+flopname+'-'+tostring(n_newbins));
}

//just so i don't die of naming
void doriverhistorybins(string, string, string, int, bool = false);
void nestturnhistorybins(string pflopname, string flopname, string turnname, int n_newbins)
{ doriverhistorybins(pflopname, flopname, turnname, n_newbins, true); }

void doriverhistorybins(string pflopname, string flopname, string turnname, int n_newbins, bool nestingturn)
{
	if( nestingturn )
	{
		if( turnname != tostr( getnumbins( turnname ) ) ) REPORT( "bad turnname" );
		cout << "generating " << pflopname << " - " << flopname << " - " << turnname << "x" << n_newbins << " turn nested history bins..." << endl;
	}
	else
		cout << "generating " << pflopname << " - " << flopname << " - " << turnname << " - " << n_newbins << " river history bins..." << endl;

	const int n_pflop = getnumbins( pflopname );
	const int n_flop = getnumbins( flopname );
	const int n_turn = getnumbins( turnname );

	const PackedBinFile pflopbins(BINSFOLDER+"preflop"+pflopname, -1, n_pflop, INDEX2_MAX, true);
	const PackedBinFile flopbins(BINSFOLDER+"flop"+pflopname+'-'+flopname, -1, n_flop, INDEX23_MAX, true);
	const PackedBinFile turnbins(BINSFOLDER+"turn"+pflopname+'-'+flopname+'-'+turnname, -1, n_turn, INDEX231_MAX, true);
	const FloaterFile binmeasure(
			( nestingturn ? "turnEV" : "riverEV" ),
			( nestingturn ? INDEX24_MAX : INDEX25_MAX ) );
	PackedBinFile binfile(
			( nestingturn ? n_turn * n_newbins : n_newbins ), 
			( nestingturn ? INDEX231_MAX : INDEX2311_MAX ) );

	const float expected = (nestingturn?1221511200.0f:56189515200.0f)/n_pflop/n_flop/n_turn;
	vector<RawHand> hand_list((int64)(hand_list_multiplier*expected));

	//build hand lists and bin
	int64 x=0;
	for(int mypflopbin=0; mypflopbin<n_pflop; mypflopbin++)
	{
		for(int myflopbin=0; myflopbin<n_flop; myflopbin++)
		{
			for(int myturnbin=0; myturnbin<n_turn; myturnbin++)
			{
				uint64 i=0;

				CardMask cards_pflop, cards_flop, card_turn;
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

							if( nestingturn )
							{
								CardMask board;
								CardMask_OR(board, cards_flop, card_turn);
								int64 index = getindex231(cards_pflop, cards_flop, card_turn);
								hand_list[i++] = RawHand(index, binmeasure[getindex2N(cards_pflop, board, 4)]);
							}
							else
							{
								CardMask dead2;
								CardMask card_river;
								CardMask_OR(dead2, dead, card_turn);
								ENUMERATE_1_CARDS_D(card_river, dead2,
								{
									CardMask board;
									CardMask_OR(board, cards_flop, card_turn);
									CardMask_OR(board, board, card_river);
									int64 index = getindex2311(cards_pflop, cards_flop, card_turn, card_river);
									hand_list[i++] = RawHand(index, binmeasure[getindex2N(cards_pflop, board, 5)]);
								});
							}
						});
					});
				});
			
				if(i < (1/chunky_bin_tolerance)*expected || i > chunky_bin_tolerance*expected)
					cout << "\nWarning: found "+tostring(i)+" hands ("+tostring(100.0*i/expected)
							+"% of expected) for preflop/flop/turn bin "+tostring(mypflopbin)
							+'/'+tostring(myflopbin)+'/'+tostring(myturnbin) << endl;
				if(i-1>=hand_list.size())
					REPORT("...and that overflowed hand_list. make fewer bins for less granularity or make hand_list bigger.");

				binandstore(hand_list, binfile, n_newbins, i, nestingturn | RIVERBUNCHHISTBINS); //true = DO bunch similar hands!
				if( nestingturn )
				{
					//I don't know which ones I just stored. A super hack is to only 
					// re-set the ones that have been stored exactly once.
					for( int64 i = 0; i < INDEX231_MAX; i++ )
						if( binfile.isstored( i ) == 1 )
							binfile.store( i, combine( myturnbin, binfile.retrieve( i ), n_newbins ) );
				}
			}
		}
	}

	cout << endl;
	if( nestingturn )
		cout << binfile.save(BINSFOLDER+"turn"+pflopname+'-'+flopname+'-'+turnname+'x'+tostring(n_newbins));
	else
		cout << binfile.save(BINSFOLDER+"river"+pflopname+'-'+flopname+'-'+turnname+'-'+tostring(n_newbins));
}

void makehistbin(int a, string b, string c, int d)
{
	size_t bx = b.find( "x" );
	size_t cx = c.find( "x" );
	int b1 = fromstr< int >( b.substr( 0, bx ) );
	int b2 = ( bx == string::npos ? 0 : fromstr< int >( b.substr( bx + 1 ) ) );
	int c1 = fromstr< int >( c.substr( 0, cx ) );
	int c2 = ( cx == string::npos ? 0 : fromstr< int >( c.substr( cx + 1 ) ) );

	dopflopbins( a );

	doflophistorybins( tostr( a ), b1 );

	if( b2 )
		nestflophistorybins( tostr( a ), tostr( b1 ), b2 );

	doturnhistorybins( tostr( a ), b, c1 );

	if( c2 )
		nestturnhistorybins( tostr( a ), b, tostr( c1 ), c2 );

	doriverhistorybins( tostr( a ), b, c, d );
}

void makehistbin(int pf, int ftr)
{
	makehistbin( pf, tostr( ftr ), tostr( ftr ), ftr );
}


void makeboardbins( int bf, int bt, int br, string f, string t, int r, string which )
{
	size_t fx = f.find( "x" );
	size_t tx = t.find( "x" );
	int f1 = fromstr< int >( f.substr( 0, fx ) );
	int f2 = ( fx == string::npos ? -1 : fromstr< int >( f.substr( fx + 1 ) ) );
	int t1 = fromstr< int >( t.substr( 0, tx ) );
	int t2 = ( tx == string::npos ? -1 : fromstr< int >( t.substr( tx + 1 ) ) );
	
	for( int i = 0; i < 6; i++ )
		if( which.length( ) != 6 || ( which[ i ] != '0' && which[ i ] != '1' ) )
			throw Exception( "The which string must be six zeros and ones, but is '" + which + "'" );

	if( which[ 0 ] == '1' )
		saveboardflopclusters( bf );

	if( which[ 1 ] == '1' )
		saveboardturnclusters( bf, bt );

	if( which[ 2 ] == '1' )
		saveboardriverclusters( bf, bt, br );

	if( which[ 3 ] == '1' )
	{
		if( f2 == 0 )
			saveflopbinclusters( bf, f1 );
		else
		{
			saveflopbins( bf, f1 );
			if( f2 > 0 )
				nestflopbins( bf, f1, f2 );
		}
	}

	if( which[ 4 ] == '1' )
	{
		if( t2 == 0 )
			saveturnbinclusters( bf, bt, t1 );
		else
		{
			saveturnbins( bf, bt, t1 );
			if( t2 > 0 )
				nestturnbins( bf, bt, t1, t2 );
		}
	}

	if( which[ 5 ] == '1' )
		saveriverbins( bf, bt, br, r );
}

	/*

Generators of HSS and EV values:

void saveriverEV(); //No dependencies.				 Generates "<floaterdir>/riverEV".
void saveturnHSS(); //Depends on "<floaterdir>/riverEV". Generates "<floaterdir>/turnHSS".
void saveturnEV(); //Depends on "<floaterdir>/riverEV". Generates "<floaterdir>/turnEV".
void saveflopHSS(); //Depends on "<floaterdir>/turnHSS". Generates "<floaterdir>/flopHSS".
void saveflopEV(); //Depends on "<floaterdir>/turnEV". Generates "<floaterdir>/flopEV".
void savepreflopHSS(); //Deps on "<floaterdir>/flopHSS". Generates "<floaterdir>/preflopHSS".


My original idea of bin generators:

void saveboardflopclusters( int n_new )
void saveboardturnclusters( const int n_flop, const int n_new )
void saveboardriverclusters( const int n_flop, const int n_turn, const int n_new )
void saveflopbins( int n_flopclusters, int n_flopnew )
void saveturnbins( const int n_flopclusters, const int n_turnclusters, const int n_turnnew )
void nestflopbins( const int n_flopclusters, const int n_flopbins, const int n_flopnew )
void saveriverbins( const int n_flopclusters, const int n_turnclusters, const int n_riverclusters, const int n_rivernew )
void nestturnbins( const int n_flopclusters, const int n_turnclusters, const int n_turnbins, const int n_turnnew )


New (history) bin generators, each generates one new output, 
depends on all previous, starting with the preflop (no deps besides preflopHSS):

void dopflopbins(int n_bins);
void doflophistorybins(string pflopname, int n_newbins);
void doturnhistorybins(string pflopname, string flopname, int n_newbins);
void nestflophistorybins(string pflopname, string flopname, int n_newbins);
void doturnhistorybins(string pflopname, string flopname, int n_newbins, bool nestingflop);
void doriverhistorybins(string pflopname, string flopname, string turnname, int n_newbins);
void nestturnhistorybins(string pflopname, string flopname, string turnname, int n_newbins);
void doriverhistorybins(string pflopname, string flopname, string turnname, int n_newbins, bool nestingturn);

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

	if((argc>=2) && (strcmp("histbin", argv[1]) == 0 || strcmp("boardbins", argv[1]) == 0 || strcmp("ramtest", argv[1])==0))
	{
		cout << "Current data type: Using " << FloaterFile::gettypename() << endl;
		cout << "fpcompare uses " << EPSILON << "." << endl;
		cout << "hand_list_multiplier will store " << hand_list_multiplier << " times more entries than expected (increases chunky tolerance too)." << endl;
		cout << "river bunches hands by HSS using fpcompare? " 
			<< (RIVERBUNCHHISTBINS ? "Yes." : "No.") << endl;
		cout << "(everything bunches hands by HSS using fpcompare, with possible exception of river.)" << endl;
		cout << "Actually doing K-Means for board bins: ";
		if( ACTUALLYDOKMEANS )
			cout << " Yes, using Hartigan-Wong with " << KMEANSINITMETHOD << " as initialization.";
		else
			cout << " No, using only index groupings.";
		cout << endl << endl;
	}

	//these checks must be before other histbin checks
	//generate one round of histbins
	if(argc == 4 && strcmp("histbin", argv[1])==0 && strcmp("preflop", argv[2])==0)
		dopflopbins(atoi(argv[3]));
	else if(argc == 5 && strcmp("histbin", argv[1])==0 && strcmp("flop", argv[2])==0)
		doflophistorybins(argv[3], atoi(argv[4]));
	else if(argc == 6 && strcmp("histbin", argv[1])==0 && strcmp("flopnest", argv[2])==0)
		nestflophistorybins(argv[3], argv[4], atoi(argv[5]));
	else if(argc == 6 && strcmp("histbin", argv[1])==0 && strcmp("turn", argv[2])==0)
		doturnhistorybins(argv[3], argv[4], atoi(argv[5]));
	else if(argc == 7 && strcmp("histbin", argv[1])==0 && strcmp("turnnest", argv[2])==0)
		nestturnhistorybins(argv[3], argv[4], argv[5], atoi(argv[6]));
	else if(argc == 7 && strcmp("histbin", argv[1])==0 && strcmp("river", argv[2])==0)
		doriverhistorybins(argv[3], argv[4], argv[5], atoi(argv[6]));

	//generate all rounds of histbins, different ways to specify number of bins
	else if(argc == 6 && strcmp("histbin", argv[1])==0)
		makehistbin(atoi(argv[2]), argv[3], argv[4], atoi(argv[5]));
	else if(argc == 4 && strcmp("histbin", argv[1])==0)
		makehistbin(atoi(argv[2]), atoi(argv[3]));
	else if(argc == 3 && strcmp("histbin", argv[1])==0)
		makehistbin(atoi(argv[2]), atoi(argv[2]));

	//generate one round of boardbins
	else if(argc == 9 && strcmp("boardbins", argv[1])==0)
		makeboardbins( 
				atoi( argv[ 2 ] ),
				atoi( argv[ 3 ] ),
				atoi( argv[ 4 ] ),
				argv[ 5 ],
				argv[ 6 ],
				atoi( argv[ 7 ] ),
			    argv[ 8 ] );

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
			case 3: indexmax = INDEX3_MAX; break;
			case 31: indexmax = INDEX31_MAX; break;
			case 311: indexmax = INDEX311_MAX; break;
			default: 
			   cout << "Usage: indextype must be 23, 24, 25, 231, 2311, 3, 31, or 311" << endl; 
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

		cout << "hand_list_multiplier is " << hand_list_multiplier << endl;

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

		int64 rawhandlength = hand_list_multiplier*56189515200.0f/n_pflop/n_flop/n_turn;
		total += temp = sizeof(RawHand)*rawhandlength;
		cout << "RawHand(" << space(sizeof(RawHand)) << ") list takes " << space(temp) << endl;

		total += temp = sizeof(CompressedHand)*(rawhandlength/hand_list_multiplier/7);
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
		cout << "Create [nested] history bins:  " << argv[0] << " histbin n_pflop flopname turnname n_river" << endl;
		cout << "Create history bins:           " << argv[0] << " histbin n_pflop n_flop-turn-river" << endl;
		cout << "Create history bins:           " << argv[0] << " histbin n_bins" << endl;
		cout << "Create one round of hist bins: " << argv[0] << " histbin (preflop | flop | flopnest | turn | turnnest | river) n_bins ... n_bins" << endl;
		cout << "Create old-style board bins:   " << argv[0] << " boardbins n_flopclusters n_turnclusters n_riverclusters flopbins turnbins n_riverbins whichstr" << endl;
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

