#include "stdafx.h"
#include "gamestate.h"
#include "flopalyzer.h"
#include "determinebins.h"
#include "treenolimit.h"
#include "constants.h"

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

	//compute flopalyzer scores
	flopscore = flopalyzer(flop);
	turnscore = turnalyzer(flop, turn);
	riverscore = rivalyzer(flop, turn, river);

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
