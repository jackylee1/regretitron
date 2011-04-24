#include "stdafx.h"
#include "cardmachine.h"
#include "../HandIndexingV1/getindex2N.h"
#include "../utility.h" //has COMBINE, REPORT...
#include "../PokerLibrary/constants.h"
#include <fstream>
#include <iomanip>
#include <poker_defs.h>
#include <inlines/eval.h>
using namespace std;

//static non-integral data member initialization
const int CardMachine::FLOPALYZER_MAX[4]= { 1, 6, 17, 37 };

CardMachine::CardMachine(cardsettings_t cardsettings, bool issolver, MTRand::uint32 randseed ) :
	myparams(cardsettings),
	cardsi_max(4, -1),
	binfiles(4, (PackedBinFile*)NULL),
	solving(issolver),
	m_randseed( randseed ),
	randforsolver( randseed ),
	randforexamplehands() //seeds with time and clock
{

	checkdataformat(); //magic number test

	//for each bin file..

	for(int gr=0; gr<4; gr++)
	{
		//set cardsi_max

		cardsi_max[gr] = myparams.bin_max[gr]; //initialize

		if(myparams.usehistory) //multiply in past bins
			for(int pastgr=0; pastgr<gr; pastgr++)
				cardsi_max[gr] *= myparams.bin_max[pastgr];

		if(myparams.useflopalyzer) //multiply in flopalyzer
			cardsi_max[gr] *= FLOPALYZER_MAX[gr];

		//if there IS a file.. open them

		if((myparams.filename[gr].length() != 0 && myparams.filesize[gr] == 0) 
			|| (myparams.filename[gr].length() == 0 && myparams.filesize[gr] != 0))
			REPORT("you have a bin file but it's size is set to zero");
		if(myparams.filesize[gr] != 0)
		{
			if(solving) //don't print unless guaranteed have console (i.e. solving)
				cout << "loading " << myparams.filename[gr] << " into memory..." << endl;

			//so ugly to do this in the loop...
			int64 index_max;
			if(gr==PREFLOP)
				index_max = INDEX2_MAX;
			else if(gr==FLOP)
				index_max = INDEX23_MAX;
			else if(gr==TURN)
				index_max = myparams.usehistory ? INDEX231_MAX : INDEX24_MAX;
			else
				index_max = myparams.usehistory ? INDEX2311_MAX : INDEX25_MAX;

			//if solving or under 5 megs, preload the file
			binfiles[gr] = new PackedBinFile(myparams.filename[gr], myparams.filesize[gr], 
				myparams.bin_max[gr], index_max,
				(solving || (myparams.filesize[gr] != -1 && myparams.filesize[gr] < 5 * 1024 * 1024)));
		}
	}
}

CardMachine::~CardMachine()
{
	for(int gr=0; gr<4; gr++)
		if(binfiles[gr] != NULL)
			delete binfiles[gr];
}

