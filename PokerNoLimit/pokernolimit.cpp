#include "stdafx.h"
#include "flopalyzer.h"
using namespace std;

//used in code below.
#define PREFLOP 0
#define FLOP 1
#define TURN 2
#define RIVER 3

//number of info nodes in a single betting tree.
#define BETI_MAX N_NODES //from TreesBetting
#define BETI_9CUTOFF 17 //before this all have max 9 actions, after have max 3
#define BETI_MAX9 BETI_9CUTOFF
#define BETI_MAX3 (BETI_MAX-BETI_9CUTOFF)
#define SCENI_MAX 500
#define HANDSTRI_MAX 30 //number of different variations in 

//current game state is stored here. [2] always refers to player number here.
//These are all chosen or computed at the beginning of an iteration.
CardMask hand[2], flop, turn, river;
int handid[2]; //from 0-168
int handstri[2][3]; // what we know about our hand at the flop, turn, and river

//this will be a large array read from a precomputed file.
float prob0wins;

//Current strategy, Average strategy, Accumulated total regret:
//separated based on max actions to save memory, allocated in main.
float * strategy9, * strategy3;
float * strategysumnum9, * strategysumnum3; 
float * strategysumden9, * strategysumden3;
float * regret9, * regret3;

//takes history of beti's, with the gameround, combines it with the (global) 
// hand information, and turns it all into a compact signature. The preflop 
// scenario index is ALWAYS 0
//I believe this should keep exact track of the pot, to ensure valid a's are valid.
static inline int getsceni(int gr, int pastbeti[3])
{
	switch (pastbeti[gr-1])
	{
	case 0:
	case 1:
	case 2:
		return 2;
	default:
		return 1;
	}
}

