// CFR_Algorithm.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace std;

struct betnode
{  
	int playertoact; // integer of the player whose turn it is
	int betindex; // provides the infonode index, or at least the betting part of it.
	betnode* parent; //the node that comes before this one
	int parentaction; //the action of our parent that leads to this node.
	betnode* child[2]; // 0 is check/fold, 1 is bet/fold, depending on the node.
	//if child[i] is null, that's becauuse that action leads to a terminal node, 
	//and in that case, and only that case, these will be used:
	bool showdown[2]; //whether we go to showdown or not after this action
	int utility[2]; //if no showdown, how much P0 wins or loses after this action, 
					//or how much the WINNER OF THE SHOWDOWN wins, after this action.
};

#define INVALID 0xDEAD

//The Kuhn betting tree.
betnode* n[4]; //4 is how many betindex values.

void createkuhntree()
{
	for(int i=0; i<4; i++)
	{
		n[i]=new betnode;
		n[i]->playertoact=i%2;
		n[i]->betindex=i;
	}
	
	//and the part harder to code for generally (hence this tree)
	//You gotta see the diagram to follow this.
	//This is how poker is played.
	n[0]->child[0]=n[1];
	n[0]->child[1]=n[3];
	n[1]->child[0]=NULL;
	n[1]->child[1]=n[2];

	n[2]->child[0]=NULL;
	n[2]->child[1]=NULL;
	n[3]->child[0]=NULL;
	n[3]->child[1]=NULL;

	n[0]->parent=NULL;
	n[1]->parent=n[0];
	n[2]->parent=n[1];
	n[3]->parent=n[0];

	n[0]->parentaction=INVALID;
	n[1]->parentaction=0;
	n[2]->parentaction=1;
	n[3]->parentaction=1;
	
	n[0]->showdown[0]=false;
	n[0]->showdown[1]=false;
	n[1]->showdown[0]=true;
	n[1]->showdown[1]=false;

	n[2]->showdown[0]=false;
	n[2]->showdown[1]=true;
	n[3]->showdown[0]=false;
	n[3]->showdown[1]=true;

	n[0]->utility[0]=INVALID;
	n[0]->utility[1]=INVALID;
	n[1]->utility[0]=1; //to the winner
	n[1]->utility[1]=INVALID;

	n[2]->utility[0]=-1; //P0 has folded, P1 wins ante
	n[2]->utility[1]=2; //to the winner
	n[3]->utility[0]=1; //P1 has folded, P0 wins ante
	n[3]->utility[1]=2; //to the winner
}

//strategy[c][b] - probability of betting or calling, given the situation:
// c is the three possible cards
// b is the betting scenario index
double strategy[3][4]={0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};

//regret[c][b][a] - Total regret for each information node.
// c is three possible cards
// b is the betting scenario index
// a is the action taken. 0 = check/fold, 1 = bet/call
double regret[3][4][2]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};


int _tmain(int argc, _TCHAR* argv[])
{

	double totalregret;

	cout << "processing..." << endl << endl;

	createkuhntree();
	for(int x=0; x<10000000; x++)
	{
		//UPDATE REGRET
		for(int mycard=0; mycard<3; mycard++)
		{
			for(int beti=0; beti<4; beti++)
			{
				//from the formula, sum over h
				for (int oppcard=(mycard+1)%3; oppcard!=mycard; oppcard=(oppcard+1)%3)
				{
					//u[i]: utility for action i from P0's PoV.
					double u[2];
					//average utility for this node, calulated using the current strategy
					double avgu;

					int card0 = (n[beti]->playertoact == 0) ? mycard : oppcard;
					int card1 = (n[beti]->playertoact == 1) ? mycard : oppcard;

					avgu = avgutility(card0, card1, beti);

					//finish finding utilities of each action
					//this completes the sum over h' in the formula
					for(int a=0; a<2; a++)
					{
						if (n[beti]->child[a] != NULL)
							u[a] = avgutility(card0, card1, n[beti]->child[a]->betindex);
						else if(n[beti]->showdown[a])
							u[a] = (card0 > card1) ? n[beti]->utility[a] : -n[beti]->utility[a];
						else
							u[a] = n[beti]->utility[a];
					}

					//set u[i] to be the correct sign.
					if (n[beti]->playertoact == 1)
					{
						u[0] = -u[0];
						u[1] = -u[1];
						avgu = -avgu;
					}

					//update the regret for this infonode
					regret[mycard][beti][0] += \
						preprob(oppcard, n[beti]->playertoact, beti) * (u[0] - avgu);
					regret[mycard][beti][1] += \
						preprob(oppcard, n[beti]->playertoact, beti) * (u[1] - avgu);
				}
			}
		}

		//UPDATE PROBABILITIES
		for(int mycard=0; mycard<3; mycard++)
		{
			for(int beti=0; beti<4; beti++)
			{
				//update the regret for this infonode
				totalregret=0;
				for(int a=0; a<2; a++)
					totalregret += max(0.0,regret[mycard][beti][a]);
				if (totalregret > 0.0001)
					//probability of bet/call
					strategy[mycard][beti] = max(0.0,regret[mycard][beti][1]) / totalregret;
				else
					strategy[mycard][beti] = 0.5;
			}
		}

	}

	cout << endl << endl;
	system("PAUSE");
	delete n[0], n[1], n[2], n[3];
	return 0;
}

//\pi^\sigma_{-i}(h)
//probability of getting to h given player i plays to get here.
double preprob(int oppcard, int playerignore, int beti)
{
	if (n[beti]->parent != NULL)
	{
		int parentbeti = n[beti]->parent->betindex;
		if (n[beti]->parent->playertoact != playerignore)
		{
			//return the probability this parent woulud have chosen this action.
			if (n[beti]->parentaction == 0)
				return (1-strategy[oppcard][parentbeti]) * preprob(oppcard, playerignore, parentbeti);
			else
				return strategy[oppcard][parentbeti] * preprob(oppcard, playerignore, parentbeti);
		}
		else
			//don't count probabilities of playerignore
			return preprob(oppcard, playerignore, parentbeti);
	}
	else
		return 1.0/6.0; //probability of getting dealt this hand
}

//return avgutility (TO PLAYER 0) of being at beti under current strategy
double avgutility(int card0, int card1, int beti)
{
	double u=0;
	
	int mycard = (n[beti]->playertoact == 0) ? card0 : card1;
	
	//0th ACTION
	if (n[beti]->child[0] != NULL)
		u += (1-strategy[mycard][beti]) * avgutility(card0, card1, n[beti]->child[0]->betindex);
	else if(n[beti]->showdown[0])
		u += (1-strategy[mycard][beti]) * ((card0 > card1) ? n[beti]->utility[0] : -n[beti]->utility[0]);
	else
		u += (1-strategy[mycard][beti]) * n[beti]->utility[0];

	//1st ACTION
	if (n[beti]->child[1] != NULL)
		u += strategy[mycard][beti] * avgutility(card0, card1, n[beti]->child[1]->betindex);
	else if(n[beti]->showdown[1])
		u += strategy[mycard][beti] * ((card0 > card1) ? n[beti]->utility[1] : -n[beti]->utility[1]);
	else
		u += strategy[mycard][beti] * n[beti]->utility[1];

	return u;
}
