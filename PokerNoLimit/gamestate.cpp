#include "stdafx.h"
#include "gamestate.h"
#include "treenolimit.h"

#define COMBINE(i4, i3, i2, i1, n3, n2, n1) ((i4)*(n3)*(n2)*(n1) + (i3)*(n2)*(n1) + (i2)*(n1) + (i1))

int GameState::getscenarioi(int gr, int player, int pot, int bethist[3])
{
	//**************************
	// sceni is created from;
	// flop score (computed once per game, same for both players)
	// hand score (computed once per game, depends on which player)
	// pot size (depends on game state, same for both players)
	// betting history (depends on game state, same for both players)

	// flop score and hand score can be stored in the gamestate class
	// and retrieved by passing in the specific gameround and playernumber

	// pot size is calculated only in walker, and passed in as an argument

	// betting history is determined by data in betting tree, and stored only in walker. 
	// it is passed in as an argument
	
	//PLAN:
	// - calculate a separate compact index based on gameround, and then add them all together

	switch(gr)
	{
	case PREFLOP:
		//flop score is non-existant
		//we have handscore
		//pot size is non-existant
		//betting history is non-existant
		// so, just handscore, and offset is zero
		return binnumber[player][PREFLOP];

	case FLOP:
		//flop score and handscore are stored here.
		//pot size index is defined from tree file
		//betting history we have
		return COMBINE(binnumber[player][FLOP], flopscore,      getpoti(gr,pot), bethist[0],
			                                    FLOPALYZER_MAX, POTI_FLOP_MAX,   BETHIST_MAX)
					+ SCENI_PREFLOP_MAX;

	case TURN:
		return COMBINE(binnumber[player][TURN], turnscore,      getpoti(gr,pot), bethist[1]*BETHIST_MAX + bethist[0],
			                                    TURNALYZER_MAX, POTI_TURN_MAX,   BETHIST_MAX*BETHIST_MAX)
					+ SCENI_PREFLOP_MAX + SCENI_FLOP_MAX;

	case RIVER:
		return COMBINE(binnumber[player][RIVER], riverscore,    getpoti(gr,pot), bethist[2]*BETHIST_MAX*BETHIST_MAX + bethist[1]*BETHIST_MAX + bethist[0],
			                                     RIVALYZER_MAX, POTI_RIVER_MAX,  BETHIST_MAX*BETHIST_MAX*BETHIST_MAX)
					+ SCENI_PREFLOP_MAX + SCENI_FLOP_MAX + SCENI_TURN_MAX;

	default:
		REPORT("ivalid gameround encountered in getscenarioi");
	}
}

void GameState::dealnewgame()
{
	CardMask usedcards, full0, full1;
	HandVal r0, r1;

	//deal out the cards randomly.
	CardMask_RESET(usedcards);
	MONTECARLO_N_CARDS_D(priv0, usedcards, 2, 1, );
	MONTECARLO_N_CARDS_D(priv1, priv0, 2, 1, );

	CardMask_OR(usedcards, priv0, priv1);
	MONTECARLO_N_CARDS_D(flop, usedcards, 3, 1, );

	CardMask_OR(usedcards, usedcards, flop);
	MONTECARLO_N_CARDS_D(turn, usedcards, 1, 1, );

	CardMask_OR(usedcards, usedcards, turn);
	MONTECARLO_N_CARDS_D(river, usedcards, 1, 1, );

	//compute flopalyzer score
	flopscore = analyzeflop(flop);
	turnscore = analyzeturn(flop, turn);
	riverscore = analyzeriver(flop, turn, river);

	//compute binnumbers
	determinebins(priv0, flop, turn, river, 
		binnumber[0][PREFLOP], binnumber[0][FLOP], binnumber[0][TURN], binnumber[0][RIVER]);
	determinebins(priv1, flop, turn, river, 
		binnumber[1][PREFLOP], binnumber[1][FLOP], binnumber[1][TURN], binnumber[1][RIVER]);

	//compute would-be winner, and we're done.
	CardMask_OR(full0, priv0, flop);
	CardMask_OR(full0, full0, turn);
	CardMask_OR(full0, full0, river);

	CardMask_OR(full1, priv1, flop);
	CardMask_OR(full1, full1, turn);
	CardMask_OR(full1, full1, river);

	r0=Hand_EVAL_N(full0, 7);
	r1=Hand_EVAL_N(full1, 7);

	if (r0>r1)
		twoprob0wins = 2;
	else if(r1>r0)
		twoprob0wins = 0;
	else
		twoprob0wins = 1;
}
