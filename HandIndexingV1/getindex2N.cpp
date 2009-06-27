#include "stdafx.h"
#include "getindex2N.h"
#include "constants.h"
#include "../utility.h"

struct suitrep //represents cards in a certain suit by number of cards and their ranks
{
	int suit; //needs to be saved, only for purposes of 231 and 2311 indexing
	int ranki; //colexi rank index
	int n_cards;
	int my[2]; //index of my two cards, -1 if not
};

//This is needed to make sort work, as sort uses the less than operator. So here you go!
inline bool operator < (const suitrep &a, const suitrep &b)  
{
	if (a.n_cards != b.n_cards)
		return a.n_cards < b.n_cards;
	else
		return a.ranki < b.ranki;
}  


//verified up to n==30 to add up to 2^n
//will return 0 when k>n or when n==0
inline int nchoosek(const int n, const int k)
{
	if(!(n>=0 && k>=0)) REPORT("invalid n choose k");
	int ans=1;
	for(int i=0; i<k; i++)
	{
		ans*= n-i;
		ans/= i+1; //must be separate due to int arithmetic
	}
	return ans;
}

//returns unique index for an array of k DECREASING numbers - x[0] BIG
//result in range 0..((n choose k)-1)
//where the range of x[i] is 0..(n-1)
//http://www.cs.cmu.edu/~sandholm/3-player%20jam-fold.AAMAS08.pdf
inline int colexi(const int x[], const int &k)
{
	int colex=0;
	for(int i=1; i<=k; i++)
		colex+= nchoosek(x[k-i], i);
	assert(0<=colex && (k==0 || colex<=nchoosek(1+*std::max_element(x,x+k),k)-1));
	return colex;
}

//returns unique index for an array of 2 DECREASING numbers, allowing dups (a is BIG)
//result in range 0=mcolexi(0,0)..((n choose 2)+n-1)=mcolexi(n-1,n-1)
//where the range of x[i] is 0..(n-1)
//http://www.cs.cmu.edu/~sandholm/3-player%20jam-fold.AAMAS08.pdf
inline int mcolexi(const int &a, const int &b)
{
	return nchoosek(b,1)+nchoosek(a+1,2);
}

//returns unique index for an array of 3 DECREASING numbers, allowing dups (a is BIG, c is SMALL)
//result in range 0..mcolexi(n-1,n-1,n-1)
//where the range of x[i] is 0..(n-1)
//http://www.cs.cmu.edu/~sandholm/3-player%20jam-fold.AAMAS08.pdf
inline int mcolexi(const int &a, const int &b, const int &c)
{
	return nchoosek(c,1)+nchoosek(b+1,2)+nchoosek(a+2,3);
}


//easy way to identify the number-of-cards-in-each-suit case.
#define SUITI(a, b, c, d) ((a) + (b<<4) + (c<<8) + (d<<12))
#define MCOL2(i,j) (mcolexi(n[i].ranki, n[j].ranki))
#define MCOL3(i,j,k) (mcolexi(n[i].ranki, n[j].ranki, n[k].ranki))

