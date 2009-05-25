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
int pflopwalkeri, flopwalkeri, turnwalkeri;

float walker(int gr, int pot, treedata_t * base0, treedata_t * base1,
			 int beti, float prob0, float prob1)
{
	float totalregret=0;
	float utility[9]; //where 9 is max actions for a node.
	float avgutility=0;
	float stratt[9]; //allocated on the stack!
	float stratmaxa=0; //this is the one that I do not store in the global data arrays.

	betnode const * mynode = gettree(gr, beti);
	int maxa = mynode->numacts;
	bool isvalid[9];
	int numa = getvalidity(pot,mynode,isvalid);
#if VECTORIZE
	vector<float>::iterator stratn, stratd, regret;
#else
	float * stratn, * stratd, * regret;
#endif
	if(mynode->playertoact==P0)
		betindexing(beti, base0, stratn, stratd, regret);
	else
		betindexing(beti, base1, stratn, stratd, regret);


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

		if(stratmaxa < -FLT_EPSILON*3)
			REPORT("fatal error in walker: stratmaxa negative");
		else if(stratmaxa <= FLT_EPSILON) //rounding errors occur
			stratmaxa=0;
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



	//NOW, WE WANT TO FIND THE UTILITY OF EACH ACTION. 
	//WE OFTEN DO THIS BY CALLING WALKER RECURSIVELY.

	for(int a=0; a<maxa; a++)
	{
		if(!isvalid[a])
			continue;

		float st = (a==maxa-1) ? stratmaxa : stratt[a]; 

		if(st==0 && ((mynode->playertoact==0 && prob1==0) || (mynode->playertoact==1 && prob0==0)) )
			continue;

		switch(mynode->result[a])
		{
		case NA: //error
			REPORT("Invalid betting tree node action reached."); //will exit

		case AI: //called allin
			utility[a] = STACKSIZE * (gs.gettwoprob0wins()-1);
			break;

		case GO: //next game round
			if(gr==RIVER)// showdown
				utility[a] = (pot+mynode->potcontrib[a]) * (gs.gettwoprob0wins()-1);
			else 
			{
				treedata_t * newbase0, * newbase1;
				if(gr==PREFLOP)
				{
					while(pfexitnodes[pflopwalkeri] != beti)
						pflopwalkeri++;
					newbase0 = ftreebase[pflopwalkeri].treedata + gs.getflopcardsi(P0);
					newbase1 = ftreebase[pflopwalkeri].treedata + gs.getflopcardsi(P1);
				}
				else if(gr==FLOP)
				{
					while(fexitnodes[flopwalkeri].x != pfexitnodes[pflopwalkeri]) //pflopwalkeri was brought up to speed when we exited the preflop
						flopwalkeri++;
					while(fexitnodes[flopwalkeri].y != beti)
						flopwalkeri++;
					newbase0 = ttreebase[flopwalkeri].treedata + gs.getturncardsi(P0);
					newbase1 = ttreebase[flopwalkeri].treedata + gs.getturncardsi(P1);
				}
				else //(gr==TURN)
				{
					if(pfexitnodes[pflopwalkeri] != fexitnodes[flopwalkeri].x)
						REPORT("failure of fucked up indexing");
					while(texitnodes[turnwalkeri].x != pfexitnodes[pflopwalkeri])
						turnwalkeri++;
					while(texitnodes[turnwalkeri].y != fexitnodes[flopwalkeri].y)
						turnwalkeri++;
					while(texitnodes[turnwalkeri].z != beti)
						turnwalkeri++;
					newbase0 = rtreebase[turnwalkeri].treedata + gs.getrivercardsi(P0);
					newbase1 = rtreebase[turnwalkeri].treedata + gs.getrivercardsi(P1);
				}

				if(mynode->playertoact==0)
					utility[a] = walker(gr+1, pot+mynode->potcontrib[a], newbase0, newbase1, 0, prob0*st, prob1);
				else
					utility[a] = walker(gr+1, pot+mynode->potcontrib[a], newbase0, newbase1, 0, prob0, prob1*st);
			}
			break;

		case FD: //fold
			utility[a] = (pot+mynode->potcontrib[a]) * (2*mynode->playertoact - 1); //acting player is loser
			break;

		default: //child node within this game round
			if(mynode->playertoact==0)
				utility[a] = walker(gr, pot, base0, base1, mynode->result[a], prob0*st, prob1);
			else
				utility[a] = walker(gr, pot, base0, base1, mynode->result[a], prob0, prob1*st);
		}

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
				//invalid ones should be kept at zero
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
				//invalid ones should be kept at zero
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
		treedata_t * newbase0, * newbase1;
		gs.dealnewgame();
		newbase0 = pftreebase->treedata + gs.getpreflopcardsi(P0);
		newbase1 = pftreebase->treedata + gs.getpreflopcardsi(P1);
		pflopwalkeri = 0;
		flopwalkeri = 0;
		turnwalkeri = 0;
		walker(PREFLOP,0,newbase0,newbase1,0,1,1);
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
		o << "output/6ss-256,90,32bins-" << iterstring(total) << "-1.txt";
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

