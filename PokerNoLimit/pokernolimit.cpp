#include "stdafx.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/determinebins.h"
#include "../PokerLibrary/gamestate.h"
#include "memorymgr.h"
#include "savestrategy.h"
#include "../PokerLibrary/constants.h"
using namespace std;

//global gamestate class instance.
GameState gs;
//counters to help index arrays by walker intances
int actioncounters[4][MAX_ACTIONS-1]; //minus one because single-action nodes don't exist


float walker(int gr, int pot, int beti, float prob0, float prob1)
{

	//obtain definition of possible bets for this turn

	betnode mynode;
	getnode(gr, pot, beti, mynode);
	const int numa = mynode.numacts; //for ease of typing

	//obtain pointers to data for this turn

	float * stratn, * stratd, * regret;
	dataindexing(gr, numa, actioncounters[gr][numa-2]++, gs.getcardsi((Player)mynode.playertoact, gr), 
		stratn, stratd, regret);


	// OK, NOW WE WILL APPLY EQUATION (8) FROM TECH REPORT, COMPUTE STRATEGY T+1

	//find total regret

	float totalregret=0;

	for(int i=0; i<numa; i++)
		if (regret[i]>0) totalregret += regret[i];

	//set strategy proportional to positive regret, or 1/numa if no positive regret

	float stratt[MAX_ACTIONS];

	if (totalregret > 0)
		for(int i=0; i<numa; i++)
			(regret[i]>0) ? stratt[i] = regret[i] / totalregret : stratt[i] = 0;
	else
		for(int i=0; i<numa; i++)
			stratt[i] = (float)1/numa;


	//NOW, WE WANT TO FIND THE UTILITY OF EACH ACTION. 
	//WE OFTEN DO THIS BY CALLING WALKER RECURSIVELY.

	float utility[MAX_ACTIONS];
	float avgutility=0;

	for(int i=0; i<numa; i++)
	{
		/**** need to call dummy walker here 
		if(stratt[i]==0 && ((mynode.playertoact==0 && prob1==0) || (mynode.playertoact==1 && prob0==0)) )
			call dummy walker
			for performance hack ******/

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
			{
				stratn[a] += prob0 * stratt[a];
				stratd[a] += prob0;
			}
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
			{
				stratn[a] += prob1 * stratt[a];
				stratd[a] += prob1;
			}
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
		o << iter/MILLION << 'M';
	else if(iter>MILLION)
		o << fixed << setprecision(1)<< double(iter)/MILLION << 'M';
	else if(iter>=10*THOUSAND)
		o << iter/THOUSAND << 'k';
	else
		o << iter;
	return o.str();
}

string timestring(time_t mytime)
{
	char mytimestr[32];
	tm mytm;
	localtime_s(&mytm, &mytime);
	strftime(mytimestr, 32, "%A, %I:%M %p", &mytm); //Thursday 5:54 PM
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
		//system("pause");
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
	simulate(i);
	total+=i;
	while(1)
	{
		simulate(i);
		total+=i;
		cout << "saving log of first node strat." << endl;
		ostringstream o;
		o << "output/13ss-256,90,32bins-" << iterstring(total) << "-1.txt";
		printfirstnodestrat(o.str().c_str());
#if SAVESTRATEGYFILE
		if(total >= 500*THOUSAND)
		{
			ostringstream oo;
			oo << "strategy/6ss-256,90,32bins-" << iterstring(total) << ".strat";
			cout << "saving strategy file." << endl;
			savestratresult(oo.str().c_str());
		}
#endif
		i*=2;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
#if TESTING
#else
	initbettingtrees();
	initmem();
	initbins();

	playgame();

	closemem();
	closebins();
	system("PAUSE");
#endif
	return 0;
}

