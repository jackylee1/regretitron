#include "stdafx.h"
#include "cardmachine.h"
#include "../PokerLibrary/binstorage.h"
#include "../HandIndexingV1/getindex2N.h"
#include "../utility.h" //has COMBINE, REPORT...
#include "../PokerLibrary/constants.h"
#include <fstream>
#include <cstring> //for strlen
#include <poker_defs.h>
#include <inlines/eval.h>

//static non-integral data member initialization
const int CardMachine::FLOPALYZER_MAX[4]= { 1, 6, 17, 37 };

CardMachine::CardMachine(cardsettings_t cardsettings, bool seedrand, int seedwith) : mersenne() //seeds with time and shit
{
	if(seedrand) //override random time seed
		mersenne.seed(seedwith);

	for(int gr=0; gr<4; gr++)
	{
		cardsi_max[gr] = cardsettings.bin_max[gr] * FLOPALYZER_MAX[gr];
		bin_max[gr] = cardsettings.bin_max[gr];

		if(strlen(cardsettings.filename[gr]) == 0)  //this means there is no file
			bindata[gr] = NULL;
		else
		{
			if(cardsettings.filesize[gr] == 0) REPORT("you have a bin file but it's size is set to zero");
			if(cardsettings.filesize[gr]%8 != 0) REPORT("Your file is not a multiple of 8 bytes.");
			bindata[gr] = new uint64[cardsettings.filesize[gr]/8];

			//open the file, read it in, and close it

			std::ifstream f(cardsettings.filename[gr], std::ifstream::binary);
			f.read((char*)bindata[gr], cardsettings.filesize[gr]);
			if(f.gcount()!=cardsettings.filesize[gr] || f.eof() || EOF!=f.peek())
				REPORT(cardsettings.filename[gr]<<" could not be found, or is not the right size.");
			f.close();
		}
	}
}

CardMachine::~CardMachine()
{
	for(int gr=0; gr<4; gr++)
		if(bindata[gr] != NULL) delete[] bindata[gr];
}

void CardMachine::getnewgame(int cardsi[4][2], int &twoprob0wins)
{

	//deal out the cards randomly.

	CardMask usedcards, priv0, priv1, flop, turn, river;
	CardMask_RESET(usedcards);
	MONTECARLO_N_CARDS_D(priv0, usedcards, 2, 1, );
	MONTECARLO_N_CARDS_D(priv1, priv0, 2, 1, );

	CardMask_OR(usedcards, priv0, priv1);
	MONTECARLO_N_CARDS_D(flop, usedcards, 3, 1, );

	CardMask_OR(usedcards, usedcards, flop);
	MONTECARLO_N_CARDS_D(turn, usedcards, 1, 1, );

	CardMask_OR(usedcards, usedcards, turn);
	MONTECARLO_N_CARDS_D(river, usedcards, 1, 1, );

	//compute flopalyzer scores

	int flopscore = flopalyzer(flop);
	int turnscore = turnalyzer(flop, turn);
	int riverscore = rivalyzer(flop, turn, river);

	//lookup binnumbers

	CardMask board;
	int binP0f = BinRoutines::retrieve(bindata[FLOP], getindex2N(priv0, flop, 3), bin_max[FLOP]);
	int binP1f = BinRoutines::retrieve(bindata[FLOP], getindex2N(priv1, flop, 3), bin_max[FLOP]);
	CardMask_OR(board, flop, turn);
	int binP0t = BinRoutines::retrieve(bindata[TURN], getindex2N(priv0, board, 4), bin_max[TURN]);
	int binP1t = BinRoutines::retrieve(bindata[TURN], getindex2N(priv1, board, 4), bin_max[TURN]);
	CardMask_OR(board, board, river);
	int binP0r = BinRoutines::retrieve(bindata[RIVER], getindex2N(priv0, board, 5), bin_max[RIVER]);
	int binP1r = BinRoutines::retrieve(bindata[RIVER], getindex2N(priv1, board, 5), bin_max[RIVER]);

	//compute the cardsi

	cardsi[PREFLOP][P0] = getindex2(priv0);
	cardsi[PREFLOP][P1] = getindex2(priv1);
	cardsi[FLOP][P0] = COMBINE(flopscore, binP0f, bin_max[FLOP]);
	cardsi[FLOP][P1] = COMBINE(flopscore, binP1f, bin_max[FLOP]);
	cardsi[TURN][P0] = COMBINE(turnscore, binP0t, bin_max[TURN]);
	cardsi[TURN][P1] = COMBINE(turnscore, binP1t, bin_max[TURN]);
	cardsi[RIVER][P0] = COMBINE(riverscore, binP0r, bin_max[RIVER]);
	cardsi[RIVER][P1] = COMBINE(riverscore, binP1r, bin_max[RIVER]);

	//compute would-be winner for twoprob0wins

	CardMask full0, full1;
	CardMask_OR(full0, priv0, board);
	CardMask_OR(full1, priv1, board);

	HandVal r0=Hand_EVAL_N(full0, 7);
	HandVal r1=Hand_EVAL_N(full1, 7);

	if (r0>r1)
		twoprob0wins = 2;
	else if(r1>r0)
		twoprob0wins = 0;
	else
		twoprob0wins = 1;
}