//walker - Operates on a GIVEN game node, and then recursively walks the children nodes
// Needs to know where the fuck it is.
// beti[] contains the current betting node in beti[gr] and past ones in beti[x] for x < gr
float walker(int gr, int pot, int pastbeti[3], int beti, float prob0, float prob1)
{
	float totalregret=0;
	float utility[9]; //where 9 is max actions for a node.
	float avgutility=0;
	betnode const * mynode;
	float * strat, * stratsumnum, * stratsumden, * regret;
	float stratmaxa=0; //this is the one that I do not store in the global data arrays.
	int myhandid; //my hand ID
	int sceni; //my scenario index
	int maxa; //just the size of the action arrays
	int numa=0; //the number of actually possible actions
	bool isvalida[9]; //if this action is possible (have enough money)
	
	//FIRST LETS FIND THE DATA WE NEED FOR THIS INSTANCE OF WALKER

	//obtain pointer to correct betting node.
	if (gr==PREFLOP) mynode = &(pfn[beti]);
	else             mynode = &(n[beti]);

	//obtain correct hand id.
	myhandid = handid[mynode->playertoact];

	//obtain correct scenario index.
	sceni = getsceni(gr, pastbeti);

	//obtain the correct maximum possible actions from this node.
	maxa = mynode->numacts;

	//obtain the correct segments of the global data arrays
	if (beti<BETI_9CUTOFF)
	{
		//and these are now pointers to arrays of 8 (for strat) or 9 (for regret) elements
		assert(maxa > 3 && maxa <= 9); //this would mean problem with betting tree
		//Essentially, I am implementing multidimensional arrays here. The first 3 have 8
		//actions in the last dimension, the last has 9.
		strat       = strategy9       + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX9 + beti)*8;
		stratsumnum = strategysumnum9 + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX9 + beti)*8;
		stratsumden = strategysumden9 + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX9 + beti)*8;
		regret      = regret9         + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX9 + beti)*9;
	}
	else
	{
		//and these are now pointers to arrays of 2 (for strat) or 3 (for regret) elements
		assert(maxa <= 3); //this would mean problem with betting tree
		//The first 3 have 2 actions in the last dimension, the last has 3.
		strat       = strategy3       + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX3 + beti)*2;
		stratsumnum = strategysumnum3 + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX3 + beti)*2;
		stratsumden = strategysumden3 + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX3 + beti)*2;
		regret      = regret3         + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX3 + beti)*3;
	}


	// DEPENDING ON STACKSIZE, AND GAME HISTORY, NOT ALL ACTIONS MAY BE POSSIBLE FROM
	// THIS NODE! SINCE I AM NOT STORING THE ENTIRE GAME TREE, AND ONLY HAVE ONE INSTANCE
	// OF THE BETTING TREE IN MEMORY FOR _ALL_ SITUATIONS, WE NEED TO DYNAMICALLY CHECK
	// HERE TO SEE WHICH ACTIONS CAN BE AFFORDED.

	for(int a=0; a<maxa; a++)
	{
		isvalida[a] = (pot + mynode->need[a] < stacksize[mynode->playertoact]);
		if (isvalida[a])
			numa++;
	}


	// OK, NOW WE WILL APPLY EQUATION (8) FROM TECH REPORT, COMPUTE STRATEGY T+1
	
	for(int a=0; a<maxa; a++)
	{
		totalregret += max((float)0,regret[a]); //non-valid a's will simply have regret still at 0.
	}

	if (totalregret > 0)
	{
		for(int a=0; a<maxa-1; a++) 
			stratmaxa += strat[a] = max((float)0,regret[a]) / totalregret;
		stratmaxa = 1-stratmaxa;
	}
	else
	{
		for(int a=0; a<maxa-1; a++) 
			strat[a] = (float)1/numa;
		stratmaxa = (float)1/numa;
	}


	//NOW WE WILL UPDATE THE AVERAGE STRATEGY, EQUATION (4) FROM TECH REPORT

	if(mynode->playertoact==0)
	{
		for(int a=0; a<maxa-1; a++)
		{
			stratsumnum[a] += prob0 * strat[a];
			stratsumden[a] += prob0;
		}
	}
	else
	{
		for(int a=0; a<maxa-1; a++)
		{
			stratsumnum[a] += prob1 * strat[a];
			stratsumden[a] += prob1;
		}
	}


	//NOW, WE WANT TO FIND THE UTILITY OF EACH ACTION. 
	//WE OFTEN DO THIS BY CALLING WALKER RECURSIVELY.

	for(int a=0; a<maxa; a++)
	{
		// because I only store the first N-1 in memory:
		float st = (a==maxa-1) ? stratmaxa : strat[a]; 


		//if we do not have enough money, we do not touch this action. It does not exist.
		if (!isvalida[a])
			continue;


		// CONTINUE WITHIN THIS GAME ROUND
		if (mynode->result[a] == NA)
		{
			assert(mynode->child[a] != NA); //this would mean an error in the betting tree.

			if(mynode->playertoact==0)
				utility[a] = walker(gr, pot, pastbeti, mynode->child[a], prob0*st, prob1);
			else
				utility[a] = walker(gr, pot, pastbeti, mynode->child[a], prob0, prob1*st);
		}

		// SHOWDOWN - from all-in call!
		else if(mynode->result[a] == AI)
		{   
			assert(mynode->child[a] == NA); //this would mean an error in the betting tree.

			utility[a] = min(stacksize[0], stacksize[1]) * (2*prob0wins-1);
		}

		// SHOWDOWN - from the river!
		else if(mynode->result[a] == GO && gr == RIVER)
		{
			assert(mynode->child[a] == NA); //this would mean an error in the betting tree.

			utility[a] = (pot+mynode->potcontrib[a]) * (2*prob0wins-1);
		}

		// CONTINUE AT NEXT GAME ROUND
		else if(mynode->result[a] == GO)
		{
			assert(mynode->child[a] == NA); //this would mean an error in the betting tree.
			pastbeti[gr]=beti; //hitherto we have not needed pastbeti[gr], but now we do. Array access ok as gr != RIVER

			if(mynode->playertoact==0)
				utility[a] = walker(gr+1, pot+mynode->potcontrib[a], pastbeti, 0, prob0*st, prob1);
			else
				utility[a] = walker(gr+1, pot+mynode->potcontrib[a], pastbeti, 0, prob0, prob1*st);
		}

		// FOLD
		else
		{
			assert(mynode->child[a] == NA); //this would mean an error in the betting tree.
			assert(mynode->result[a]==0 || mynode->result[a]==1);

			utility[a] = (pot+mynode->potcontrib[a]) * (1 - 2*mynode->result[a]);
		}


		//keep a running total of the EV of utility under current strat.
		avgutility += st*utility[a];
	}


	//NOW, WE WISH TO UPDATE THE REGRET TOTALS. THIS IS DONE IN ACCORDANCE WITH
	//EQUATION 3.5 (I THINK) FROM THE THESIS.

	if (mynode->playertoact==0) //P0 playing, use prob1, proability of player 1 getting here.
	{
		for(int a=0; a<maxa; a++)
		{
			regret[a] += prob1 * (utility[a] - avgutility);
		}
	}
	else // P1 playing, so his regret values are negative of P0's regret values.
	{
		for(int a=0; a<maxa; a++)
		{
			regret[a] += - prob0 * (utility[a] - avgutility);
		}
	}


	//FINALLY, OUR LAST ACTION. WE WILL RETURN THE AVERAGE UTILITY, UNDER THE COMPUTED STRAT
	//SO THAT THE CALLER CAN KNOW THE EXPECTED VALUE OF THIS PATH. GOD SPEED CALLER.

	return avgutility;
}



