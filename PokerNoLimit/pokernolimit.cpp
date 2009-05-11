#include "stdafx.h"
#include "../PokerLibrary/treenolimit.h" //for the no-limit betting tree n[]
#include "../PokerLibrary/determinebins.h"
#include "gamestate.h"
#include "memorymgr.h"
#include "savestrategy.h"
#include "../PokerLibrary/constants.h"
using namespace std;

//global gamestate class instance.
GameState gs;
//global bool array hasbeenvisited
bitset<WALKERI_MAX*BETI_MAX> hasbeenvisited;
//bethist array for the whoooole walker recursive call-tree
int bethist[3]={-1,-1,-1};

#define COMBINE(i2, i1, n1) ((i2)*(n1) + (i1))
inline int getwalkeri(int gr, int pot)
{
	switch(gr)
	{
	case PREFLOP:
		return 0;

	case FLOP:
		return COMBINE(getpoti(gr,pot), bethist[0],
			                            BETHIST_MAX)
					+ WALKERI_PREFLOP_MAX;

	case TURN:
		return COMBINE(getpoti(gr,pot), bethist[1]*BETHIST_MAX + bethist[0],
			                            BETHIST_MAX*BETHIST_MAX)
					+ WALKERI_FLOP_MAX;

	case RIVER:
		return COMBINE(getpoti(gr,pot), bethist[2]*BETHIST_MAX*BETHIST_MAX + bethist[1]*BETHIST_MAX + bethist[0],
			                            BETHIST_MAX*BETHIST_MAX*BETHIST_MAX)
					+ WALKERI_TURN_MAX;

	default:
		REPORT("invalid gameround encountered in walkeri");
	}
}


