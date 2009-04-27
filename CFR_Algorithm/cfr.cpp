#include "stdafx.h"

using namespace std;

/* 10million iterations of broken algo:
CFR1 - pre-tree									- 8.1 seconds
CFr1 - kuhntree (pointers to dynamic memory)	- 9.4 seconds, 16% slower
CFR1 - kuhntree (array of static memory)		- 11.3 seconds, but gives wrong answer.
*/

//controls whether code to iterate through all cards, and then computer strat, is used.
//otherwise chooses 1 random card set, and competes regret & strat.
#define chance_player_sampling 1

struct betnode
{  
	char playertoact; // integer of the player whose turn it is
	char child[2]; // 0 is check/fold, 1 is bet/fold, depending on the node.
	//if child[i] is INVALID, that's because that action leads to a terminal node, 
	//and in that case, and only that case, these will be used:
	bool showdown[2]; //whether we go to showdown or not after this action
	char utility[2]; //if no showdown, how much P0 wins or loses after this action, 
					//or how much the WINNER OF THE SHOWDOWN wins, after this action.
};

#define INVALID -2

//The Kuhn betting tree.
betnode n[4]; //4 is how many bet index values.

void createkuhntree()
{
	for(int i=0; i<4; i++)
		n[i].playertoact=i%2;
	
	//and the part harder to code for generally (hence this tree)
	//You gotta see the diagram to follow this.
	//This is how poker is played.
	n[0].child[0]=1;
	n[0].child[1]=3;
	n[1].child[0]=INVALID;
	n[1].child[1]=2;

	n[2].child[0]=INVALID;
	n[2].child[1]=INVALID;
	n[3].child[0]=INVALID;
	n[3].child[1]=INVALID;

	n[0].showdown[0]=false;
	n[0].showdown[1]=false;
	n[1].showdown[0]=true;
	n[1].showdown[1]=false;

	n[2].showdown[0]=false;
	n[2].showdown[1]=true;
	n[3].showdown[0]=false;
	n[3].showdown[1]=true;

	n[0].utility[0]=INVALID;
	n[0].utility[1]=INVALID;
	n[1].utility[0]=1; //to the winner
	n[1].utility[1]=INVALID;

	n[2].utility[0]=-1; //P0 has folded, P1 wins ante
	n[2].utility[1]=2; //to the winner
	n[3].utility[0]=1; //P1 has folded, P0 wins ante
	n[3].utility[1]=2; //to the winner
}


//cards[i] - the dealt card, either 0, 1, or 2
// i is the player number
int cards[2];