inline int getindex7(const suitrep n[])
{
	switch(SUITI(n[3].n_cards, n[2].n_cards, n[1].n_cards, n[0].n_cards)) //largest first
	{
		//FIRST, THE SEVEN CARD HANDS
		//four suited
	case SUITI(4,1,1,1):
		return combine(n[3].ranki, MCOL3(2,1,0), M31);

	case SUITI(3,2,1,1):
		return n[3].ranki*N2*M21 + n[2].ranki*M21 + mcolexi(n[1].ranki, n[0].ranki) \
			+ N4*M31;

	case SUITI(2,2,2,1):
		return mcolexi(n[3].ranki, n[2].ranki, n[1].ranki)*N1 + n[0].ranki \
			+ N4*M31 + N3*N2*M21;

		//three suited
	case SUITI(5,1,1,0):
		return n[3].ranki*M21 + mcolexi(n[2].ranki, n[1].ranki) \
			+ N4*M31 + N3*N2*M21 + M32*N1;

	case SUITI(4,2,1,0):
		return n[3].ranki*N2*N1 + n[2].ranki*N1 + n[1].ranki \
			+ N4*M31 + N3*N2*M21 + M32*N1 + N5*M21;

	case SUITI(3,3,1,0):
		return mcolexi(n[3].ranki, n[2].ranki)*N1 + n[1].ranki \
			+ N4*M31 + N3*N2*M21 + M32*N1 + N5*M21 + N4*N2*N1;

	case SUITI(3,2,2,0):
		return n[3].ranki*M22 + mcolexi(n[2].ranki, n[1].ranki) \
			+ N4*M31 + N3*N2*M21 + M32*N1 + N5*M21 + N4*N2*N1 + M23*N1;

		//two suited
	case SUITI(6,1,0,0):
		return n[3].ranki*N1 + n[2].ranki \
			+ N4*M31 + N3*N2*M21 + M32*N1 + N5*M21 + N4*N2*N1 + M23*N1 + N3*M22;

	case SUITI(5,2,0,0):
		return n[3].ranki*N2 + n[2].ranki \
			+ N4*M31 + N3*N2*M21 + M32*N1 + N5*M21 + N4*N2*N1 + M23*N1 + N3*M22 + N6*N1;

	case SUITI(4,3,0,0):
		return n[3].ranki*N3 + n[2].ranki \
			+ N4*M31 + N3*N2*M21 + M32*N1 + N5*M21 + N4*N2*N1 + M23*N1 + N3*M22 + N6*N1 + N5*N2;

		//one suited
	case SUITI(7,0,0,0):
		return n[3].ranki \
			+ N4*M31 + N3*N2*M21 + M32*N1 + N5*M21 + N4*N2*N1 + M23*N1 + N3*M22 + N6*N1 + N5*N2 + N4*N3;

	default:
		REPORT("You called getindex2N with wrong N");
		return -1;
	}
}

inline int getindex6(const suitrep n[])
{
	switch(SUITI(n[3].n_cards, n[2].n_cards, n[1].n_cards, n[0].n_cards)) //largest first
	{
		//NOW, THE SIX CARDED HANDS
		//four suited
	case SUITI(3,1,1,1):
		return n[3].ranki*M31 + mcolexi(n[2].ranki, n[1].ranki, n[0].ranki);

	case SUITI(2,2,1,1):
		return mcolexi(n[3].ranki, n[2].ranki)*M21 + mcolexi(n[1].ranki, n[0].ranki) \
			+ N3*M31;

		//three suited
	case SUITI(4,1,1,0):
		return n[3].ranki*M21 + mcolexi(n[2].ranki, n[1].ranki) \
			+ N3*M31 + M22*M21;

	case SUITI(3,2,1,0):
		return n[3].ranki*N2*N1 + n[2].ranki*N1 + n[1].ranki \
			+ N3*M31 + M22*M21 + N4*M21;

	case SUITI(2,2,2,0):
		return mcolexi(n[3].ranki, n[2].ranki, n[1].ranki) \
			+ N3*M31 + M22*M21 + N4*M21 + N3*N2*N1;

		//two suited
	case SUITI(5,1,0,0):
		return n[3].ranki*N1 + n[2].ranki \
			+ N3*M31 + M22*M21 + N4*M21 + N3*N2*N1 + M32;

	case SUITI(4,2,0,0):
		return n[3].ranki*N2 + n[2].ranki \
			+ N3*M31 + M22*M21 + N4*M21 + N3*N2*N1 + M32 + N5*N1;

	case SUITI(3,3,0,0):
		return mcolexi(n[3].ranki, n[2].ranki) \
			+ N3*M31 + M22*M21 + N4*M21 + N3*N2*N1 + M32 + N5*N1 + N4*N2;

		//one suited
	case SUITI(6,0,0,0):
		return n[3].ranki \
			+ N3*M31 + M22*M21 + N4*M21 + N3*N2*N1 + M32 + N5*N1 + N4*N2 + M23;

	default:
		REPORT("You called getindex2N with wrong N"); 
		return -1;
	}
}