void CardMachine::getnewgame(int cardsi[4][2], int &twoprob0wins)
{
	if(!solving) REPORT("I didn't expect PokerPlayer to call this function");

	//deal out the cards randomly.

	CardMask priv[2], board[4];
	int random_vector[52] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
		30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
	for(int i=0; i<2; i++) CardMask_RESET(priv[i]);
	for(int i=0; i<4; i++) CardMask_RESET(board[i]);
	for(int i=0; i<9; i++) swap(random_vector[i], random_vector[i+randforsolver.randInt(51-i)]);
	CardMask_SET(priv[P0],random_vector[0]);
	CardMask_SET(priv[P0],random_vector[1]);
	CardMask_SET(priv[P1],random_vector[2]);
	CardMask_SET(priv[P1],random_vector[3]);
	CardMask_SET(board[FLOP],random_vector[4]);
	CardMask_SET(board[FLOP],random_vector[5]);
	CardMask_SET(board[FLOP],random_vector[6]);
	CardMask_SET(board[TURN],random_vector[7]);
	CardMask_SET(board[RIVER],random_vector[8]);

	//initialize cardsi with bin numbers

	if(myparams.usehistory) //a la CPRG
	{
		int bin[4][2];
		bin[PREFLOP][P0] = binfiles[PREFLOP]->retrieve(getindex2(priv[P0]));
		bin[PREFLOP][P1] = binfiles[PREFLOP]->retrieve(getindex2(priv[P1]));
		bin[FLOP][P0] = binfiles[FLOP]->retrieve(getindex23(priv[P0], board[FLOP]));
		bin[FLOP][P1] = binfiles[FLOP]->retrieve(getindex23(priv[P1], board[FLOP]));
		bin[TURN][P0] = binfiles[TURN]->retrieve(getindex231(priv[P0], board[FLOP], board[TURN]));
		bin[TURN][P1] = binfiles[TURN]->retrieve(getindex231(priv[P1], board[FLOP], board[TURN]));
		bin[RIVER][P0] = binfiles[RIVER]->retrieve(getindex2311(priv[P0], board[FLOP], board[TURN], board[RIVER]));
		bin[RIVER][P1] = binfiles[RIVER]->retrieve(getindex2311(priv[P1], board[FLOP], board[TURN], board[RIVER]));
		
		cardsi[PREFLOP][P0] = bin[PREFLOP][P0];
		cardsi[PREFLOP][P1] = bin[PREFLOP][P1];

		const int* const &binmax = myparams.bin_max; //alias binmax for shorter lines
		cardsi[FLOP][P0] = combine(bin[FLOP][P0],   bin[PREFLOP][P0], binmax[PREFLOP]);
		cardsi[FLOP][P1] = combine(bin[FLOP][P1],   bin[PREFLOP][P1], binmax[PREFLOP]);

		cardsi[TURN][P0] = combine(bin[TURN][P0],   bin[FLOP][P0], binmax[FLOP],   bin[PREFLOP][P0], binmax[PREFLOP]);
		cardsi[TURN][P1] = combine(bin[TURN][P1],   bin[FLOP][P1], binmax[FLOP],   bin[PREFLOP][P1], binmax[PREFLOP]);

		cardsi[RIVER][P0] = combine(bin[RIVER][P0],                bin[TURN][P0], binmax[TURN],   
				                    bin[FLOP][P0], binmax[FLOP],   bin[PREFLOP][P0], binmax[PREFLOP]);
		cardsi[RIVER][P1] = combine(bin[RIVER][P1],                bin[TURN][P1], binmax[TURN],   
				                    bin[FLOP][P1], binmax[FLOP],   bin[PREFLOP][P1], binmax[PREFLOP]);
	}
	else //my old system
	{
		cardsi[PREFLOP][P0] = getindex2(priv[P0]);
		cardsi[PREFLOP][P1] = getindex2(priv[P1]);
		CardMask fullboard;
		CardMask_RESET(fullboard);
		for(int gr=FLOP; gr<4; gr++)
		{
			CardMask_OR(fullboard, fullboard, board[gr]);
			cardsi[gr][P0] = binfiles[gr]->retrieve(getindex2N(priv[P0], fullboard, gr+2)); //gr+2 is number of board cards
			cardsi[gr][P1] = binfiles[gr]->retrieve(getindex2N(priv[P1], fullboard, gr+2));
		}
	}

	//insert the flopalyzer score in-place if requested

	if(myparams.useflopalyzer)
	{
		int flopscore = flopalyzer(board[FLOP]);
		int turnscore = turnalyzer(board[FLOP], board[TURN]);
		int riverscore = rivalyzer(board[FLOP], board[TURN], board[RIVER]);
		cardsi[FLOP][P0] = combine(cardsi[FLOP][P0], flopscore, FLOPALYZER_MAX[FLOP]);
		cardsi[FLOP][P1] = combine(cardsi[FLOP][P1], flopscore, FLOPALYZER_MAX[FLOP]);
		cardsi[TURN][P0] = combine(cardsi[TURN][P0], turnscore, FLOPALYZER_MAX[TURN]);
		cardsi[TURN][P1] = combine(cardsi[TURN][P1], turnscore, FLOPALYZER_MAX[TURN]);
		cardsi[RIVER][P0] = combine(cardsi[RIVER][P0], riverscore, FLOPALYZER_MAX[RIVER]);
		cardsi[RIVER][P1] = combine(cardsi[RIVER][P1], riverscore, FLOPALYZER_MAX[RIVER]);
	}

	//compute would-be winner for twoprob0wins

	CardMask full0, full1;
	CardMask fullboard;
	CardMask_OR(fullboard, board[FLOP], board[TURN]);
	CardMask_OR(fullboard, fullboard, board[RIVER]);
	CardMask_OR(full0, priv[P0], fullboard);
	CardMask_OR(full1, priv[P1], fullboard);

	HandVal r0=Hand_EVAL_N(full0, 7);
	HandVal r1=Hand_EVAL_N(full1, 7);

	if (r0>r1)
		twoprob0wins = 2;
	else if(r1>r0)
		twoprob0wins = 0;
	else
		twoprob0wins = 1;
}