//walker - Operates on a GIVEN game node, and then recursively walks the children nodes
// Needs to know where the fuck it is.
// beti[] contains the current betting node in beti[gr] and past ones in beti[x] for x < gr
float walker(int gr, int pot, int beti, float prob0, float prob1)
{
	float totalregret=0;
	float utility[9]; //where 9 is max actions for a node.
	float avgutility=0;
	betnode const * mynode;
	float * stratt, * stratn, * stratd, * regret;
	float stratmaxa=0; //this is the one that I do not store in the global data arrays.
	int maxa; //just the size of the action arrays
	int numa=0; //the number of actually possible actions
	bool isvalid[9]; //if this action is possible (have enough money)
	int walkeri;

	//FIRST LETS FIND THE DATA WE NEED FOR THIS INSTANCE OF WALKER

	//obtain pointer to correct betting node.
	if (gr==PREFLOP) mynode = &(pfn[beti]);
	else             mynode = &(n[beti]);

	//obtain the correct maximum possible actions from this node.
	maxa = mynode->numacts;


	// DEPENDING ON STACKSIZE, AND GAME HISTORY, NOT ALL ACTIONS MAY BE POSSIBLE FROM
	// THIS NODE! SINCE I AM NOT STORING THE ENTIRE GAME TREE, AND ONLY HAVE ONE INSTANCE
	// OF THE BETTING TREE IN MEMORY FOR _ALL_ SITUATIONS, WE NEED TO DYNAMICALLY CHECK
	// HERE TO SEE WHICH ACTIONS CAN BE AFFORDED.

	numa=getvalidity(pot,mynode,isvalid);


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
	walkeri = getwalkeri(gr, pot);
	getpointers(gs.getscenarioi(gr, mynode->playertoact, pot, bethist), beti, maxa, walkeri,
		stratt, stratn, stratd, regret);
	//***************************


	//HASBEENVISITED

	if(!hasbeenvisited.test(COMBINE(walkeri, beti, BETI_MAX)))
	{
		hasbeenvisited.set(COMBINE(walkeri, beti, BETI_MAX));

		// OK, NOW WE WILL APPLY EQUATION (8) FROM TECH REPORT, COMPUTE STRATEGY T+1
		
		//we trust that regret[a] is zero always for non-valid actions
		switch(maxa) //this switch alone gives a savings of about 1.5% total execution time
		{
			//so this won't do anything for non-valid acitons
			//non-valid a's will simply have regret still at 0.
		case 9:
			if (regret[8]>0) totalregret += regret[8];
		case 8:
			if (regret[7]>0) totalregret += regret[7];
			if (regret[6]>0) totalregret += regret[6];
			if (regret[5]>0) totalregret += regret[5];
			if (regret[4]>0) totalregret += regret[4];
			if (regret[3]>0) totalregret += regret[3];
		case 3:
			if (regret[2]>0) totalregret += regret[2];
		case 2:
			if (regret[1]>0) totalregret += regret[1];
			if (regret[0]>0) totalregret += regret[0];
		}

		if (totalregret > 0)
		{
			//this won't do anything if regret[a] is zero
			switch(maxa) //this switch gives 0.5% total savings
			{
			case 9:
				(regret[7]>0) ? stratmaxa += stratt[7] = regret[7] / totalregret : stratt[7] = 0;
			case 8:
				(regret[6]>0) ? stratmaxa += stratt[6] = regret[6] / totalregret : stratt[6] = 0;
				(regret[5]>0) ? stratmaxa += stratt[5] = regret[5] / totalregret : stratt[5] = 0;
				(regret[4]>0) ? stratmaxa += stratt[4] = regret[4] / totalregret : stratt[4] = 0;
				(regret[3]>0) ? stratmaxa += stratt[3] = regret[3] / totalregret : stratt[3] = 0;
				(regret[2]>0) ? stratmaxa += stratt[2] = regret[2] / totalregret : stratt[2] = 0;
			case 3:
				(regret[1]>0) ? stratmaxa += stratt[1] = regret[1] / totalregret : stratt[1] = 0;
			case 2:
				(regret[0]>0) ? stratmaxa += stratt[0] = regret[0] / totalregret : stratt[0] = 0;
			}

			//this will naturally be zero if non-valid
			stratmaxa = 1-stratmaxa;
		}
		else
		{
			int a;
			for(a=0; a<maxa-1; a++) 
			{
				//but here we must check to make sure it's valid
				if(isvalid[a])
					stratt[a] = (float)1/numa;
			}
			//and here too
			if(isvalid[a])
				stratmaxa = (float)1/numa;
		}

	}


	//NOW, WE WANT TO FIND THE UTILITY OF EACH ACTION. 
	//WE OFTEN DO THIS BY CALLING WALKER RECURSIVELY.

	for(int a=0; a<maxa; a++)
	{
		// because I only store the first N-1 in memory:
		float st = (a==maxa-1) ? stratmaxa : stratt[a]; 


		//if we do not have enough money, we do not touch this action. It does not exist.
		if (!isvalid[a])
			continue;

		//sometimes you feel like a nut! (cuts execution time many fold)
		//both options work, though i don't understand why the second one does. They each produce the
		//same exact correct numbers, to 5 decimal places after 1, 2, and 8 million iterations at 4ss.
		// and they yeild the same execution time, to within a tenth of a second.
		// the second one may be *slightly* slower. it confounds me!
		if( st==0 && ((mynode->playertoact==0 && prob1==0) || (mynode->playertoact==1 && prob0==0)) )
		//if((mynode->playertoact==0 && prob1==0 && (st==0 || prob0==0)) || 
	    //   (mynode->playertoact==1 && prob0==0 && (st==0 || prob1==0)))
		{
			utility[a] = 10000000000000000.0F; //that oughta do it.
			continue;
		}

		switch(mynode->result[a])
		{
		case NA:
			REPORT("Invalid betting tree node action reached."); //will exit

		case AI: // SHOWDOWN - from all-in call!
			utility[a] = STACKSIZE * (gs.gettwoprob0wins()-1);
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
				bethist[gr]=(mynode->result[a]-GO_BASE); 
				if(bethist[gr] <0 || bethist[gr] >= BETHIST_MAX)
					REPORT("invalid betting history encountered in walker");

				//NOTE that bethist, being an array, is passed by reference. this is bad becasue
				// walker is recursive, and only ONE copy of the array will be used for an ENTIRE
				// game including all instances of walker. BUT, because all child instances of walker
				// will return before we go onto another betting history option, and because bethist's
				// are recalled (i.e. don't change in the future), i think it should work perfectly.

				if(mynode->playertoact==0)
					utility[a] = walker(gr+1, pot+mynode->potcontrib[a], 0, prob0*st, prob1);
				else
					utility[a] = walker(gr+1, pot+mynode->potcontrib[a], 0, prob0, prob1*st);
			}
			// ...or a SHOWDOWN - from the river!
			else
				utility[a] = (pot+mynode->potcontrib[a]) * (gs.gettwoprob0wins()-1);

			break;
		
		case FD: // FOLD

			utility[a] = (pot+mynode->potcontrib[a]) * (2*mynode->playertoact - 1); //acting player is loser
			break;

		default: // CONTINUE WITHIN THIS GAME ROUND

			//result is a child node!
			if(mynode->playertoact==0)
				utility[a] = walker(gr, pot, mynode->result[a], prob0*st, prob1);
			else
				utility[a] = walker(gr, pot, mynode->result[a], prob0, prob1*st);
		}

		//keep a running total of the EV of utility under current strat.
		avgutility += st*utility[a];
	}


	//NOW, WE WISH TO UPDATE THE REGRET TOTALS. THIS IS DONE IN ACCORDANCE WITH
	//EQUATION 3.5 (I THINK) FROM THE THESIS.
	//WE WILL ALSO UPDATE THE AVERAGE STRATEGY, EQUATION (4) FROM TECH REPORT

	if (mynode->playertoact==0) //P0 playing, use prob1, proability of player 1 getting here.
	{
		//shortcut, along with the related one below, speeds up by 10% or so.
		if(prob0!=0)
			for(int a=0; a<maxa-1; a++)
			{
				//this is just for performance, as these dont affect valid entries, they're only result
				if(isvalid[a])
				{
					stratn[a] += prob0 * stratt[a];
					stratd[a] += prob0;
				}
			}

		//shortcut
		if(prob1==0) return avgutility;

		for(int a=0; a<maxa; a++)
		{
			//this was a major bug, only update when valid.
			if(isvalid[a])
				regret[a] += prob1 * (utility[a] - avgutility);
		}
	}
	else // P1 playing, so his regret values are negative of P0's regret values.
	{
		//shortcut
		if(prob1!=0)
			for(int a=0; a<maxa-1; a++)
			{
				//this is just for performance
				if(isvalid[a])
				{
					stratn[a] += prob1 * stratt[a];
					stratd[a] += prob1;
				}
			}

		//shortcut
		if(prob0==0) return avgutility;

		for(int a=0; a<maxa; a++)
		{
			//this was a major bug, only update when valid.
			if(isvalid[a])
				regret[a] += - prob0 * (utility[a] - avgutility);
		}
	}


	//FINALLY, OUR LAST ACTION. WE WILL RETURN THE AVERAGE UTILITY, UNDER THE COMPUTED STRAT
	//SO THAT THE CALLER CAN KNOW THE EXPECTED VALUE OF THIS PATH. GOD SPEED CALLER.

	return avgutility;
}


//------------------------ p l a y g a m e -------------------------//
void simulate(unsigned int iter)
{
	clock_t c1;
	c1 = clock();
	for(unsigned int i=0; i<iter; i++)
	{
		gs.dealnewgame();
		hasbeenvisited.reset(); //sets all to false
		walker(0,0,0,1,1);
	}
	BENCH(c1,iter);
}

inline void playgame()
{
	clock_t c1;
	c1 = clock();

	cout << "starting work..." << endl;
	simulate(1000);
//	printfirstnodestrat("output/test 1Mi8bin4ss v2.txt");

//	simulate(1000000);
//	printfirstnodestrat("output/test 2Mi8bin4ss v2.txt");

//	simulate(6000000);
//	printfirstnodestrat("output/test 8Mi8bin4ss v2.txt");
}

int _tmain(int argc, _TCHAR* argv[])
{
	initpfn();
	initbins();
	initmem();

	playgame();

	closemem();
	closebins();
	system("PAUSE");
	return 0;
}