inline int getindex5(const suitrep n[])
{
	switch(SUITI(n[3].n_cards, n[2].n_cards, n[1].n_cards, n[0].n_cards)) //largest first
	{
		//FINALLY, THE FIVE CARDED HANDS
		//four suited
	case SUITI(2,1,1,1):
		return n[3].ranki*M31 + mcolexi(n[2].ranki, n[1].ranki, n[0].ranki);

		//three suited
	case SUITI(3,1,1,0):
		return n[3].ranki*M21 + mcolexi(n[2].ranki, n[1].ranki) + N2*M31;

	case SUITI(2,2,1,0):
		return mcolexi(n[3].ranki, n[2].ranki)*N1 + n[1].ranki + N2*M31 + N3*M21;

		//two suited
	case SUITI(4,1,0,0):
		return n[3].ranki*N1 + n[2].ranki + N2*M31 + N3*M21 + M22*N1;

	case SUITI(3,2,0,0):
		return n[3].ranki*N2 + n[2].ranki + N2*M31 + N3*M21 + M22*N1 + N4*N1;

		//one suited
	case SUITI(5,0,0,0):
		return n[3].ranki + N2*M31 + N3*M21 + M22*N1 + N4*N1 + N3*N2;

	default:
		REPORT("You called getindex2N with wrong N");
		return -1;
	}
}

inline int getindex2(const suitrep n[])
{
	switch(SUITI(n[3].n_cards, n[2].n_cards, n[1].n_cards, n[0].n_cards)) //largest first
	{
		//AND NOW, THE TWO CARD HANDS
		//two suited
	case SUITI(1,1,0,0):
		return MCOL2(3,2);

		//one suited
	case SUITI(2,0,0,0):
		return n[3].ranki + M21;

	default:
		REPORT("You called getindex2 with wrong amount of cards");
		return -1;
	}
}


//converts a single suit's worth of cardmask to my custom suitrep struct
//defines indexing based on arbitrary ordering defined in pokereval source
//watch for changes to pokereval source
void masktosuitrep(unsigned int m, unsigned int b, suitrep &sr)
{
	//mask of all cards
	const unsigned int mask = m | b;
	//simple local array of the ranks in this suit
	int rank[7];

	//my[] holds the placement of my hole cards within this suit.
	//initialize it to 'not present' (meaning my cards are not in this suit)
	sr.my[0] = sr.my[1] = -1;

	//look through each bit to find the placement of my hole cards.
	//smaller cards get lower numbers, start counting at 0
	for(int i=0,j=0,passed=0; m!=0; i++)
	{
		if ((b&1)==1) passed++;
		else if ((m&1)==1) sr.my[j++]= passed++;//range of my[] should be 0..6, with larger values rare
		m>>=1;
		b>>=1;
	}

	//note we are done with m and b now, and will only use the OR'd mask from here on out.
	//quickly determine the number of cards in this suit, and which they are.
	sr.n_cards = nBitsTable[mask];
	if(sr.n_cards <= 5)
	{
		const HandVal hv = topFiveCardsTable[mask];
		switch(sr.n_cards)
		{
		case 5: rank[4] = HandVal_FIFTH_CARD(hv); //rank[] is sorted by decreasing rank, rank[0] BIG
		case 4: rank[3] = HandVal_FOURTH_CARD(hv);
		case 3: rank[2] = HandVal_THIRD_CARD(hv);
		case 2: rank[1] = HandVal_SECOND_CARD(hv);
		case 1: rank[0] = HandVal_TOP_CARD(hv);
		}
	}
	else
	{
		int j=0, i=12;
		while(j < sr.n_cards)
		{
			if (mask & (1<<i)) rank[j++] = i; //rank[0] gets the LARGEST one
			assert(i>=0);
			i--;
		}
	}

	//Finally, change those rank[] numbers into something more index-like
	sr.ranki = colexi(rank, sr.n_cards);
}