//returns cardsi
int CardMachine::getindices(int gr, const vector<CardMask> &cards, vector<int> &handi, int &boardi)
{
	handi.resize(gr+1);
	if(cards.size() != (unsigned)gr+1) REPORT("gr and cards-size must match");

	int cardsi;

	if(myparams.usehistory)
	{
		//use a switch jump table to get bin numbers for current round and all past rounds

		switch(gr)
		{
			case RIVER:
				handi[RIVER] = binfiles[RIVER]->retrieve(getindex2311(cards[PREFLOP], cards[FLOP], cards[TURN], cards[RIVER]));
			case TURN:
				handi[TURN] = binfiles[TURN]->retrieve(getindex231(cards[PREFLOP], cards[FLOP], cards[TURN]));
			case FLOP:
				handi[FLOP] = binfiles[FLOP]->retrieve(getindex2N(cards[PREFLOP], cards[FLOP], 3));
			case PREFLOP:
				handi[PREFLOP] = binfiles[PREFLOP]->retrieve(getindex2(cards[PREFLOP]));
		}

		//combine these bin numbers together in the same way as in getnewgame()

		const int* const &binmax = myparams.bin_max; //alias binmax for shorter lines
		switch(gr)
		{
			case RIVER:
				cardsi = combine(handi[RIVER], handi[TURN], binmax[TURN], 
						handi[FLOP], binmax[FLOP], handi[PREFLOP], binmax[PREFLOP]); 
				break;
			case TURN:
				cardsi = combine(handi[TURN], handi[FLOP], binmax[FLOP], handi[PREFLOP], binmax[PREFLOP]);
				break;
			case FLOP:
				cardsi = combine(handi[FLOP], handi[PREFLOP], binmax[PREFLOP]);
				break;
			case PREFLOP:
				cardsi = handi[PREFLOP];
				break;
			default: 
				REPORT("bad gr");
				exit(1);
		}
	}
	else
	{
		//my old system, cardsi is determined simply by the bin for that round
		//so look up the appropriate bin and set it to handi and cardsi

		CardMask board;
		switch(gr)
		{
			case PREFLOP:
				cardsi = handi[PREFLOP] = getindex2(cards[PREFLOP]);
				break;
			case FLOP:
				cardsi = handi[FLOP] = binfiles[FLOP]->retrieve(getindex2N(cards[PREFLOP],cards[FLOP],3));
				break;
			case TURN:
				CardMask_OR(board, cards[FLOP], cards[TURN]);
				cardsi = handi[TURN] = binfiles[TURN]->retrieve(getindex2N(cards[PREFLOP],board,4));
				break;
			case RIVER:
				CardMask_OR(board, cards[FLOP], cards[TURN]);
				CardMask_OR(board, board, cards[RIVER]);
				cardsi = handi[RIVER] = binfiles[RIVER]->retrieve(getindex2N(cards[PREFLOP],board,5));
				break;
			default:
				REPORT("invalid gameround");
				exit(1);
		}
	}

	//if we are using the flopalyzer, then combine in that score as well in the same way as in getnewgame()

	if(myparams.useflopalyzer)
	{
		switch(gr)
		{
			case PREFLOP: boardi = 0; break;
			case FLOP: boardi = flopalyzer(cards[FLOP]); break;
			case TURN: boardi = turnalyzer(cards[FLOP], cards[TURN]); break;
			case RIVER: boardi = rivalyzer(cards[FLOP], cards[TURN], cards[RIVER]); break;
			default: REPORT("bad gr");
		}
		cardsi = combine(cardsi, boardi, FLOPALYZER_MAX[gr]); //depends on boardi being 0 preflop
	}
	else
		boardi = 0;

	return cardsi;
}	


