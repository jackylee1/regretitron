#include "stdafx.h"

#define FOLD -1
#define CHECK 0

#define SB 1 //small blind and big blind
#define BB 2

//GLOBAL GAMESTATE VARIABLES
CardMask p1cards;
CardMask p2cards;
CardMask flopcards;
CardMask turncard;
CardMask rivercard;

int pot;

int gameround;
#define PREFLOP 1
#define FLOP 2
#define TURN 3
#define RIVER 4


//GETACTION
//args: player number (1 or 2), step number within this betting round (1-4), amount already invested, amount asking)
//amount asking should always be greater than the amount invested, as it is the amount 'to go'.
//only exception to that is if both invested and asking are zero.
//return: -1 (or FOLD) to fold. Otherwise a number greater than or equal to asking.
//if asking is zero, we can return 0 (or CHECK).
//otherwise we can return FOLD.
//or we can return the amount we are willing to bet:
//if we return asking, then we are calling.
//if we return more than asking, then we are raising (if asking was not zero)
//or we are betting, (if asking was zero)
//Finally, if step == 4, we can only return check or fold
int getAction(int p, int step, int invested, int asking)
{
	int retval;

	//computer is 1
	if (p==1)
	{
		//raise 1/4 of time
		if (rand()%4 == 0)
		{
			retval = asking + 2 * (rand()%4+1);
			printf("I Bet %d.\n", retval);
			return retval;
		}

		//call 1/(addnl amount asking) of time
		if (asking == invested)
		{
			printf("I Check.\n");
			return asking;
		}
		
		if (rand()%(asking-invested) == 0)
		{
			printf("I Call.\n");
			return asking;
		}
		else
		{
			printf("I fold.\n");
			return FOLD;
		}
	}
	else
	//humanoid
	{
		int retval;
		do{
			printf("Pot is %d. I've wagered %d. You've bet %d this round.\n:", pot, asking, invested);
			scanf_s("%d",&retval);
		} while (!((asking != 0 && retval == FOLD) || retval >= asking));
		return retval;
	}
}


//PREFLOP BETTING ROUND
//return 0 for ending in call, 1 for p1 wins, 2 for p2 wins.
//set amt to amount won from this betting round, or amount added by each to pot
int preflopbettinground(int &amt)
{
	int act1, act2, act3, act4;

	//ROUND1
	act1 = getAction(2, 1, SB, BB);//player 2, acting 1st, invested 1, asking 2
	//P2 FOLDS
	if (act1 == FOLD)
	{
		amt = SB;
		return 1;
	}
	//P2 LIMPS
	if (act1 == BB)
	{
		
		//ROUND2
		act2 = getAction(1, 2, BB, BB);//player 1, acting 2nd, invested BB, asking BB
		//P1 CHECKS
		if (act2 == BB)
		{
			amt = BB;
			return 0;
		}
		//P1 BETS
		else
		{
			//ROUND3
			act3 = getAction(2, 3, BB, act2);//player 2, acting 3rd, invested BB, asking bet amount.
			//P2 FOLDS
			if (act3 == FOLD)
			{
				amt = BB;
				return 1;
			}
			//P2 CALLS
			else if (act3 == act2)
			{
				amt = act2;
				return 0;
			}
			//P2 RAISES!
			else
			{
				//ROUND4
				act4 = getAction(1, 4, act2, act3);//p1, act 4th, invested prev bet amt, asking total of new bet amt.
				//P1 FOLDS
				if (act4 == FOLD)
				{
					amt = act2;
					return 2;
				}
				//P1 MUST CALL (4th round)
				else
				{
					amt = act3;
					return 0;
				}
			}
		}
	}
	//P2 BETS
	else
	{
		//ROUND2
		act2 = getAction(1, 2, BB, act1);//player 1, acting 2nd, invested BB, asking p1 bet amount
		//P1 FOLDS
		if (act2 == FOLD)
		{
			amt = BB;
			return 2;
		}
		//P1 CALLS
		else if (act2 == act1)
		{
			amt = act1;
			return 0;
		}
		//P1 RAISES
		else
		{
			//ROUND3
			act3 = getAction(2, 3, act1, act2);//player 2, acting 3rd, invested 1st bet amt, asking 2nd bet amt.
			//P2 FOLDS
			if (act3 == FOLD)
			{
				amt = act1;
				return 1;
			}
			//P2 CALLS
			else if (act3 == act2)
			{
				amt = act2;
				return 0;
			}
			//P2 RE-RAISES!
			else
			{
				//ROUND4
				act4 = getAction(1, 4, act2, act3);//p1, act 4th, invested prev bet amt, asking total of new bet amt.
				//P1 FOLDS
				if (act4 == FOLD)
				{
					amt = act2;
					return 2;
				}
				//P1 MUST CALL (4th round)
				else
				{
					amt = act3;
					return 0;
				}
			}
		}
	}
	printf("MASSIVE ERROR!");
}