int getindex2N(const CardMask& mine, const CardMask& board, const int boardcards, int * suits)
{
	suitrep n[4];
	int mytotal[2];
	int ret;

	if(CardMask_ANY_SET(mine, board))
		REPORT("duplicate cards found in getindex2N");

	//for ease of coding in some areas, its handy to call getindex2N when you really mean getindex2.
	if(boardcards==0 && StdDeck_CardMask_IS_EMPTY(board)) return getindex2(mine);

	//fill suitrep 'suit' field - only used by 231 and 2311
	n[0].suit = StdDeck_Suit_SPADES;
	n[1].suit = StdDeck_Suit_DIAMONDS;
	n[2].suit = StdDeck_Suit_CLUBS;
	n[3].suit = StdDeck_Suit_HEARTS;
	//fill the rest of the structure
	masktosuitrep(CardMask_SPADES(mine),   CardMask_SPADES(board),   n[0]);
	masktosuitrep(CardMask_DIAMONDS(mine), CardMask_DIAMONDS(board), n[1]);
	masktosuitrep(CardMask_CLUBS(mine),    CardMask_CLUBS(board),    n[2]);
	masktosuitrep(CardMask_HEARTS(mine),   CardMask_HEARTS(board),   n[3]);

	std::sort(n, n+4);
	//n[3] is BIGGEST - MOST CARDS, or if same cards, MOST RANKI
	//n[0] IS smallest - least cards, or if same cards, SMALLEST RANK.

	if(suits!=NULL) // - only used by 231 and 2311
	{
		suits[0] = n[0].suit;
		suits[1] = n[1].suit;
		suits[2] = n[2].suit;
		suits[3] = n[3].suit;
	}

	for(int i=3,j=1,passed=0; j>=0; i--) //increment from LARGEST to SMALLEST (for speed and consistency)
	{
		assert(i>=0);
		if(n[i].my[0]==-1)
		{
			passed += n[i].n_cards;
			continue;
		}
		else 
		{
			mytotal[j--] = passed + n[i].my[0]; //mytotal[1] has SMALLEST value, NECESSARY for colexi
			if(n[i].my[1]==-1)
			{
				passed += n[i].n_cards;
				continue;
			}
			else
			{
				mytotal[j] = passed + n[i].my[1];
				break;//does not see if{}
			}
		}
	}


	//The arbitrary ordering of mytotal, defined as the locations of my two cards
	//among the 7 total cards, is as follows. Start with the largest suit, defined
	//in comments around sort() above. The lowest cards in that suit are first, the
	//higher cards in that suit come next, then the cards in the next highest suit come
	//next, and so on, until the lowest suit.
	ret = colexi(mytotal,2);

	//this right here is the only code that depends on number of cards
	switch(boardcards)
	{
	case 3: 
		assert(0<=ret && ret<10);
		ret*=INDEX5_MAX; 
		ret+=getindex5(n);
		break;

	case 4: 
		assert(0<=ret && ret<15);
		ret*=INDEX6_MAX; 
		ret+=getindex6(n);
		break;

	case 5: 
		assert(0<=ret && ret<21);
		ret*=INDEX7_MAX; 
		ret+=getindex7(n);
		break;
		
	default:
		REPORT("You called getindex2N with wrong N");
	}

	return ret;
}

//this code is not going to match the above in style or convention.
//it was written much later, and will be much more awesome

int getindex231(CardMask pflop, CardMask flop, CardMask turn)
{
	if(CardMask_ANY_SET(flop, turn))
		REPORT("duplicate cards found in getindex231");

	//need to get an ordering of the 4 suits that does not change with suit rotation (and the index)

	CardMask board;
	CardMask_OR(board, flop, turn);
	int suits[4];
	int index24 = getindex2N(pflop, board, 4, suits);

	//we just need to find the location of the turn card among the 4 total cards
	//the ordering doesn't matter, but it NEEDS to be such that a rotation of the suits won't change
	//the index.

	int num_passed = 0;
	for(int i=3; i>=0; i--) //faster since most cards sorted to end
		for(int r=0; r<13; r++)
			if(CardMask_CARD_IS_SET(flop, StdDeck_MAKE_CARD(r, suits[i])))
				num_passed++;
			else if(CardMask_CARD_IS_SET(turn, StdDeck_MAKE_CARD(r, suits[i])))
				goto foundit; //need to break out of double loop
foundit:

	//now num_passed is the 0-3 location of the turn card, in some arbitrary ordering defined by PokerEval

	if(num_passed < 0 || num_passed > 3)
		REPORT("getindex231 failed!");

	//we just need to combine that with the 24 index, and we're good.

	return combine(index24, num_passed, 4);
}

