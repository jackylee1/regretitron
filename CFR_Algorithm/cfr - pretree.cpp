// CFR_Algorithm.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace std;

//cards[i] - the dealt card, either 0, 1, or 2
// i is the player number
int cards[2];

//strategy[i][k] - probability of betting or calling, given the situation:
// i is the three possible cards, values as above// //j is the game round. 0 = preflop
// k is the betting scenario: 0 (1st act), 1(2nd act, no outstanding),
//							  2 (3rd act), or 3 (2nd act, outstanding bet)
double strategy[3][4]={0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};

//regret[i][k][a] - Total regret for each information node.
// i is three possible cards
// k is the betting scenario, same as above.
// a is the action taken. 0 = check/fold, 1 = bet/call
double regret[3][4][2]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* Need a way to specify a location in the game tree.
I need to choose a way to represent the game that 
a) allows easy indexing into the arrays above.
b) allows to easily find the next state of the game, be it another
   betting state or something that depends on the cards being dealt.
In the paper, they use an actual full game tree in memory, which is rediculous 
for full poker. The arrays above take enough memory. And I'd have to write code
just to create that tree in memory. 
Node is determined by: 
- gameround (here always preflop)
- card values (in Hold'em, these would be values of E(HS^2) for each gameround or something)
- betting round: 0, 1, or 2. (counting starts are zero in this world)
- outstanding bet: 0 or 1.
- pot size. (in this game, this is determinable by the betting scenario)
Then, the betting scenario index above may be calculated easily from:
(betting round) + ((betting round) == 1 ? (outstanding bet) * 2 : 0)
*/

//walker - walks the game tree, given a dealing of cards.
// returns the EV of utility (winnings) of player1
double walker(int betrnd, int outbet, double prob0, double prob1)
{
	double totalregret=0;
	//need utility for each action, from P0's view let's say. 
	//for general code, it could make sense to store an array of the max
	//number of actions for each node. or perhaps a generic action structure, and
	//pointers to one of the (only couple possible) action structures.
	double utility[2]; //where 2 is max actions for this node, that number is used a lot here.
	double avgutility;
	int betsc = betrnd + (betrnd == 1 ? outbet*2 : 0); //see comments above
	int mycard = cards[betrnd%2];
/*
	//compute probabilities anew, from total regret (equation (8))
	//Note the 1/T (here would be 1/N) divides out.
	for(int a=0; a<2; a++)
		totalregret += max(0.0,regret[mycard][betsc][a]);
	if (totalregret > 0)
		strategy[mycard][betsc] = max(0.0,regret[mycard][betsc][1]) / totalregret;
	else
		strategy[mycard][betsc] = 0.5;
*/

	//call walker for each action
	//if there are any information nodes left, we have to figure out which one
	//is next, set the variables, and call walker on the next one, so walker can 
	//process those info nodes to set the regret and strats, and also return the utility.
	//Otherwise we are at the end of a game, nothing left to walk, and we can set the 
	//utility right here.

	//so we are in betting round betrnd, with outstanding bet or not per outbet
	//ACTION = 0 - check/fold
	if (betrnd == 0)
	{
		utility[0]=walker(1, 0, prob0*(1-strategy[mycard][betsc]), prob1); //P0 checked.
		utility[1]=walker(1, 1, prob0*(strategy[mycard][betsc]), prob1); //P0 bets.
	}
	else if (betrnd == 1 && outbet == 0) 
	{
		//P0 checks, P1 checks -> showdown.
		utility[0]=(cards[0]>cards[1] ? 1 : -1);
		//P0 checks, P1 bets
		utility[1]=walker(2, 1, prob0, prob1*strategy[mycard][betsc]);
	}
	else if (betrnd == 1 && outbet == 1)
	{
		//P0 bet, P1 folded.
		utility[0]=1;
		//P0 bet, P1 called -> showdown.
		utility[1]=(cards[0]>cards[1] ? 2 : -2);
	}
	else if (betrnd == 2) 
	{
		//P0 checked, P1 bet, P0 folded
		utility[0]=-1;
		//P0 checked, P1 bet, P0 called -> showdown
		utility[1]=(cards[0]>cards[1] ? 2 : -2);
	}
	else
		assert(0);

	//compute EV of utility for this node anew
	avgutility = (1-strategy[mycard][betsc])*utility[0] + strategy[mycard][betsc]*utility[1];

	//add in new regret values to the totals
	if (betrnd%2==0) //P0 playing, use prob1, proability of player 1 getting here.
	{
		regret[mycard][betsc][0] += prob1 * (utility[0] - avgutility);
		regret[mycard][betsc][1] += prob1 * (utility[1] - avgutility);
	}
	else // P1 playing, so his regret values are negative of P0's regret values.
	{
		regret[mycard][betsc][0] += - prob0 * (utility[0] - avgutility);
		regret[mycard][betsc][1] += - prob0 * (utility[1] - avgutility);
	}

	//return EV of utility computed above
	return avgutility;
}


int _tmain(int argc, _TCHAR* argv[])
{

	double totalregret;

	cout << "processing..." << endl << endl;
/*
	for (int i=0; i<100000; i++)
	{
		cards[0] = rand()%3;
		cards[1] = (cards[0] + 1 + rand()%2)%3 ;

		//start iteration
		//zero'th betting round, no outstanding bet, probability 1 for each player to reach node.
		walker(0,0,1,1);
		if (i%10==0)
			i=i;
	}
*/
	for(int i=0; i<10000000; i++)
	{
		cards[0]=0;
		cards[1]=1;
		walker(0,0,1/6.,1/6.);

		cards[0]=0;
		cards[1]=2;
		walker(0,0,1/6.,1/6.);

		cards[0]=1;
		cards[1]=0;
		walker(0,0,1/6.,1/6.);

		cards[0]=1;
		cards[1]=2;
		walker(0,0,1/6.,1/6.);

		cards[0]=2;
		cards[1]=0;
		walker(0,0,1/6.,1/6.);

		cards[0]=2;
		cards[1]=1;
		walker(0,0,1/6.,1/6.);


		//compute probabilities anew, from total regret (equation (8))
		//Note the 1/T (here would be 1/N) divides out.
		for(int c=0; c<3; c++)
		{
			for(int b=0; b<4; b++)
			{
				totalregret=0;
				for(int a=0; a<2; a++)
					totalregret += max(0.0,regret[c][b][a]);
				if (totalregret > 0.02)
					strategy[c][b] = max(0.0,regret[c][b][1]) / totalregret;
				else
					strategy[c][b] = 0.5;
			}
		}
	}


	cout << endl << endl;
	system("PAUSE");
	return 0;
}



/*
Bug log:

time: 5-10 minutes
out of bounds array index.
symptom was seg fault of some sort. turns out cards was 1-3 instead of 0-2 as
i expected, so when i used it as an index into one array, that write overwrote
one of the cards, and then the next usage of cards as index caused fault. it
could have been much more subtle and hard to fix though.

time: 30-60 minutes
calculated utilities for player 0. forgot to negate for player 1.

*/