//macros for flopalyzer code and its breathren
#define SUITED(n) (nBitsTable[ss] == n || nBitsTable[sc] == n || nBitsTable[sd] == n || nBitsTable[sh] == n)
#define STRAIGHT3(s) ((s & (s<<1) & (s<<2)) || (s & (s<<2) & (s<<3)) || (s & (s<<1) & (s<<3)))
#define STRAIGHT4(s) ((s & (s<<1) & (s<<2) & (s<<3)) || (s & (s<<2) & (s<<3) & (s<<4)) || \
						(s & (s<<1) & (s<<3) & (s<<4)) || (s & (s<<1) & (s<<2) & (s<<4)))

int CardMachine::flopalyzer(const CardMask flop)
{
	int n_ranks;
	uint32 sc, sd, sh, ss, ranks, s;

	ss = StdDeck_CardMask_SPADES(flop);
	sc = StdDeck_CardMask_CLUBS(flop);
	sd = StdDeck_CardMask_DIAMONDS(flop);
	sh = StdDeck_CardMask_HEARTS(flop);

	ranks = sc | sd | sh | ss;
	if (ranks & (0xFFFFFFFF<<13)) REPORT("ranks appears to contain more than just cards");
	//to check for straight, duplicate ace
	s = (ranks<<1) | (ranks>>12); 
	n_ranks = nBitsTable[ranks];

	//3 of a suit or 3 of a kind
	if(SUITED(3) || n_ranks ==1)
		return 0; //5.42%
	//check for 3-straight or 1-gapped 3-straight
	else if (STRAIGHT3(s))
		return 1; //9.25%
	//2 of a suit...
	else if(SUITED(2))
	{
		//...and has a pair.
		if (n_ranks==2)
			return 2; //8.44%
		//...or it does not have a pair (nor a straight).
		else
			return 3; //40.99%
	}
	//rainbow plus a pair
	else if(n_ranks==2)
		return 4; //8.48%
	//rainbow with no pair
	else
		return 5; //27.42%
}