//strategy[i][k] - probability of betting or calling, given the situation:
// i is the three possible cards, values as above// //j is the game round. 0 = preflop
// k is the betting scenario: 0 (1st act), 1(2nd act, no outstanding),
//							  2 (3rd act), or 3 (2nd act, outstanding bet)
float strategy[3][4]={0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
//the numerator and denominator of eq (4) in Tech Report paper
float strategysumnum[3][4]={0,0,0,0,0,0,0,0,0,0,0,0};
float strategysumden[3][4]={0,0,0,0,0,0,0,0,0,0,0,0};
//just for purposes of watching the progress in the debugger:
float strategyaverage[3][4];

//regret[i][k][a] - Total regret for each information node.
// i is three possible cards
// k is the betting scenario, same as above.
// a is the action taken. 0 = check/fold, 1 = bet/call
float regret[3][4][2]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//walker - walks the game tree, given a dealing of cards.
// returns the EV of utility (winnings) of player1
float walker(int beti, float prob0, float prob1)
{
	float totalregret=0;
	//need utility for each action, from P0's view let's say. 
	//for general code, it could make sense to store an array of the max
	//number of actions for each node. or perhaps a generic action structure, and
	//pointers to one of the (only couple possible) action structures.
	float utility[2]; //where 2 is max actions for this node, that number is used a lot here.
	float avgutility;
	int mycard = cards[n[beti].playertoact];
	float * strat, * stratnum, * stratden, * reg, * stratavg;
	const int c1=0, c2=1, b1=2, b2=2;

	if((mycard==c1 && beti==b1) || (mycard==c2 && beti==b2))
	{
		strat = &(strategy[c1][b1]);
		stratnum = &(strategysumnum[c1][b1]);
		stratden = &(strategysumden[c1][b1]);
		reg = &(regret[c1][b1][0]);
		stratavg = &(strategyaverage[c1][b1]);
	}
	else
	{
		strat = &(strategy[mycard][beti]);
		stratnum = &(strategysumnum[mycard][beti]);
		stratden = &(strategysumden[mycard][beti]);
		reg = &(regret[mycard][beti][0]);
		stratavg = &(strategyaverage[mycard][beti]);
	}

#if chance_player_sampling
	//compute probabilities anew, from total regret (equation (8))
	//Note the 1/T (here would be 1/N) divides out.
	//then update values used in equation (4)
	totalregret=0;
	for(int a=0; a<2; a++)
		totalregret += max((float)0,reg[a]);
	if (totalregret > 0)
		*strat = max((float)0,reg[1]) / totalregret;
	else
		*strat = 0.5;

	if(n[beti].playertoact==0)
	{
		*stratnum += prob0 * *strat;
		*stratden += prob0;
	}
	else
	{
		*stratnum += prob1 * *strat;
		*stratden += prob1;
	}

	*stratavg = *stratnum / *stratden;
#endif

	//call walker for each action
	//if there are any information nodes left, we have to figure out which one
	//is next, set the variables, and call walker on the next one, so walker can 
	//process those info nodes to set the regret and strats, and also return the utility.
	//Otherwise we are at the end of a game, nothing left to walk, and we can set the 
	//utility right here.

	//0th ACTION
	if (n[beti].child[0] != INVALID)
	{
		if(n[beti].playertoact==0)
			utility[0] = walker(n[beti].child[0], prob0*(1-*strat), prob1);
		else
			utility[0] = walker(n[beti].child[0], prob0, (1-*strat)*prob1);
	}
	else if(n[beti].showdown[0])
		utility[0] = (float) ((cards[0] > cards[1]) ? n[beti].utility[0] : -n[beti].utility[0]);
	else
		utility[0] = (float) n[beti].utility[0];

	//1st ACTION
	if (n[beti].child[1] != INVALID)
	{
		if(n[beti].playertoact==0)
			utility[1] = walker(n[beti].child[1], prob0**strat, prob1);
		else
			utility[1] = walker(n[beti].child[1], prob0, *strat*prob1);
	}
	else if(n[beti].showdown[1])
		utility[1] = (float) ((cards[0] > cards[1]) ? n[beti].utility[1] : -n[beti].utility[1]);
	else
		utility[1] = (float) n[beti].utility[1];


	//compute EV of utility for this node anew
	avgutility = (1-*strat)*utility[0] + *strat*utility[1];

	//add in new regret values to the totals
	if (n[beti].playertoact==0) //P0 playing, use prob1, proability of player 1 getting here.
	{
		reg[0] += prob1 * (utility[0] - avgutility);
		reg[1] += prob1 * (utility[1] - avgutility);
	}
	else // P1 playing, so his regret values are negative of P0's regret values.
	{
		reg[0] += - prob0 * (utility[0] - avgutility);
		reg[1] += - prob0 * (utility[1] - avgutility);
	}

	//return EV of utility computed above
	return avgutility;
}


int _tmain(int argc, _TCHAR* argv[])
{
	createkuhntree();
	cout << "processing..." << endl << endl;

#if chance_player_sampling
	for (int i=0; i<100000000; i++)
	{
		cards[0] = rand()%3;
		cards[1] = (cards[0] + 1 + rand()%2)%3 ;

		//start iteration
		//zero'th betting round, no outstanding bet, probability 1/6 for cards to be dealt.
		walker(0,(float)1/6,(float)1/6);

		if(i%2000==0)
			i=i;
	}

#else

	for(int i=0; i<10000000; i++)
	{
		cards[0]=0;
		cards[1]=1;
		walker(0,(float)1/6,(float)1/6);

		cards[0]=0;
		cards[1]=2;
		walker(0,(float)1/6,(float)1/6);

		cards[0]=1;
		cards[1]=0;
		walker(0,(float)1/6,(float)1/6);

		cards[0]=1;
		cards[1]=2;
		walker(0,(float)1/6,(float)1/6);

		cards[0]=2;
		cards[1]=0;
		walker(0,(float)1/6,(float)1/6);

		cards[0]=2;
		cards[1]=1;
		walker(0,(float)1/6,(float)1/6);

		for(int c=0; c<3; c++) 
		{
			for(int b=0; b<4; b++) 
			{
				float totalregret=0;
				for(int a=0; a<2; a++)
					totalregret += max((float)0,regret[c][b][a]);
				if (totalregret > 0)
					strategy[c][b] = max((float)0,regret[c][b][1]) / totalregret;
				else
					strategy[c][b] = 0.5;
			}
		}

	}
#endif

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