//THIS IS A SIMPLE FUNCTION TO COMPUTE THE HANDID OF A PAIR OF HOLE CARDS.
//THE RANGE OF THIS FUNCTION IS 0-168. THE DOMAIN OF THE INPUTS ARE 0-51.
int computehandid(CardMask * hand)
{
	int bigrank, smlrank, cards[2];

	//this function is from pokereval.
	StdDeck.maskToCards(hand, cards);

	//detect pairs by comparing ranks
	if (cards[0]%13 == cards[1]%13)
		return (cards[0]%13)*14; //this fills the diagonal of the table

	if(cards[0]%13 > cards[1]%13)
	{
		bigrank = cards[0]%13;
		smlrank = cards[1]%13;
	}
	else
	{
		bigrank = cards[1]%13;
		smlrank = cards[0]%13;
	}
	
	if (cards[0]/13 == cards[1]/13) //then it's suited, integer arithmetic
		return bigrank*13+smlrank; //one off-diagonal triangle
	else
		return smlrank*13+bigrank; //the other off-diagonal triangle
}



//THIS IS A SIMPLE FUNCTION TO RADOMLY DISTRIBUTE CARDS INTO THE GLOBAL CARDMASK
//VARIABLES.
void dealcards()
{
	CardMask usedcards;
	CardMask_RESET(usedcards);

	MONTECARLO_N_CARDS_D(hand[0], usedcards, 2, 1, );
	CardMask_OR(usedcards, usedcards, hand[0]);

	MONTECARLO_N_CARDS_D(hand[1], usedcards, 2, 1, );
	CardMask_OR(usedcards, usedcards, hand[1]);

	MONTECARLO_N_CARDS_D(flop, usedcards, 3, 1, );
	CardMask_OR(usedcards, usedcards, flop);

	MONTECARLO_N_CARDS_D(turn, usedcards, 1, 1, );
	CardMask_OR(usedcards, usedcards, turn);

	MONTECARLO_N_CARDS_D(river, usedcards, 1, 1, );
}

//------------------------ p l a y g a m e -------------------------//
void playgame()
{
	//allocate memory!
	strategy9       = new float[169*SCENI_MAX*BETI_MAX9*8];
	strategysumnum9 = new float[169*SCENI_MAX*BETI_MAX9*8];
	strategysumden9 = new float[169*SCENI_MAX*BETI_MAX9*8];
	regret9         = new float[169*SCENI_MAX*BETI_MAX9*9];

	strategy3       = new float[169*SCENI_MAX*BETI_MAX3*2];
	strategysumnum3 = new float[169*SCENI_MAX*BETI_MAX3*2];
	strategysumden3 = new float[169*SCENI_MAX*BETI_MAX3*2];
	regret3         = new float[169*SCENI_MAX*BETI_MAX3*3];

	//and initialize to 0.
	memset(strategy9,       0, 169*SCENI_MAX*BETI_MAX9*8*sizeof(float));
	memset(strategysumnum9, 0, 169*SCENI_MAX*BETI_MAX9*8*sizeof(float));
	memset(strategysumden9, 0, 169*SCENI_MAX*BETI_MAX9*8*sizeof(float));
	memset(regret9,         0, 169*SCENI_MAX*BETI_MAX9*9*sizeof(float));

	memset(strategy3,       0, 169*SCENI_MAX*BETI_MAX3*2*sizeof(float));
	memset(strategysumnum3, 0, 169*SCENI_MAX*BETI_MAX3*2*sizeof(float));
	memset(strategysumden3, 0, 169*SCENI_MAX*BETI_MAX3*2*sizeof(float));
	memset(regret3,         0, 169*SCENI_MAX*BETI_MAX3*3*sizeof(float));
	
	//next, we want to read the showdown ev data from our precomputed file
	prob0wins = 0.5;

	for(int i=0; i<1000; i++)
	{
		dealcards();
		handid[0] = computehandid(&(hand[0]));
		handid[1] = computehandid(&(hand[1]));
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	testflopalyzer();

	system("PAUSE");
	return 0;
}