int64 getindex2311(CardMask pflop, CardMask flop, CardMask turn, CardMask river)
{
	//need to get an ordering of the 4 suits that does not change with suit rotation (and the index)

	if(CardMask_ANY_SET(flop, turn) || CardMask_ANY_SET(flop, river) || CardMask_ANY_SET(river, turn))
		REPORT("duplicate cards found in getindex231");
	CardMask board;
	CardMask_OR(board, flop, turn);
	CardMask_OR(board, board, river);
	int suits[4];
	int index25 = getindex2N(pflop, board, 5, suits);

	//i give you 5 (arbitrarily) ordered cards (from the number returned by getindex2N with N=5)
	//i need to know, which 1 of the 5 is the turn, and which 1 of the remaining 4 is the river
	//so i find the number of cards to the left of the turn-card, which will be 0,1,2,3,or 4, 
	//and i find the number of cards --besides the turn-card-- to the left of the river,
	//which will be 0,1,2,or 3.

	int num_passed = 0, turn_location = -1, river_location = -1;

	for(int i=0; i<4 && (turn_location<0 || river_location<0); i++)
		for(int r=StdDeck_Rank_FIRST; r<=StdDeck_Rank_LAST && (turn_location<0 || river_location<0); r++)

			if(CardMask_CARD_IS_SET(river, StdDeck_MAKE_CARD(r, suits[i])))
				river_location = num_passed++; //count river for turn count (and not river count -- post inc)
			else if(CardMask_CARD_IS_SET(turn, StdDeck_MAKE_CARD(r, suits[i])))
				turn_location = num_passed; //do not count turn card for either count
			else if(CardMask_CARD_IS_SET(flop, StdDeck_MAKE_CARD(r, suits[i])))
				num_passed++;

	if(num_passed < 1 || num_passed > 5 || 
			turn_location < 0 || turn_location > 4 || 
			river_location < 0 || river_location > 3)
		REPORT("getindex2311 failed! num_passed="+tostring(num_passed)
			+", turn_loc="+tostring(turn_location)+", river_loc="+tostring(river_location));

	//we just need to combine that with the 25 index, and we're good.

	return combine(index25, turn_location, 5, river_location, 4);
}

int getindex2(const CardMask& mine)
{
	suitrep n[4];

	//gives us a little more info than we need in this case
	masktosuitrep(CardMask_SPADES(mine),   0, n[0]);
	masktosuitrep(CardMask_DIAMONDS(mine), 0, n[1]);
	masktosuitrep(CardMask_CLUBS(mine),    0, n[2]);
	masktosuitrep(CardMask_HEARTS(mine),   0, n[3]);

	std::sort(n, n+4);
	//n[3] is BIGGEST - MOST CARDS, or if same cards, MOST RANKI
	//n[0] IS smallest - least cards, or if same cards, SMALLEST RANK.

	return getindex2(n);
}

int getindex2N(const vector<CardMask> &cards, int maxround)
{
	CardMask board;
	CardMask_RESET(board);
	if((maxround = std::min((int)cards.size()-1, maxround)) == PREFLOP)
		return getindex2(cards[PREFLOP]);
	else
	{
		for(int i=FLOP; i<=maxround; i++)
			CardMask_OR(board, board, cards[i]);
		return getindex2N(cards[PREFLOP], board, maxround+2);
	}
}


//Here, begins the testing code. Code used to verify that the above does
//indeed produce unique indices that only combine non-distinct hands.
#ifdef COMPILE_TESTCODE

#include <fstream>
#include <string.h> //for memset
#include "inlines/eval.h"
using namespace std;

//globally defined logfile
ofstream log;

//currently not used.
void reportbadindex(int badi, int N)
{
	CardMask m,b;

	log << "The following hands all ";

	ENUMERATE_2_CARDS(m, 
	{
		ENUMERATE_N_CARDS_D(b, N-2, m, 
		{
			if (getindex2N(m,b,N-2)==badi)
			{
				//f(m,b); //calls getindex2N again in a place where i can set breakpoint
				StdDeck_printMask(m);
				cout << " , ";
				StdDeck_printMask(b);
				cout << endl;
			}
		});
	});
}

//these should 0) get index, 1) verify index in range, 2) increment count, 3) check 2 handval's.
void checkindex(CardMask m, CardMask b, int N, char * cnt2N, HandVal * val2N, HandVal * valN)
{
	//get the index
	int i = getindex2N(m,b,N-2);
	int j,maxi;
	HandVal val;
	CardMask w;
	CardMask_OR(w,m,b);

	switch(N)
	{
	case 5: 
		j = i%INDEX5_MAX; 
		maxi = INDEX23_MAX;
		break;

	case 6: 
		j = i%INDEX6_MAX; 
		maxi = INDEX24_MAX;
		break;
		
	case 7: 
		j = i%INDEX7_MAX; 
		maxi = INDEX25_MAX;
		break;
	default:
		log << "YOU FUCKED UP." << endl;
	}

	//check the N-card hand value
	val = Hand_EVAL_N(w,N);
	//if val is zero, then my initialization won't work.
	if(val==0) log << "HandVal is 0 for " << Deck_maskString(w) << endl;
	//if stored value is zero, that means we haven't been here yet
	if(valN[j]==0) valN[j] = val;
	//if these miss, then we indexed wrong.
	else if(valN[j] != val)
		log << "Incorrect i" << N << " HandVal: " << valN[j] << " != " << val 
			<< " for " << Deck_maskString(w) << endl;

	//check the index range before we use i
	if(i<0 || i>=maxi)
	{
		log << "Index2" << N-2 << " out of range " << i << " : " << Deck_maskString(m) 
			<< ", " << Deck_maskString(b) << endl;
		return;
	}

	//increase count, to be checked later
	cnt2N[i]++;

	//check the 3-card hand value, same deal (not sure how this works for 3 or 4 cards)
	val = Hand_EVAL_N(b,N-2);
	if(val==0) log << "HandVal is 0 for " << Deck_maskString(b) << endl;
	if(val2N[i]==0) val2N[i] = val;
	else if(val2N[i] != val)
		log << "Incorrect i2" << N-2 << " HandVal: " << val2N[i] << " != " << val 
			<< " for " << Deck_maskString(b) << endl;
}