//POST FLOP BETTING ROUND
//return 0 for ending in call, 1 for p1 wins, 2 for p2 wins.
//set amt to amount won from this betting round, or amount added by each to pot
int bettinground(int &amt)
{
	int act1, act2, act3, act4;

	//ROUND1
	act1 = getAction(1, 1, 0, 0);//player 1, acting 1st, invested 0, asking 0
	//P1 CHECKS
	if (act1 == CHECK)
	{
		//ROUND2
		act2 = getAction(2, 2, 0, 0);//player 2, acting 2nd, invested 0, asking 0
		//P2 CHECKS
		if (act2 == CHECK)
		{
			amt = 0;
			return 0;
		}
		//P2 BETS
		else
		{
			//ROUND3
			act3 = getAction(1, 3, 0, act2);//player 1, acting 3rd, invested 0, asking bet amount.
			//P1 FOLDS
			if (act3 == FOLD)
			{
				amt = 0;
				return 2;
			}
			//P1 CALLS
			else if (act3 == act2)
			{
				amt = act2;
				return 0;
			}
			//P1 RAISES!
			else
			{
				//ROUND4
				act4 = getAction(2, 4, act2, act3);//p2, act 4th, invested prev bet amt, asking total of new bet amt.
				//P2 FOLDS
				if (act4 == FOLD)
				{
					amt = act2;
					return 1;
				}
				//P2 MUST CALL (4th round)
				else
				{
					amt = act3;
					return 0;
				}
			}
		}
	}
	//P1 BETS
	else
	{
		//ROUND2
		act2 = getAction(2, 2, 0, act1);//player 2, acting 2nd, invested 0, asking p1 bet amount
		//P2 FOLDS
		if (act2 == FOLD)
		{
			amt = 0;
			return 1;
		}
		//P2 CALLS
		else if (act2 == act1)
		{
			amt = act1;
			return 0;
		}
		//P2 RAISES
		else
		{
			//ROUND3
			act3 = getAction(1, 3, act1, act2);//player 1, acting 3rd, invested 1st bet amt, asking 2nd bet amt.
			//P1 FOLDS
			if (act3 == FOLD)
			{
				amt = act1;
				return 2;
			}
			//P1 CALLS
			else if (act3 == act2)
			{
				amt = act2;
				return 0;
			}
			//P1 RE-RAISES!
			else
			{
				//ROUND4
				act4 = getAction(2, 4, act2, act3);//p2, act 4th, invested prev bet amt, asking total of new bet amt.
				//P2 FOLDS
				if (act4 == FOLD)
				{
					amt = act2;
					return 1;
				}
				//P2 MUST CALL (4th round)
				else
				{
					amt = act3;
					return 0;
				}
			}
		}
	}
	printf("MASSIVE ERROR!");
}