int CardMachine::turnalyzer(const CardMask flop, const CardMask turn)
{
	int n_ranks;
	uint32 sc, sd, sh, ss, ranks;
	CardMask board;

	CardMask_OR(board, flop, turn);

	ss = StdDeck_CardMask_SPADES(board);
	sc = StdDeck_CardMask_CLUBS(board);
	sd = StdDeck_CardMask_DIAMONDS(board);
	sh = StdDeck_CardMask_HEARTS(board);
	 
	ranks = sc | sd | sh | ss;
	if (ranks & (0xFFFFFFFF<<13)) REPORT("ranks appears to contain more than just cards");
	//to check for straight, duplicate ace
	uint32 s = (ranks<<1) | (ranks>>12); 
	n_ranks = nBitsTable[ranks];

	switch(flopalyzer(flop))
	{
	case 0://3 of a suit or 3 of a kind (5.42%)

		//4OAS or 4OAK or 3OAK
		if(SUITED(4) || n_ranks == 1 || n_ranks == 2)
			return 0;
		//3OAS + a pair
		else if(n_ranks == 3)
			return 1;
		//3OAS & no pair
		else
			return 2;

	case 1://3-straight or 1-gapped 3-straight (9.25%)

        //4-straight or 1-gapped 4-straight
		if(STRAIGHT4(s))
			return 3;
		//3-straight or 1-gapped 3-straight & a pair
		else if(n_ranks == 3)
			return 4;
		//still just the 3-straight
		else
			return 5;

	case 2://2OAS and has a pair (8.44%)

		//3OAS (must still have pair)
		if(SUITED(3))
		{
			if (n_ranks != 3) REPORT("analysis confusion abounds.");
			return 6;
		}
		//2+1+1 or 2+2 suited and just a pair
		else if (n_ranks ==3)
			return 7;
		//2+1+1 or 2+2 suited and 2 pair or 3OAK
		else
		{
			if (n_ranks != 2) REPORT("not 2 pair or 3OAK");
			return 8;
		}

	case 3://2OAS, no pair nor 3-straight (40.99%)

		//3OAS... 
		if(SUITED(3))
		{
			//... no pair
			if (n_ranks == 4)
				return 9;
			//... pair
			else
				return 10;
		}
		//2+1+1 suited or 2+2 suited, no pair
		else if(n_ranks == 4)
			return 11;
		//2+1+1 suited or 2+2 suited, pair
		else
			return 12;

	case 4://rainbow plus a pair (8.48%)

		//1+1+1+1 suited or 2+1+1 suited...
		//...3OAK or 2 pair
		if(n_ranks == 2)
			return 13;
		//...still just a pair
		else
			return 14;

	case 5://rainbow with no pair (27.42%)

		//1+1+1+1 suited or 2+1+1 suited...
		//...made pair
		if(n_ranks == 3)
			return 15;
		//...no pair
		else
			return 16;

	default:
		REPORT("invalid flop index in turnalyzer");
	}
}