void CardMachine::findexamplehand(int gr, const vector<int> &handi, int boardi, vector<CardMask> &cards)
{
	//ensure our vectors are only as big as needed 

	cards.resize(gr+1);
	if(handi.size() != (unsigned)gr+1) REPORT("handi is wrong size");

	//alias rand for the macros

	MTRand &mersenne = randforexamplehands;

	if(myparams.usehistory) //cprg system
	{
		CardMask usedcards;
		//the story of findcount: if we randomly choose all hands at once, check the indices, 
		// and then randomly choose them again, it is slow (looking for 1 in 10000 that way),
		// if we do it incrementally, like here, only looking for 1 in 10 at each step, but
		// for some preflop/flop/turn combos, NO river card satisfies it. This is what works.
		int findcount;
restart:
		CardMask_RESET(usedcards);

		do
		{
			MONTECARLO_N_CARDS_D(cards[PREFLOP], usedcards, 2, 1, );
		}
		while(binfiles[PREFLOP]->retrieve(getindex2(cards[PREFLOP])) != handi[PREFLOP]);

		if(gr==PREFLOP) return; //we are done
		usedcards = cards[PREFLOP];

		findcount = 0;
		do
		{
			if(++findcount == 150)
				goto restart;
			MONTECARLO_N_CARDS_D(cards[FLOP], usedcards, 3, 1, );
		}
		while((myparams.useflopalyzer && flopalyzer(cards[FLOP]) != boardi)
				|| binfiles[FLOP]->retrieve(getindex23(cards[PREFLOP], cards[FLOP])) != handi[FLOP]);

		if(gr==FLOP) return;
		CardMask_OR(usedcards, usedcards, cards[FLOP]);

		findcount = 0;
		do
		{
			if(++findcount == 150)
				goto restart;
			MONTECARLO_N_CARDS_D(cards[TURN], usedcards, 1, 1, );
		}
		while((myparams.useflopalyzer && turnalyzer(cards[FLOP],cards[TURN]) != boardi)
				|| binfiles[TURN]->retrieve(getindex231(cards[PREFLOP], cards[FLOP], cards[TURN])) != handi[TURN]);

		if(gr==TURN) return;
		CardMask_OR(usedcards, usedcards, cards[TURN]);

		findcount = 0;
		do
		{
			if(++findcount == 150)
				goto restart;
			MONTECARLO_N_CARDS_D(cards[RIVER], usedcards, 1, 1, );
		}
		while((myparams.useflopalyzer && rivalyzer(cards[FLOP],cards[TURN],cards[RIVER]) != boardi)
				|| binfiles[RIVER]->retrieve(getindex2311(cards[PREFLOP], cards[FLOP], cards[TURN], cards[RIVER])) != handi[RIVER]);

		if(gr==RIVER) return;
	}
	else // my old binning system
	{
		while(1) //find entire board, it is fast due to smart if statements
		{
			CardMask board;
			CardMask_RESET(board); //also used as usedcards
			switch(gr)
			{
				//deal out random cards
				case RIVER:
					MONTECARLO_N_CARDS_D(cards[RIVER], board;, 1, 1, );
					board = cards[RIVER]; //do not break from switch
				case TURN:
					MONTECARLO_N_CARDS_D(cards[TURN], board, 1, 1, );
					CardMask_OR(board, board, cards[TURN]);
				case FLOP:
					MONTECARLO_N_CARDS_D(cards[FLOP], board, 3, 1, );
					CardMask_OR(board, board, cards[FLOP]);
				case PREFLOP:
					MONTECARLO_N_CARDS_D(cards[PREFLOP], board, 2, 1, );

			}

			switch(gr)
			{
				//check the ones that are fast to check first
				case RIVER: 
					if( (!myparams.useflopalyzer || rivalyzer(cards[FLOP],cards[TURN],cards[RIVER]) == boardi)
							&& binfiles[RIVER]->retrieve(getindex2N(cards[PREFLOP], board, 5)) == handi[RIVER])
						return;
					break; //break from switch

				case TURN: 
					if( (!myparams.useflopalyzer || turnalyzer(cards[FLOP],cards[TURN]) == boardi)
							&& binfiles[TURN]->retrieve(getindex2N(cards[PREFLOP], board, 4)) == handi[TURN])
						return;
					break;

				case FLOP: 
					if( (!myparams.useflopalyzer || flopalyzer(cards[FLOP]) == boardi)
							&& binfiles[FLOP]->retrieve(getindex2N(cards[PREFLOP], board, 3)) == handi[FLOP])
						return;
					break;
				case PREFLOP:
					if( getindex2(cards[PREFLOP]) == handi[PREFLOP] )
						return;
					break;
			}
		}
	}

	REPORT("we got to the end of example hands, but it should have returned by now.");
}
	