void checkcount(char * count, int maxi, int N)
{
	//we will keep track of when the values go in and out of range
	bool datawasok = (count[0]>=4 && count[0]<=24);
	//we will count the number of times each possible count was seen
	int numseen[100];

	//initialize numseen to having seen nothing
	for(int i=0; i<100; i++)
		numseen[i]=0;

	//start logging
	log << "Testing count2" << N-2 << ":" << endl;

	if(!datawasok)
		log << "  Begins with bad data." << endl;

	for(int i=0; i< maxi; i++) 
	{
		if (datawasok && !(count[i]>=4 && count[i]<=24))
			log << "  Data goes bad (not within 4-24 inclusive) at " << i << endl;
		else if (!datawasok && (count[i]>=4 && count[i]<=24))
			log << "  Data is okay starting " << i << endl << endl;
		datawasok = (count[i]>=4 && count[i]<=24);
		if(0<=count[i] && count[i]<100) numseen[(int)count[i]]++;
	}

	log << "  Multiplicity of values between 0 and 100 seen in this count array:" << endl;
	for(int i=0; i<100; i++)
		if (numseen[i]!=0) log << "   " << i << ": " << numseen[i] << endl;
	log << endl << endl;
}

	
// We check this:
//  Is every index within the range 0..INDEX2n_MAX ?
//  Does every hand mapped to the same index have the same N-2 HandVal?
//  Does every hand mapped to the same N-card index have the same N card HandVal?
//  Do the number of hands mapped to each index make sense?
// For the 3rd one, we make use of the implementation of the index and mod by INDEXN_MAX
//  to get the N-card index, rather than the 2n index that tracks hole cards
// For the 4th one, we make sure that between 4 and 24 hands map to each index,
//  and print out the number of hands mapping to all indices for someone to make sense of.
// 
// If that works, and all the indexes map within the required range, and all of the 
// possible indices are used at least once (should be at least 4 times though), and
// the HandVal's work, then there should be no problems.
void testindex2N()
{
	CardMask m,b; //m for mine, b for board
	long long int n=0; //counter used for status output only

	//data buffers for stored testing data. megabyte values before bugfixes.
	char *    count25 = new    char[INDEX25_MAX]; //131 mb
	HandVal * value25 = new HandVal[INDEX25_MAX]; //524 mb
	HandVal * value7  = new HandVal[INDEX7_MAX];  // 25 mb
	char *    count24 = new    char[INDEX24_MAX]; // 14 mb
	HandVal * value24 = new HandVal[INDEX24_MAX]; // 55 mb
	HandVal * value6  = new HandVal[INDEX6_MAX];  //3.7 mb
	char *    count23 = new    char[INDEX23_MAX]; //1.3 mb
	HandVal * value23 = new HandVal[INDEX23_MAX]; //5.1 mb
	HandVal * value5  = new HandVal[INDEX5_MAX];  //525 Kb
	char *    count2  = new    char[INDEX2_MAX];
	HandVal * value2  = new HandVal[INDEX2_MAX];

	//initialize the arrays to nothing.
	memset(count25, 0, INDEX25_MAX*sizeof(char));
	memset(value25, 0, INDEX25_MAX*sizeof(HandVal));
	memset(value7,  0, INDEX7_MAX*sizeof(HandVal));
	memset(count24, 0, INDEX24_MAX*sizeof(char));
	memset(value24, 0, INDEX24_MAX*sizeof(HandVal));
	memset(value6,  0, INDEX6_MAX*sizeof(HandVal));
	memset(count23, 0, INDEX23_MAX*sizeof(char));
	memset(value23, 0, INDEX23_MAX*sizeof(HandVal));
	memset(value5,  0, INDEX5_MAX*sizeof(HandVal));
	memset(count2,  0, INDEX2_MAX*sizeof(char));
	memset(value2,  0, INDEX2_MAX*sizeof(HandVal));

	//globally defined logfile
	log.open("testindex2N.txt");

	log << "Report of errors and data for getindex2N():" << endl << endl;

	cout << "Testing preflop..." << endl;

	ENUMERATE_2_CARDS(m,
	{
		int i = getindex2(m);
		HandVal val;
		//check the index range before we use i
		if(i<0 || i>=INDEX2_MAX)
			log << "Index2 out of range " << i << " : " << Deck_maskString(m) << endl;
		else
		{
			//increase count, to be checked later
			count2[i]++;

			//check the 2-card hand value
			val = Hand_EVAL_N(m,2);
			if(val==0) log << "HandVal is 0 for " << Deck_maskString(m) << endl;
			if(value2[i]==0) value2[i] = val;
			else if(value2[i] != val)
				log << "Incorrect i2 HandVal: " << value2[i] << " != " << val 
					<< " for " << Deck_maskString(m) << endl;
		}
	});

	checkcount(count2, INDEX2_MAX, 2);

	cout << endl << "Testing flop..." << endl;
	n=0;

	ENUMERATE_2_CARDS(m, 
	{
		ENUMERATE_3_CARDS_D(b, m, 
		{
			if (++n%328982==0) cout << "." << flush;
			checkindex(m,b,5,count23,value23,value5);
		});
	});

	checkcount(count23, INDEX23_MAX, 5);

	cout << endl << "Testing turn..." << endl;
	n=0;

	ENUMERATE_2_CARDS(m,
	{
		ENUMERATE_4_CARDS_D(b, m, 
		{
			if (++n%3865542==0) cout << "." << flush;
			checkindex(m,b,6,count24,value24,value6);
		});
	});

	checkcount(count24, INDEX24_MAX, 6);

	cout << endl << "Testing river..." << endl;
	n=0;

	ENUMERATE_2_CARDS(m,
	{
		ENUMERATE_5_CARDS_D(b, m, 
		{
			if (++n%35562984==0) cout << "." << flush;
			checkindex(m,b,7,count25,value25,value7);
		});
	});

	checkcount(count25, INDEX25_MAX, 7);

	log << endl << "Finished reporting errors and data for getindex2N()." << endl;

	log.close();
}