int CardMachine::rivalyzer(const CardMask flop, const CardMask turn, const CardMask river)
{
	int n_ranks;
	uint32 sc, sd, sh, ss, ranks, s;
	CardMask board;

	CardMask_OR(board, flop, turn);
	CardMask_OR(board, board, river);

	ss = StdDeck_CardMask_SPADES(board);
	sc = StdDeck_CardMask_CLUBS(board);
	sd = StdDeck_CardMask_DIAMONDS(board);
	sh = StdDeck_CardMask_HEARTS(board);

	ranks = sc | sd | sh | ss;
	if (ranks & (0xFFFFFFFF<<13)) REPORT("ranks appears to contain more than just cards");
	//to check for straight, duplicate ace
	s = (ranks<<1) | (ranks>>12); 
	n_ranks = nBitsTable[ranks];

	switch(turnalyzer(flop,turn))
	{
//flop: 3 of a suit or 3 of a kind (5.42%)
	case 0://4OAS or 4OAK or 3OAK (1.30%)

		//can't improve on perfection
		return 0;

	case 1://3OAS + a pair (0.94%)

		return 1;

	case 2://3OAS & no pair (3.18%)

		//4OAS, possibly pair
		if(SUITED(4))
			return 2;
		//3OAS, possible pair
		else
			return 3;

//flop: 3-straight or 1-gapped 3-straight (9.25%)
	case 3://4-straight or 1-gapped 4-straight (2.31%)

		//hit pair also
		if(n_ranks==4)
			return 4;
		else
			return 5;

	case 4://3-straight or 1-gapped 3-straight & a pair (1.69%)

		//4-straight & pair
		if(STRAIGHT4(s))
			return 6;
		//3-straight & pair plus
		else
			return 7;


	case 5://still just the 3-straight (5.25%)

		//4-straight
		if(STRAIGHT4(s))
			return 8;
		//still 3-straight
		else
			return 9;

//flop: 2OAS and has a pair (8.44%)
	case 6://3OAS (must still have pair) (1.88%)

		//4OAS
		if(SUITED(4))
			return 10;
		//3OAS, pair plus
		else
			return 11;

	case 7://2+1+1 or 2+2 suited and just a pair (5.66%)

		//hit flush draw, pair plus
		if(SUITED(3))
			return 12;
		//no flush, hit pair
		else if(n_ranks ==4)
			return 13;
		//no flush, no pair
		else
			return 14;

	case 8://2+1+1 or 2+2 suited and 2 pair or 3OAK (0.87%)

		//hit flush draw, pairedness still crazy
		if(SUITED(3))
			return 15;
		//no flush, pairedness still crazy
		else
			return 16;

//flop: 2OAS, no pair nor 3-straight (40.99%)
	case 9://3OAS no pair (8.42%)

		//4OAS, pair or not
		if(SUITED(4))
			return 17;
		//3OAS, pair
		else if(n_ranks==4)
			return 18;
		//3OAS, no pair
		else
			return 19;

	case 10://3OAS pair (0.82%)

		//4OAS, must be 1 pair
		if(SUITED(4))
			return 20;
		//3OAS, could be pair, 2 pair, or 3OAK
		else
			return 21;

	case 11://2+1+1 suited or 2+2 suited, no pair (25.10%)

		//hit flush draw on river...
		if(SUITED(3))
		{
			//...and hit pair
			if (n_ranks==4)
				return 22;
			//...just hit flush draw only
			else
				return 23;
		}
		//did not hit flush draw
		else
		{
			//...and hit pair
			if (n_ranks==4)
				return 24;
			//...just nothing.
			else
				return 25;
		}

	case 12://2+1+1 suited or 2+2 suited, pair (6.70%)

		//hit flush draw on river...
		if(SUITED(3))
		{
			//...and increased rankedness!
			if (n_ranks==3)
				return 26;
			//...just hit flush draw only, same pair
			else
				return 27;
		}
		//did not hit flush draw
		else
		{
			//...and hit double pair or trips!
			if (n_ranks==3)
				return 28;
			//...just did not hit flush draw only
			else
				return 29;
		}

//flop: rainbow plus a pair (8.48%)
	case 13://1+1+1+1 suited or 2+1+1 suited & 3OAK or 2 pair (0.88%)

		//same as above plus all river card options
		return 30;

	case 14://1+1+1+1 suited or 2+1+1 suited & still just a pair (7.63%)

		//made 3OAK or 2 pair
		if(n_ranks == 3)
			return 31;
		//could be runner runner suited, often just still pair
		else
			return 32;

//flop: rainbow with no pair (27.42%)
	case 15://1+1+1+1 suited or 2+1+1 suited & made pair (5.04%)

		//made 3OAK or 2 pair
		if(n_ranks == 3)
			return 33;
		//could be runner runner suited, often just still made pair
		else
			return 34;

	case 16://1+1+1+1 suited or 2+1+1 suited & still no pair (22.32%)

		//made a pair
		if(n_ranks == 4)
			return 35;
		//could be runner runner suited, often just nothing
		else
			return 36;

	default:
		REPORT("invalid turn index in rivalyzer");
	}
}

#if COMPILE_FLOPALYZER_TESTCODE
#include <iostream>
using std::cout;

void CardMachine::testflopalyzer()
{
	CardMask f,t,d1,d2,r;
	CardMask_RESET(d1);
	int x[RIVER_MAX];
	int found=0;
	
	memset(x, 0, RIVER_MAX*4);

	//count the statistics
	MONTECARLO_N_CARDS_D(f, d1, 3, 10000000, 
	{
		MONTECARLO_N_CARDS_D(t, f, 1, 1, );

		CardMask_OR(d2,f,t);
		MONTECARLO_N_CARDS_D(r, d2, 1, 1, );

		x[rivalyzer(f,t,r)]++;
	});


	//print stats and representative hands
	for(int i=0; i<RIVER_MAX; i++)
	{
		cout << i << " => " << (float)x[i]/100000 << "%" << endl;
		while(found<5)
		{
			MONTECARLO_N_CARDS_D(f, d1, 3, 1, );
			MONTECARLO_N_CARDS_D(t, f, 1, 1, );
			CardMask_OR(d2,f,t);
			MONTECARLO_N_CARDS_D(r, d2, 1, 1, );

			if (rivalyzer(f,t,r)==i)
			{
				cout << "  " << printmask(f) << " : ";
				cout << printmask(t) << " : ";
				cout << printmask(r) << endl;
				++found;
			}
		}
		found=0;
		cout << endl;
	}
}

#endif