int CardMachine::preflophandinfo(int index, string &handstring) //index is getindex2() index, return cardsi
{

	//find a CardMask that fits this index

	CardMask mymask, usedcards;
	CardMask_RESET(usedcards);
	MTRand &mersenne = randforexamplehands;
	while(1)
	{
		MONTECARLO_N_CARDS_D(mymask, usedcards, 2, 1, );
		if(getindex2(mymask) == index)
			break;
	}

	//get the string for this hand

	char * mystring = GenericDeck_maskString(&StdDeck, &mymask);

	handstring = "";
	handstring += mystring[0]; //first rank
	handstring += mystring[3]; //second rank
	if(handstring[0] < handstring[1]) std::swap(handstring[0], handstring[1]); //sort them somewhat
	if(mystring[1] == mystring[4]) handstring += 's'; //suited

	//return the cardsi for this hand

	return getcardsi(PREFLOP, vector<CardMask>(1, mymask));
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
		return -1;
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
		return -1;
	}
}

// static function used to replace the code in solveparams.h with code that can
// be used at runtime (old code in solveparams.h generated a cardsettings_t
// at compiletime
cardsettings_t CardMachine::makecardsettings( int pfbin, int fbin, int tbin, int rbin,
		bool usehistory, bool useflopalyzer )
{
	cardsettings_t historyretval =
	{
		{ pfbin, fbin, tbin, rbin },
		{
			"bins/preflop"+tostring(pfbin),
			"bins/flop"+tostring(pfbin)+"-"+tostring(fbin),
			"bins/turn"+tostring(pfbin)+"-"+tostring(fbin)+"-"+tostring(tbin),
			"bins/river"+tostring(pfbin)+"-"+tostring(fbin)+"-"+tostring(tbin)+"-"+tostring(rbin),
		},
		{
			PackedBinFile::numwordsneeded(pfbin, INDEX2_MAX)*8,
			PackedBinFile::numwordsneeded(fbin, INDEX23_MAX)*8,
			PackedBinFile::numwordsneeded(tbin, INDEX231_MAX)*8,
			PackedBinFile::numwordsneeded(rbin, INDEX2311_MAX)*8
		},
		true, //use history
		useflopalyzer //use flopalyzer
	};

	/* default values I had for no-history (imperfect recall) bin amounts:
		#define FBIN 256
		#define TBIN 90
		#define RBIN 90
	 */

	cardsettings_t nohistoryretval =
	{
		{ INDEX2_MAX, fbin, tbin, rbin },
		{
			"",
			"bins/flop" + tostring( fbin ),
			"bins/turn" + tostring( tbin ),
			"bins/river" + tostring( rbin ),
		},
		{
			0,
			PackedBinFile::numwordsneeded(fbin, INDEX23_MAX)*8,
			PackedBinFile::numwordsneeded(tbin, INDEX24_MAX)*8,
			PackedBinFile::numwordsneeded(rbin, INDEX25_MAX)*8
		},
		false, //use history
		false //use flopalyzer
	};

	return ( usehistory ? historyretval : nohistoryretval );
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