//PLAY A GAME
int playgame()
{
	int bet;
	int winner;
	HandVal r1, r2; // for showdown only
	CardMask usedcards;

	//---DEAL ALL CARDS (computers don't cheat :)---
	pot=0;
	CardMask_RESET(usedcards);
	DECK_MONTECARLO_N_CARDS_D(Deck, p1cards, usedcards, 2, 1, );
	CardMask_OR(usedcards, usedcards, p1cards);
	DECK_MONTECARLO_N_CARDS_D(Deck, p2cards, usedcards, 2, 1, );
	CardMask_OR(usedcards, usedcards, p2cards);
	DECK_MONTECARLO_N_CARDS_D(Deck, flopcards, usedcards, 3, 1, );
	CardMask_OR(usedcards, usedcards, flopcards);
	DECK_MONTECARLO_N_CARDS_D(Deck, turncard, usedcards, 1, 1, );
	CardMask_OR(usedcards, usedcards, turncard);
	DECK_MONTECARLO_N_CARDS_D(Deck, rivercard, usedcards, 1, 1, );
	CardMask_OR(usedcards, usedcards, rivercard);

	//---PREFLOP---
	gameround = PREFLOP;
	printf("\nYou have: [");
	Deck_printMask(p2cards);
	printf("]\n");
	winner = preflopbettinground(bet);
	pot += bet;
	if (winner != 0) return winner;
	
	//---FLOP---
	gameround = FLOP;
	printf("\nBoard: [");
	Deck_printMask(flopcards);
	printf("]\n");
	winner = bettinground(bet);
	pot += bet;
	if (winner != 0) return winner;
	
	//---TURN---
	gameround = TURN;
	printf("\nBoard: [");
	Deck_printMask(flopcards);
	printf("] [");
	Deck_printMask(turncard);
	printf("]\n");
	winner = bettinground(bet);
	pot += bet;
	if (winner != 0) return winner;
	
	//---RIVER---
	gameround = RIVER;
	printf("\nBoard: [");
	Deck_printMask(flopcards);
	printf("] [");
	Deck_printMask(turncard);
	printf("] [");
	Deck_printMask(rivercard);
	printf("]\n");
	winner = bettinground(bet);
	pot += bet;
	if (winner != 0) return winner;
	
	//---SHOWDOWN---
	printf("\nI have ");
	Deck_printMask(p1cards);
	printf(".\n");
	CardMask_OR(flopcards, flopcards, turncard); //beware abuse of variable flopcards
	CardMask_OR(flopcards, flopcards, rivercard);
	CardMask_OR(p1cards, p1cards, flopcards); //beware abuse of variable p1cards
	CardMask_OR(p2cards, p2cards, flopcards); //beware abuse of variable p1cards
	
	r1 = Hand_EVAL_N(p1cards, 7);
	r2 = Hand_EVAL_N(p2cards, 7);

	if (r1 > r2) return 1;
	else if (r2 > r1) return 2;
	else return 0;
}


//MAIN
int _tmain(int argc, _TCHAR* argv[])
{
	int winner;
	printf("Welcome to Poker Bonanza!\nYou will be playing RandomBot.\nAll wager amounts are totals for that betting round.\n\n");

	//seed rand.
	srand((unsigned) time(NULL) / 2);

	while(1)
	{
		winner = playgame();

		if (winner == 0)
			printf("We tie, each don't lose %d\n", pot);
		else if (winner == 1)
			printf("You lose %d.\n", pot);
		else
			printf("You win %d.\n",pot);

		printf("\n\n\n");
	}

	return 0;
}

/*
queryprofile()
{
	if (gameround == PREFLOP)
	{
		if cards in group 1
			handbin = 1;
		...;
		else 
			handbin = 7;

for each hand bin (7)
//for a given chip stack (4)
for a given betting history before given round (1+2+3+3)
for a given (asking-invested) for each betting step, includes both positions (1+2+2+2)
we need call pct and bet amounts and probabilities (1+4) else fold.

=7*9*7*5
=2205 reals
hbin1 - hbin7

r11 : r-round-bettingclass
r21
r22
r31
r32
r33
r41
r42
r43

*/