#include <vector>
const int PREFLOP = 0;
const int FLOP = 1;
const int TURN = 2;
const int RIVER = 3;
inline void checkindex231(const int64 index, const vector<CardMask> &cards, 
		HandVal &hv1, HandVal &hv2, HandVal &hv3, bool haveseen, int pass)
{
	HandVal myhv1, myhv2, myhv3;
	CardMask temp;
	CardMask_OR(temp, cards[PREFLOP], cards[FLOP]);

	switch(pass)
	{
		case 0:
			myhv1 = Hand_EVAL_N(cards[PREFLOP], 2);
			myhv2 = Hand_EVAL_N(temp, 5);
			CardMask_OR(temp, temp, cards[TURN]);
			myhv3 = Hand_EVAL_N(temp, 6);
			break;

		case 1:
			myhv1 = Hand_EVAL_N(cards[PREFLOP], 2);
			myhv2 = Hand_EVAL_N(temp, 5);
			break;

		case 2:
			CardMask_OR(temp, temp, cards[TURN]);
			myhv1 = Hand_EVAL_N(temp, 6);
			CardMask_OR(temp, temp, cards[RIVER]);
			myhv2 = Hand_EVAL_N(temp, 7);
			break;
	}


	if(!haveseen)
	{
		hv1 = myhv1;
		hv2 = myhv2;
		if(pass==0) hv3 = myhv3;
	}
	else if(hv1 != myhv1 || hv2 != myhv2 || (pass==0 && hv3 != myhv3))
		REPORT("handval check failure at index:"+tostring(index)+" pass:"+tostring(pass)+"  "+tostring(cards));
}

