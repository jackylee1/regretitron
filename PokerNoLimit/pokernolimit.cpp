#include "stdafx.h"
#include "treenolimit.h" //for the no-limit betting tree n[]
#include "determinebins.h"
#include "gamestate.h"
#include "memorymgr.h"
using namespace std;

//global gamestate class instance.
GameState gs;

//walker - Operates on a GIVEN game node, and then recursively walks the children nodes
// Needs to know where the fuck it is.
// beti[] contains the current betting node in beti[gr] and past ones in beti[x] for x < gr
float walker(int gr, int pot, int bethist[3], int beti, float prob0, float prob1)
{
	float totalregret=0;
	float utility[9]; //where 9 is max actions for a node.
	float avgutility=0;
	betnode const * mynode;
	float * stratt, * stratn, * stratd, * regret;
	float stratmaxa=0; //this is the one that I do not store in the global data arrays.
	int maxa; //just the size of the action arrays
	int numa=0; //the number of actually possible actions
	bool isvalida[9]; //if this action is possible (have enough money)
	
	//FIRST LETS FIND THE DATA WE NEED FOR THIS INSTANCE OF WALKER

	//obtain pointer to correct betting node.
	if (gr==PREFLOP) mynode = &(pfn[beti]);
	else             mynode = &(n[beti]);

	//obtain the correct maximum possible actions from this node.
	maxa = mynode->numacts;

	//**************************
	// need to obtain pointers to the relevant data
	// those pointers are indexed by:
	// flop score (computed once per game, same for both players)
	// hand score (computed once per game, depends on which player)
	// pot size (depends on game state, same for both players)
	// betting history (depends on game state, same for both players)
	// current betting index (depends on game state, same for both players)

	// flop score and hand score can be stored in a gamestate structure or class
	// and retrieved by passing in the specific gameround and playertoact

	// pot size is calculated only in walker, and we have it available all the time easily.

	// betting history is determined by data in betting tree, and stored only in walker. 
	// because of notes below, it is available by also including the specific gameround.
	
	// we of course have the betting index.
	
	// because the data is stored differently for different beti depending on the number 
	// of actions, it makes sense to combine everything together BUT beti into a single
	// scenario index.
	 
	// I can coerce the GameState class to calculate this for me.

	// THEN, i pass this scenario index in to the getpointers function, which handles memory
	// and the special beti indexing.
	getpointers(gs.getscenarioi(gr, mynode->playertoact, pot, bethist), beti, maxa, 
		stratt, stratn, stratd, regret);
	//***************************


	// DEPENDING ON STACKSIZE, AND GAME HISTORY, NOT ALL ACTIONS MAY BE POSSIBLE FROM
	// THIS NODE! SINCE I AM NOT STORING THE ENTIRE GAME TREE, AND ONLY HAVE ONE INSTANCE
	// OF THE BETTING TREE IN MEMORY FOR _ALL_ SITUATIONS, WE NEED TO DYNAMICALLY CHECK
	// HERE TO SEE WHICH ACTIONS CAN BE AFFORDED.

	for(int a=0; a<maxa; a++)
	{
		//they both need the money for it to be a valid action. All-in is always a valid action. (should have need 0).
		isvalida[a] = (pot + mynode->potcontrib[a] < STACKSIZE);
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
			stratmaxa += stratt[a] = max((float)0,regret[a]) / totalregret;
		stratmaxa = 1-stratmaxa;
	}
	else
	{
		for(int a=0; a<maxa-1; a++) 
			stratt[a] = (float)1/numa;
		stratmaxa = (float)1/numa;
	}


	//NOW WE WILL UPDATE THE AVERAGE STRATEGY, EQUATION (4) FROM TECH REPORT

	if(mynode->playertoact==0)
	{
		for(int a=0; a<maxa-1; a++)
		{
			stratn[a] += prob0 * stratt[a];
			stratd[a] += prob0;
		}
	}
	else
	{
		for(int a=0; a<maxa-1; a++)
		{
			stratn[a] += prob1 * stratt[a];
			stratd[a] += prob1;
		}
	}


	//NOW, WE WANT TO FIND THE UTILITY OF EACH ACTION. 
	//WE OFTEN DO THIS BY CALLING WALKER RECURSIVELY.

	for(int a=0; a<maxa; a++)
	{
		// because I only store the first N-1 in memory:
		float st = (a==maxa-1) ? stratmaxa : stratt[a]; 


		//if we do not have enough money, we do not touch this action. It does not exist.
		if (!isvalida[a])
			continue;

		switch(mynode->result[a])
		{
		case NA:
			REPORT("Invalid betting tree node action reached."); //will exit

		case AI: // SHOWDOWN - from all-in call!
			utility[a] = STACKSIZE * (2*gs.getprob0wins()-1);
			break;

		// CONTINUE AT NEXT GAME ROUND
		case GO1: 
		case GO2: 
		case GO3: 
		case GO4: 
		case GO5: 

			//...which is an actual game round...
			if(gr!=RIVER)
			{
				//this is the only time we set bethist. array access okay as gr!=RIVER
				//this should create an integer between 0 and 4
				bethist[gr]=(mynode->result[a]-GO1); 
				if(bethist[gr] >= BETHIST_MAX)
					REPORT("invalid betting history encountered in walker");

				//NOTE that bethist, being an array, is passed by reference. this is bad becasue
				// walker is recursive, and only ONE copy of the array will be used for an ENTIRE
				// game including all instances of walker. BUT, because all child instances of walker
				// will return before we go onto another betting history option, and because bethist's
				// are recalled (i.e. don't change in the future), i think it should work perfectly.

				if(mynode->playertoact==0)
					utility[a] = walker(gr+1, pot+mynode->potcontrib[a], bethist, 0, prob0*st, prob1);
				else
					utility[a] = walker(gr+1, pot+mynode->potcontrib[a], bethist, 0, prob0, prob1*st);
			}
			// ...or a SHOWDOWN - from the river!
			else
				utility[a] = (pot+mynode->potcontrib[a]) * (2*gs.getprob0wins()-1);

			break;
		
		case FD: // FOLD

			utility[a] = (pot+mynode->potcontrib[a]) * (2*mynode->playertoact - 1); //acting player is loser
			break;

		default: // CONTINUE WITHIN THIS GAME ROUND

			//result is a child node!
			if(mynode->playertoact==0)
				utility[a] = walker(gr, pot, bethist, mynode->result[a], prob0*st, prob1);
			else
				utility[a] = walker(gr, pot, bethist, mynode->result[a], prob0, prob1*st);
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


//------------------------ p l a y g a m e -------------------------//
void playgame()
{
	//bethist array for the whoooole walker recursive call-tree
	int bethist[3] = {-1, -1, -1};

	for(int i=0; i<10000; i++)
	{
		gs.dealnewgame();
		walker(0,0,bethist,0,1,1);
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	initpfn();
	initbins();
	initmem();

	playgame();

	system("PAUSE");
	return 0;
}

