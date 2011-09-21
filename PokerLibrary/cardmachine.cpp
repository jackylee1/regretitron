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

const int64 HACK2311_SIZE_THRESHOLD = 500000000;
bool is2311( const int64 size ) { return size < 0 || size > HACK2311_SIZE_THRESHOLD; }

CardMachine::CardMachine(cardsettings_t cardsettings, bool issolver, MTRand::uint32 randseed ) :
	myparams(cardsettings),
	cardsi_max(4, -1),
	binfiles(4, (PackedBinFile*)NULL),
	boardbinfiles(4, (PackedBinFile*)NULL),
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

		if(myparams.useflopalyzer && ( gr != FLOP || myparams.bin_max[FLOP] != INDEX23_MAX ) ) //multiply in flopalyzer
			cardsi_max[gr] *= FLOPALYZER_MAX[gr];

		if(myparams.useboardbins && gr >= FLOP && ( gr != FLOP || myparams.bin_max[FLOP] != INDEX23_MAX ) ) //multiply in board bins
			for(int boardi = FLOP; boardi <= gr; boardi ++ )
				cardsi_max[gr] *= myparams.board_bin_max[boardi];

		//if there are board bins, proeed to open them

		if( myparams.useboardbins )
		{
			if((myparams.boardbinsfilename[gr].length() != 0 && myparams.boardbinsfilesize[gr] == 0) 
					|| (myparams.boardbinsfilename[gr].length() == 0 && myparams.boardbinsfilesize[gr] != 0))
				REPORT("you have a board bin file but it's size is set to zero");
			if( myparams.boardbinsfilesize[gr] != 0 )
			{
				if(solving) //don't print unless guaranteed have console (i.e. solving)
					cout << "loading " << myparams.boardbinsfilename[gr] << " into memory..." << endl;

				//so ugly to do this in the loop...
				int64 index_max = 0;
				if(gr==PREFLOP)
					REPORT( "loading board bins on preflop..." );
				else if(gr==FLOP)
					index_max = INDEX3_MAX;
				else if(gr==TURN)
					index_max = INDEX31_MAX;
				else
					index_max = INDEX311_MAX;

				//if solving or under 5 megs, preload the file
				boardbinfiles[gr] = new PackedBinFile(myparams.boardbinsfilename[gr], myparams.boardbinsfilesize[gr], 
						myparams.board_bin_max[gr], index_max,
						(solving || (myparams.boardbinsfilesize[gr] != -1 && myparams.boardbinsfilesize[gr] < 5 * 1024 * 1024)));
			}
		}

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
				index_max = INDEX231_MAX;
			else if( is2311( myparams.filesize[ gr ] ) )
				index_max = INDEX2311_MAX;
			else
				index_max = INDEX25_MAX;

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
	{
		delete binfiles[gr];
		delete boardbinfiles[gr];
	}
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

	if(myparams.usehistory) //combine a rounds' bins with all previous rounds bins
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
	else //do not combine in any previous bins
	{
		cardsi[PREFLOP][P0] = getindex2(priv[P0]);
		cardsi[PREFLOP][P1] = getindex2(priv[P1]);

		if( myparams.bin_max[FLOP] == INDEX23_MAX )
		{
			cardsi[FLOP][P0] = getindex23(priv[P0], board[FLOP]);
			cardsi[FLOP][P1] = getindex23(priv[P1], board[FLOP]);
		}
		else
		{
			cardsi[FLOP][P0] = binfiles[FLOP]->retrieve(getindex23(priv[P0], board[FLOP]));
			cardsi[FLOP][P1] = binfiles[FLOP]->retrieve(getindex23(priv[P1], board[FLOP]));
		}

		cardsi[TURN][P0] = binfiles[TURN]->retrieve(getindex231(priv[P0], board[FLOP], board[TURN]));
		cardsi[TURN][P1] = binfiles[TURN]->retrieve(getindex231(priv[P1], board[FLOP], board[TURN]));

		cardsi[RIVER][P0] = binfiles[RIVER]->retrieve(
				is2311( myparams.filesize[ RIVER ] ) ?
				getindex2311(priv[P0], board[FLOP], board[TURN], board[RIVER]) :
				getindex25(priv[P0], board[FLOP], board[TURN], board[RIVER])
				);
		cardsi[RIVER][P1] = binfiles[RIVER]->retrieve(
				is2311( myparams.filesize[ RIVER ] ) ?
				getindex2311(priv[P1], board[FLOP], board[TURN], board[RIVER]) :
				getindex25(priv[P1], board[FLOP], board[TURN], board[RIVER])
				);
	}

	//insert the flopalyzer score in-place if requested

	if(myparams.useflopalyzer)
	{
		int flopscore = flopalyzer(board[FLOP]);
		int turnscore = turnalyzer(board[FLOP], board[TURN]);
		int riverscore = rivalyzer(board[FLOP], board[TURN], board[RIVER]);

		if( myparams.bin_max[FLOP] != INDEX23_MAX )
		{
			cardsi[FLOP][P0] = combine(cardsi[FLOP][P0], flopscore, FLOPALYZER_MAX[FLOP]);
			cardsi[FLOP][P1] = combine(cardsi[FLOP][P1], flopscore, FLOPALYZER_MAX[FLOP]);
		}

		cardsi[TURN][P0] = combine(cardsi[TURN][P0], turnscore, FLOPALYZER_MAX[TURN]);
		cardsi[TURN][P1] = combine(cardsi[TURN][P1], turnscore, FLOPALYZER_MAX[TURN]);

		cardsi[RIVER][P0] = combine(cardsi[RIVER][P0], riverscore, FLOPALYZER_MAX[RIVER]);
		cardsi[RIVER][P1] = combine(cardsi[RIVER][P1], riverscore, FLOPALYZER_MAX[RIVER]);
	}

	//insert the board bins in-place if requested, board bins always remember history

	if( myparams.useboardbins )
	{
		int flopbin = boardbinfiles[FLOP]->retrieve(getindex3(board[FLOP]));
		int turnbin = boardbinfiles[TURN]->retrieve(getindex31(board[FLOP], board[TURN]));
		int riverbin = boardbinfiles[RIVER]->retrieve(getindex311(board[FLOP], board[TURN], board[RIVER]));
		
		const int* const &binmax = myparams.board_bin_max; //alias binmax for shorter lines

		if( myparams.bin_max[FLOP] != INDEX23_MAX )
		{
			cardsi[FLOP][P0] = combine(cardsi[FLOP][P0], flopbin, binmax[FLOP]);
			cardsi[FLOP][P1] = combine(cardsi[FLOP][P1], flopbin, binmax[FLOP]);
		}

		cardsi[TURN][P0] = combine(cardsi[TURN][P0],   turnbin, binmax[TURN],   flopbin, binmax[FLOP]);
		cardsi[TURN][P1] = combine(cardsi[TURN][P1],   turnbin, binmax[TURN],   flopbin, binmax[FLOP]);

		cardsi[RIVER][P0] = combine(cardsi[RIVER][P0],                riverbin, binmax[RIVER],   
				                    turnbin, binmax[TURN],   flopbin, binmax[FLOP]);
		cardsi[RIVER][P1] = combine(cardsi[RIVER][P1],                riverbin, binmax[RIVER],   
				                    turnbin, binmax[TURN],   flopbin, binmax[FLOP]);
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
int CardMachine::getindices(int gr, const vector<CardMask> &cards, vector<int> &handi, vector<int> &boardi)
{
	handi.resize( myparams.usehistory ? gr+1 : 1 );
	boardi.resize( ( myparams.useflopalyzer | myparams.useboardbins ) * gr );
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
	else //not using history
	{
		//my old system, cardsi is determined simply by the bin for that round
		//so look up the appropriate bin and set it to handi and cardsi

		switch(gr)
		{
			case PREFLOP:
				cardsi = handi[0] = getindex2(cards[PREFLOP]);
				break;
			case FLOP:
				if( myparams.bin_max[FLOP] == INDEX23_MAX )
					cardsi = handi[0] = getindex23(cards[PREFLOP],cards[FLOP]);
				else
					cardsi = handi[0] = binfiles[FLOP]->retrieve(getindex23(cards[PREFLOP],cards[FLOP]));
				break;
			case TURN:
				cardsi = handi[0] = binfiles[TURN]->retrieve(getindex231(cards[PREFLOP],cards[FLOP],cards[TURN]));
				break;
			case RIVER:
				cardsi = handi[0] = binfiles[RIVER]->retrieve(
						is2311( myparams.filesize[ RIVER ] ) ?
						getindex2311(cards[PREFLOP],cards[FLOP],cards[TURN],cards[RIVER]) :
						getindex25(cards[PREFLOP],cards[FLOP],cards[TURN],cards[RIVER])
						);
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
			case RIVER: 
				boardi[ 2 ] = rivalyzer(cards[FLOP], cards[TURN], cards[RIVER]);
				boardi[ 1 ] = turnalyzer(cards[FLOP], cards[TURN]);
				boardi[ 0 ] = flopalyzer(cards[FLOP]);
				cardsi = combine(cardsi, boardi[ 2 ], FLOPALYZER_MAX[gr]);
				break;
			case TURN: 
				boardi[ 1 ] = turnalyzer(cards[FLOP], cards[TURN]);
				boardi[ 0 ] = flopalyzer(cards[FLOP]);
				cardsi = combine(cardsi, boardi[ 1 ], FLOPALYZER_MAX[gr]);
				break;
			case FLOP: 
				if( myparams.bin_max[FLOP] == INDEX23_MAX )
					boardi[ 0 ] = 0;
				else
				{
					boardi[ 0 ] = flopalyzer(cards[FLOP]);
					cardsi = combine(cardsi, boardi[ 0 ], FLOPALYZER_MAX[gr]);
				}
				break;
			case PREFLOP: 
				break;
			default: 
				REPORT("bad gr");
		}
	}

	//if we are using board bins, then combine in those scores as well in the same way as in getnewgame()

	if( myparams.useboardbins )
	{
		const int* const &binmax = myparams.board_bin_max; //alias binmax for shorter lines
		switch(gr)
		{
			case RIVER: 
				boardi[ 2 ] = boardbinfiles[RIVER]->retrieve(getindex311( cards[FLOP], cards[TURN], cards[RIVER]) );
				boardi[ 1 ] = boardbinfiles[TURN]->retrieve(getindex31(cards[FLOP], cards[TURN]));
				boardi[ 0 ] = boardbinfiles[FLOP]->retrieve(getindex3(cards[FLOP]));
				cardsi = combine(cardsi,  boardi[ 2 ], binmax[RIVER],  boardi[ 1 ], binmax[TURN],  boardi[ 0 ], binmax[FLOP]);
				break;
			case TURN: 
				boardi[ 1 ] = boardbinfiles[TURN]->retrieve(getindex31(cards[FLOP], cards[TURN]));
				boardi[ 0 ] = boardbinfiles[FLOP]->retrieve(getindex3(cards[FLOP]));
				cardsi = combine(cardsi,  boardi[ 1 ], binmax[TURN],  boardi[ 0 ], binmax[FLOP]);
				break;
			case FLOP: 
				if( myparams.bin_max[FLOP] == INDEX23_MAX )
					boardi[ 0 ] = 0;
				else
				{
					boardi[ 0 ] = boardbinfiles[FLOP]->retrieve(getindex3(cards[FLOP]));
					cardsi = combine(cardsi, boardi[ 0 ], binmax[FLOP]);
				}
				break;
			case PREFLOP: 
				break;
			default: 
				REPORT("bad gr");
		}
	}

	return cardsi;
}	


void CardMachine::findexamplehand(int gr, const vector<int> &handi, vector<int> &boardi, vector<CardMask> &cards)
{

	cards.resize(gr+1);

	//alias rand for the macros

	MTRand &mersenne = randforexamplehands;

	if(myparams.usehistory && ! myparams.useboardbins) //cprg system
	{
		//ensure our vectors are only as big as needed 
		if(handi.size() != (unsigned)gr+1) REPORT("handi is wrong size");
		if(boardi.size() != ( myparams.useflopalyzer * (unsigned)gr ) ) REPORT("boardi is wrong size");

		CardMask usedcards;
		//the story of findcount: if we randomly choose all hands at once, check the indices, 
		// and then randomly choose them again, it is slow (looking for 1 in 10000 that way),
		// if we do it incrementally, like here, only looking for 1 in 10 at each step, but
		// for some preflop/flop/turn combos, NO river card satisfies it. This is what works.
		int findcount;
restarthistory:
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
				goto restarthistory;
			MONTECARLO_N_CARDS_D(cards[FLOP], usedcards, 3, 1, );
		}
		while((myparams.useflopalyzer && flopalyzer(cards[FLOP]) != boardi[ 0 ])
				|| binfiles[FLOP]->retrieve(getindex23(cards[PREFLOP], cards[FLOP])) != handi[FLOP]);

		if(gr==FLOP) return;
		CardMask_OR(usedcards, usedcards, cards[FLOP]);

		findcount = 0;
		do
		{
			if(++findcount == 150)
				goto restarthistory;
			MONTECARLO_N_CARDS_D(cards[TURN], usedcards, 1, 1, );
		}
		while((myparams.useflopalyzer && turnalyzer(cards[FLOP],cards[TURN]) != boardi[ 1 ])
				|| binfiles[TURN]->retrieve(getindex231(cards[PREFLOP], cards[FLOP], cards[TURN])) != handi[TURN]);

		if(gr==TURN) return;
		CardMask_OR(usedcards, usedcards, cards[TURN]);

		findcount = 0;
		do
		{
			if(++findcount == 150)
				goto restarthistory;
			MONTECARLO_N_CARDS_D(cards[RIVER], usedcards, 1, 1, );
		}
		while((myparams.useflopalyzer && rivalyzer(cards[FLOP],cards[TURN],cards[RIVER]) != boardi[ 2 ])
				|| binfiles[RIVER]->retrieve(getindex2311(cards[PREFLOP], cards[FLOP], cards[TURN], cards[RIVER])) != handi[RIVER]);

		if(gr==RIVER) return;
	}
	// my own binning system
	else if( ! myparams.usehistory && myparams.useboardbins && ! myparams.useflopalyzer )
	{
		//ensure our vectors are only as big as needed 
		if(handi.size() != 1) REPORT("handi is wrong size");
		if(boardi.size() != (unsigned)gr) REPORT("boardi is wrong size");
		CardMask usedcards;

		if( gr == FLOP && myparams.bin_max[FLOP] == INDEX23_MAX )
		{
			CardMask_RESET(usedcards);
			do
			{
				MONTECARLO_N_CARDS_D(cards[FLOP], usedcards, 3, 1, );
				MONTECARLO_N_CARDS_D(cards[PREFLOP], cards[FLOP], 2, 1, );
			}
			while( getindex23( cards[ PREFLOP ], cards[ FLOP ] ) != handi[ 0 ] );
			return;
		}

		int findcount;

restartboard:
		CardMask_RESET(usedcards);
		if(gr==PREFLOP) goto findpreflop;

		do
		{
			MONTECARLO_N_CARDS_D(cards[FLOP], usedcards, 3, 1, );
		}
		while(boardbinfiles[FLOP]->retrieve(getindex3(cards[FLOP])) != boardi[0]);

		usedcards = cards[FLOP];
		if(gr==FLOP) goto findpreflop;

		findcount = 0;
		do
		{
			if(++findcount == 150)
				goto restartboard;
			MONTECARLO_N_CARDS_D(cards[TURN], usedcards, 1, 1, );
		}
		while(boardbinfiles[TURN]->retrieve(getindex31(cards[FLOP], cards[TURN])) != boardi[1]);

		CardMask_OR(usedcards, usedcards, cards[TURN]);
		if(gr==TURN) goto findpreflop;

		findcount = 0;
		do
		{
			if(++findcount == 150)
				goto restartboard;
			MONTECARLO_N_CARDS_D(cards[RIVER], usedcards, 1, 1, );
		}
		while(boardbinfiles[RIVER]->retrieve(getindex311(cards[FLOP], cards[TURN], cards[RIVER])) != boardi[2]);

		CardMask_OR(usedcards, usedcards, cards[RIVER]);

findpreflop:
		//finally we must find a preflop hand
		findcount = 0;
		while( true )
		{
			if(++findcount == 150)
				goto restartboard;
			MONTECARLO_N_CARDS_D(cards[PREFLOP], usedcards, 2, 1, );
			switch( gr )
			{
				case PREFLOP:
					if( handi[0] == getindex2( cards[PREFLOP] ) )
						return;
					break;
				case FLOP:
					if( handi[0] == binfiles[gr]->retrieve( getindex23( cards[PREFLOP], cards[FLOP] ) ) )
						return;
					break;
				case TURN:
					if( handi[0] == binfiles[gr]->retrieve( getindex231( cards[PREFLOP], cards[FLOP], cards[TURN] ) ) )
						return;
					break;
				case RIVER:
					if( handi[0] == binfiles[gr]->retrieve( 
							is2311( myparams.filesize[ RIVER ] ) ?
							getindex2311( cards[PREFLOP], cards[FLOP], cards[TURN], cards[RIVER] ) :
							getindex25( cards[PREFLOP], cards[FLOP], cards[TURN], cards[RIVER] ) 
							) )
						return;
					break;
				default:
					REPORT("bad gr");
			}
		}
	}
	else
		REPORT("example hands is not written for the settings this solve uses");

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

namespace //used in makecardsettings
{
	int getnumbins( string binname )
	{
		const size_t xpos = binname.find( "x" );
		if( xpos == string::npos )
			return fromstr< int >( binname );
		else
			return fromstr< int >( binname.substr( 0, xpos ) )
				 * fromstr< int >( binname.substr( xpos + 1 ) );
	}
}
// static function used to replace the code in solveparams.h with code that can
// be used at runtime (old code in solveparams.h generated a cardsettings_t
// at compiletime
cardsettings_t CardMachine::makecardsettings( 
		string pfbin, string fbin, string tbin, string rbin,
		int boardfbin, int boardtbin, int boardrbin,
		bool usehistory, bool useflopalyzer, bool useboardbins )
{
	if( pfbin == "all" || pfbin == "perfect" )
		pfbin = tostr( INDEX2_MAX );
	if( fbin == "all" || fbin == "perfect" )
		fbin = tostr( INDEX23_MAX );

	int pfbinnum = getnumbins( pfbin );
	int fbinnum = getnumbins( fbin );
	int tbinnum = getnumbins( tbin );
	int rbinnum = getnumbins( rbin );

	if( usehistory == true )
	{
		cardsettings_t historyretval =
		{
			{ pfbinnum, fbinnum, tbinnum, rbinnum },
			{
				"bins/preflop"+pfbin,
				"bins/flop"+pfbin+"-"+fbin,
				"bins/turn"+pfbin+"-"+fbin+"-"+tbin,
				"bins/river"+pfbin+"-"+fbin+"-"+tbin+"-"+rbin,
			},
			{
				PackedBinFile::numwordsneeded(pfbinnum, INDEX2_MAX)*8,
				PackedBinFile::numwordsneeded(fbinnum, INDEX23_MAX)*8,
				PackedBinFile::numwordsneeded(tbinnum, INDEX231_MAX)*8,
				PackedBinFile::numwordsneeded(rbinnum, INDEX2311_MAX)*8
			},
			true, //use history
			useflopalyzer, //use flopalyzer
			useboardbins, //use board bins
			{ 0, boardfbin, boardtbin, boardrbin },
			{
				"",
				"bins/flopb" + tostr( boardfbin ),
				"bins/turnb" + tostr( boardfbin ) + '-' + 'b' + tostr( boardtbin ),
				"bins/riverb" + tostr( boardfbin ) + '-' + 'b' + tostr( boardtbin ) + '-' + 'b' + tostr( boardrbin )
			},
			{
				0,
				PackedBinFile::numwordsneeded(boardfbin, INDEX3_MAX)*8,
				PackedBinFile::numwordsneeded(boardtbin, INDEX31_MAX)*8,
				PackedBinFile::numwordsneeded(boardrbin, INDEX311_MAX)*8
			}
		};

		return historyretval;

	}
	else // ( usehistory == false )
	{
		if( pfbinnum != INDEX2_MAX )
			REPORT( "Bad value for pfbinnum when not using history. (should be max, 169)" );

		const string riverfile = "bins/riverb" + tostr( boardfbin ) + '-' + 'b' + tostr( boardtbin ) + '-' + 'b' + tostr( boardrbin ) + '-' + rbin;
		const bool bigfile = is2311( boost::filesystem::file_size( riverfile + ".bins" ) );

		cardsettings_t nohistoryretval =
		{
			{ INDEX2_MAX, fbinnum, tbinnum, rbinnum },
			{
				"",
				( fbinnum == INDEX23_MAX ? "" : "bins/flopb" + tostr( boardfbin ) + '-' + fbin ),
				"bins/turnb" + tostr( boardfbin ) + '-' + 'b' + tostr( boardtbin ) + '-' + tbin,
				riverfile
			},
			{
				0,
				( fbinnum == INDEX23_MAX ? 0 : PackedBinFile::numwordsneeded(fbinnum, INDEX23_MAX)*8 ),
				PackedBinFile::numwordsneeded(tbinnum, INDEX231_MAX)*8,
				PackedBinFile::numwordsneeded(rbinnum, bigfile ? INDEX2311_MAX : INDEX25_MAX)*8
			},
			false, //use history
			useflopalyzer, //use flopalyzer
			useboardbins, //use board bins
			{ 0, boardfbin, boardtbin, boardrbin },
			{
				"",
				"bins/flopb" + tostr( boardfbin ),
				"bins/turnb" + tostr( boardfbin ) + '-' + 'b' + tostr( boardtbin ),
				"bins/riverb" + tostr( boardfbin ) + '-' + 'b' + tostr( boardtbin ) + '-' + 'b' + tostr( boardrbin )
			},
			{
				0,
				PackedBinFile::numwordsneeded(boardfbin, INDEX3_MAX)*8,
				PackedBinFile::numwordsneeded(boardtbin, INDEX31_MAX)*8,
				PackedBinFile::numwordsneeded(boardrbin, INDEX311_MAX)*8
			}
		};

		return nohistoryretval;
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