void testindex231()
{
	for(int pass = 0; pass < 5; pass++)
	{
		int64 index_max;
		if(pass == 0)
			index_max = INDEX231_MAX;
		else if(pass == 1 || pass == 2)
			index_max = INDEX2311_MAX;
		else if(pass == 3)
			index_max = INDEX24_MAX;
		else if(pass == 4)
			index_max = INDEX25_MAX;

		vector< HandVal > handval1(pass == 0 || pass == 1 || pass == 2 ? index_max : 0, 0);
		vector< HandVal > handval2(pass == 0 || pass == 1 || pass == 2 ? index_max : 0, 0);
		vector< HandVal > handval3(pass == 0 ? index_max : 0, 0); //only used for 231
		vector< short > count(pass != 1 ? index_max : 0, 0);
		vector< bool > seen(index_max, false);
		vector< CardMask > cards(pass == 1 || pass == 2 ? 4 : pass == 0 ? 3 : 2);

		ENUMERATE_2_CARDS(cards[PREFLOP],
		{
			if(pass == 0 || pass == 1 || pass == 2)
			{
				ENUMERATE_3_CARDS_D(cards[FLOP], cards[PREFLOP],
				{
					CardMask dead;
					CardMask_OR(dead, cards[PREFLOP], cards[FLOP]);
					ENUMERATE_1_CARDS_D(cards[TURN], dead,
					{
						if(pass == 0)
						{
							int64 index = getindex231(cards[PREFLOP], cards[FLOP], cards[TURN]);
							if(index < 0 || index >= INDEX231_MAX)
								REPORT("index is out of range: "+tostring(index));
							count[index]++;
							checkindex231(index, cards, handval1[index], handval2[index], handval3[index], seen[index], pass);
							seen[index] = true;
						}
						else if(pass == 1 || pass == 2)
						{
							CardMask dead2;
							CardMask_OR(dead2, dead, cards[TURN]);
							ENUMERATE_1_CARDS_D(cards[RIVER], dead2,
							{
								int64 index = getindex2311(cards[PREFLOP], cards[FLOP], cards[TURN], cards[RIVER]);
								if(index < 0 || index >= INDEX2311_MAX)
									REPORT("index is out of range: "+tostring(index));
								if(pass==2) count[index]++;
								checkindex231(index, cards, handval1[index], handval2[index], handval3[index], seen[index], pass);
								seen[index] = true;
							});
						}
					});
				});
			}
			else if(pass == 3 || pass == 4)
			{
				CardMask board;
				ENUMERATE_N_CARDS_D(board, pass+1, cards[PREFLOP], 
				{
					count[getindex2N(cards[PREFLOP], board, pass+1)]++;
				});
			}
		});


		if(pass == 0 || pass == 2 || pass == 3 || pass == 4)
		{
			//we will count the number of times each possible count was seen
			vector< int64 > numseen(100, 0);

			for(int64 i=0; i<index_max; i++)
				if(count[0]<0 || count[0]>=100)
					REPORT("count out of range! pass:" + tostring(pass)+" index: "+tostring(i));
				else
					numseen[count[i]]++;

			cout << "  Multiplicity of values between 0 and 100 seen in pass " << pass << ':' << endl;
			for(int i=0; i<100; i++)
				if (numseen[i]!=0) 
					cout << "   " << i << ": " << numseen[i] << endl;
		}
	}
}

/*** Correct results, from when vector numseen above was a vector<int>. 
  ** Adding 2^32 to that one yields the correct value: 
  ** -2132987476 + 2^32 = 2161979820 = 108098991 * 20
  **
  ** Multiplying pass 4 by twenty yields the pass 2 counts, multiplying the 
  ** pass 3 by four, yields the pass 0 counts. No errors were REPORTed, 
  ** indexing function works.

scott@pokerserver:~/pokerbreedinggrounds/bin$ ./binmaker test
  Multiplicity of values between 0 and 100 seen in pass 0:
   4: 330720
   6: 78000
   12: 13097760
   24: 44272800
  Multiplicity of values between 0 and 100 seen in pass 2:
   4: 5050500
   12: 356816460
   24: -2132987476
  Multiplicity of values between 0 and 100 seen in pass 3:
   4: 82680
   6: 19500
   12: 3274440
   24: 11068200
  Multiplicity of values between 0 and 100 seen in pass 4:
   4: 252525
   12: 17840823
   24: 108098991

*******/



#endif
