#include "stdafx.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/determinebins.h"
#include "../PokerLibrary/gamestate.h"
#include "memorymgr.h"
#include "savestrategy.h"
#include "../PokerLibrary/constants.h"
#include "../mymersenne.h"
using namespace std;

//global gamestate class instance.
GameState gs;
//counters to help index arrays by walker intances
int actioncounters[4][MAX_ACTIONS-1]; //minus one because single-action nodes don't exist

//basically the same function as used by memorymgr to only increment actioncounters
void dummywalker(int gr, int pot, int beti)
{
	betnode mynode;
	getnode(gr, pot, beti, mynode);
	int numa = mynode.numacts; //for ease of typing

	actioncounters[gr][numa-2]++;

	for(int a=0; a<numa; a++)
	{
		switch(mynode.result[a])
		{
		case NA:
			REPORT("invalid tree");
		case FD:
		case AI:
			continue;
		case GO:
			if(gr!=RIVER)
				dummywalker(gr+1, pot+mynode.potcontrib[a], 0);
			continue;

		default://child node
			dummywalker(gr, pot, mynode.result[a]);
			continue;
		}
	}
}


fpworking_type walker(int gr, int pot, int beti, fpworking_type prob0, fpworking_type prob1)
{

	//obtain definition of possible bets for this turn

	betnode mynode;
	getnode(gr, pot, beti, mynode);
	const int numa = mynode.numacts; //for ease of typing

	//obtain pointers to data for this turn

	fpstore_type * stratn, * stratd, * regret;
	dataindexing(gr, numa, actioncounters[gr][numa-2]++, gs.getcardsi((Player)mynode.playertoact, gr), 
		stratn, stratd, regret);


	// OK, NOW WE WILL APPLY EQUATION (8) FROM TECH REPORT, COMPUTE STRATEGY T+1

	//find total regret

	fpworking_type totalregret=0;

	for(int i=0; i<numa; i++)
		if (regret[i]>0) totalregret += regret[i];

	//set strategy proportional to positive regret, or 1/numa if no positive regret

	fpworking_type stratt[MAX_ACTIONS];

	if (totalregret > 0)
		for(int i=0; i<numa; i++)
			(regret[i]>0) ? stratt[i] = regret[i] / totalregret : stratt[i] = 0;
	else
		for(int i=0; i<numa; i++)
			stratt[i] = (fpworking_type)1/numa;


	//NOW, WE WANT TO FIND THE UTILITY OF EACH ACTION. 
	//WE OFTEN DO THIS BY CALLING WALKER RECURSIVELY.

	fpworking_type utility[MAX_ACTIONS];
	fpworking_type avgutility=0;

	for(int i=0; i<numa; i++)
	{
		//utility will be unused, children's regret will be unaffected
		//performance hack
		if(stratt[i]==0 && ((mynode.playertoact==0 && prob1==0) || (mynode.playertoact==1 && prob0==0)) )
		{
			//same code as in dummywalker
			switch(mynode.result[i])
			{
			case NA:
				REPORT("invalid tree");
			case FD:
			case AI:
				continue;
			case GO:
				if(gr!=RIVER)
					dummywalker(gr+1, pot+mynode.potcontrib[i], 0);
				continue;

			default://child node
				dummywalker(gr, pot, mynode.result[i]);
				continue;
			}
		}

		switch(mynode.result[i])
		{
		case NA: //error
			REPORT("Invalid betting tree node action reached."); //will exit

		case AI: //called allin
			utility[i] = STACKSIZE * (gs.gettwoprob0wins()-1);
			break;

		case GO: //next game round
			if(gr==RIVER)// showdown
				utility[i] = (pot+mynode.potcontrib[i]) * (gs.gettwoprob0wins()-1);
			else 
			{
				if(mynode.playertoact==0)
					utility[i] = walker(gr+1, pot+mynode.potcontrib[i], 0, prob0*stratt[i], prob1);
				else
					utility[i] = walker(gr+1, pot+mynode.potcontrib[i], 0, prob0, prob1*stratt[i]);
			}
			break;

		case FD: //fold
			utility[i] = (pot+mynode.potcontrib[i]) * (2*mynode.playertoact - 1); //acting player is loser
			break;

		default: //child node within this game round
			if(mynode.playertoact==0)
				utility[i] = walker(gr, pot, mynode.result[i], prob0*stratt[i], prob1);
			else
				utility[i] = walker(gr, pot, mynode.result[i], prob0, prob1*stratt[i]);
		}

		avgutility += stratt[i]*utility[i];
	}


	//NOW, WE WISH TO UPDATE THE REGRET TOTALS. THIS IS DONE IN ACCORDANCE WITH
	//EQUATION 3.5 (I THINK) FROM THE THESIS.
	//WE WILL ALSO UPDATE THE AVERAGE STRATEGY, EQUATION (4) FROM TECH REPORT

	if (mynode.playertoact==0) //P0 playing, use prob1, proability of player 1 getting here.
	{
		//shortcut, along with the related one below, speeds up by 10% or so.
		if(prob0!=0)
		{
			for(int a=0; a<numa-1; a++)
				stratn[a] += prob0 * stratt[a];
			*stratd += prob0;
#if STORENTHSTRAT
			stratn[numa-1] += prob0 * stratt[numa-1];
#endif
		}

		//shortcut
		if(prob1==0) return avgutility;

		for(int a=0; a<numa; a++)
			regret[a] += prob1 * (utility[a] - avgutility);
	}
	else // P1 playing, so his regret values are negative of P0's regret values.
	{
		//shortcut
		if(prob1!=0)
		{
			for(int a=0; a<numa-1; a++)
				stratn[a] += prob1 * stratt[a];
			*stratd += prob1;
#if STORENTHSTRAT
			stratn[numa-1] += prob1 * stratt[numa-1];
#endif
		}

		//shortcut
		if(prob0==0) return avgutility;

		for(int a=0; a<numa; a++)
			regret[a] += - prob0 * (utility[a] - avgutility);
	}


	//FINALLY, OUR LAST ACTION. WE WILL RETURN THE AVERAGE UTILITY, UNDER THE COMPUTED STRAT
	//SO THAT THE CALLER CAN KNOW THE EXPECTED VALUE OF THIS PATH. GOD SPEED CALLER.

	return avgutility;
}


//------------------------ p l a y g a m e -------------------------//
const long long MILLION = 1000000;
const long long THOUSAND = 1000;

string iterstring(long long iter)
{
	ostringstream o;
	if(iter%MILLION==0)
		o << 'M' << iter/MILLION;
	else if(iter>MILLION)
		o << 'M' << fixed << setprecision(1)<< (double)iter/MILLION;
	else if(iter>=10*THOUSAND)
		o << 'k' << iter/THOUSAND;
	else
		o << iter;
	return o.str();
}

string timestring(time_t mytime)
{
	char mytimestr[32];
#ifdef _WIN32 //avoid deprecation warning
	tm mytm;
	localtime_s(&mytm, &mytime);
	strftime(mytimestr, 32, "%A, %I:%M %p", &mytm); //Thursday 5:54 PM
#else
	strftime(mytimestr, 32, "%A, %I:%M %p", localtime(&mytime)); //Thursday 5:54 PM
#endif
	return string(mytimestr);
}


void simulate(long long iter)
{
	static float prevrate=-1;
	cout << endl << "starting on " << timestring(time(NULL)) << ":" << endl;
	cout << "doing " << iterstring(iter) << " iterations..." << endl;
	if(prevrate > 0)
		cout << "Expect to finish on " 
			<< timestring(time(NULL)+iter/prevrate) << endl;

	clock_t c = clock();
	for(long long i=0; i<iter; i++)
	{
		gs.dealnewgame();
		for(int i=0; i<4; i++) 
			for(int j=0; j<MAX_ACTIONS-1; j++)
				actioncounters[i][j] = 0;
		walker(PREFLOP,0,0,1,1);
	}
	clock_t diff = clock()-c;
	prevrate = (float) iter * CLOCKS_PER_SEC / (float) diff;

	cout << "...took " << float(diff)/CLOCKS_PER_SEC << " seconds. ("
		<< prevrate << " per second)." << endl;
}

#define SAVESTRATEGYFILE 0
#define TESTING 0

inline void playgame()
{
	cout << "starting work..." << endl;
	long long i=25*THOUSAND;
	long long total=0;
	simulate(100);
	simulate(i-100);
	total+=i;
	while(1)
	{
		simulate(i);
		total+=i;
		cout << "saving log of first node strat." << endl;
		ostringstream o;
		o << "output/" << SAVENAME << "-" << iterstring(total) 
#ifdef _WIN32
			<< "-win.txt";
#else
			<< "-linux.txt";
#endif
		printfirstnodestrat(o.str().c_str());
#if SAVESTRATEGYFILE
		if(total >= 500*THOUSAND)
		{
			ostringstream oo;
			oo << "strategy/" << SAVENAME << "-" << iterstring(total) << ".strat";
			cout << "saving strategy file." << endl;
			savestratresult(oo.str().c_str());
		}
#endif
		i*=2;
		if(i>3*MILLION) break;
	}
}

int main(int argc, char* argv[])
{
#if TESTING
#else
	initbettingtrees();
	initmem();
	initbins();

	mersenne.seed(42);

	playgame();

	closemem();
	closebins();
#endif
#ifdef _WIN32 //in windows, the window will close right away
	PAUSE();
#endif
	return 0;
}

