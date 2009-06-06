#include "stdafx.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/determinebins.h"
#include "../PokerLibrary/gamestate.h"
#include "memorymgr.h"
#include "savestrategy.h"
#include "../PokerLibrary/constants.h"
#include "../mymersenne.h"
using namespace std;

//needs to be accessible by dummywalker and actual walker
struct threaddata_t
{
	int cardsi[4][2]; // 4 gamerounds, 2 players.
	int twoprob0wins;
	int actioncounters[4][MAX_ACTIONS-1]; //minus one because single-action nodes don't exist
} threaddata[NUM_THREADS];

//basically the same function as used by memorymgr to only increment actioncounters
void dummywalker(int id, int gr, int pot, int beti)
{
	betnode mynode;
	getnode(gr, pot, beti, mynode);
	int numa = mynode.numacts; //for ease of typing

	threaddata[id].actioncounters[gr][numa-2]++;

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
				dummywalker(id, gr+1, pot+mynode.potcontrib[a], 0);
			continue;

		default://child node
			dummywalker(id, gr, pot, mynode.result[a]);
			continue;
		}
	}
}


fpworking_type walker(int id, int gr, int pot, int beti, fpworking_type prob0, fpworking_type prob1)
{

	//obtain definition of possible bets for this turn

	betnode mynode;
	getnode(gr, pot, beti, mynode);
	const int numa = mynode.numacts; //for ease of typing

	//obtain pointers to data for this turn

	fpstore_type * stratn, * stratd, * regret;
	dataindexing(gr, numa, 
		threaddata[id].actioncounters[gr][numa-2]++, 
		threaddata[id].cardsi[gr][(int)mynode.playertoact], 
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
					dummywalker(id, gr+1, pot+mynode.potcontrib[i], 0);
				continue;

			default://child node
				dummywalker(id, gr, pot, mynode.result[i]);
				continue;
			}
		}

		switch(mynode.result[i])
		{
		case NA: //error
			REPORT("Invalid betting tree node action reached."); //will exit

		case AI: //called allin
			utility[i] = STACKSIZE * (threaddata[id].twoprob0wins-1);
			break;

		case GO: //next game round
			if(gr==RIVER)// showdown
				utility[i] = (pot+mynode.potcontrib[i]) * (threaddata[id].twoprob0wins-1);
			else 
			{
				if(mynode.playertoact==0)
					utility[i] = walker(id, gr+1, pot+mynode.potcontrib[i], 0, prob0*stratt[i], prob1);
				else
					utility[i] = walker(id, gr+1, pot+mynode.potcontrib[i], 0, prob0, prob1*stratt[i]);
			}
			break;

		case FD: //fold
			utility[i] = (pot+mynode.potcontrib[i]) * (2*mynode.playertoact - 1); //acting player is loser
			break;

		default: //child node within this game round
			if(mynode.playertoact==0)
				utility[i] = walker(id, gr, pot, mynode.result[i], prob0*stratt[i], prob1);
			else
				utility[i] = walker(id, gr, pot, mynode.result[i], prob0, prob1*stratt[i]);
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

//-----------------------t h r e a d-------------------------------//

//global gamestate class instance.
GameState gs;

//number of iterations remaining
long long iterations;

#if __GNUC__ && NUM_THREADS > 1
//eventually one lock per player per gameround per cardsi
pthread_mutex_t * cardsilocks[4];
pthread_mutex_t threaddatalock;

void initlocks()
{
	cardsilocks[PREFLOP] = new pthread_mutex_t[2*CARDSI_PFLOP_MAX];
	cardsilocks[FLOP] = new pthread_mutex_t[2*CARDSI_FLOP_MAX];
	cardsilocks[TURN] = new pthread_mutex_t[2*CARDSI_TURN_MAX];
	cardsilocks[RIVER] = new pthread_mutex_t[2*CARDSI_RIVER_MAX];
	for(int i=0; i<2*CARDSI_PFLOP_MAX; i++)
		pthread_mutex_init(&cardsilocks[PREFLOP][i], NULL);
	for(int i=0; i<2*CARDSI_FLOP_MAX; i++)
		pthread_mutex_init(&cardsilocks[FLOP][i], NULL);
	for(int i=0; i<2*CARDSI_TURN_MAX; i++)
		pthread_mutex_init(&cardsilocks[TURN][i], NULL);
	for(int i=0; i<2*CARDSI_RIVER_MAX; i++)
		pthread_mutex_init(&cardsilocks[RIVER][i], NULL);
	pthread_mutex_init(&threaddatalock, NULL);
}
#endif

void* threadloop(void* threadnum)
{
	long myid = (long)threadnum;
	if(myid<0 || myid>=NUM_THREADS)
		REPORT("failure of pointer casting");
	int id = (int)myid;

	for(int i=0; i<4; i++) 
		for(int j=0; j<MAX_ACTIONS-1; j++)
			threaddata[id].actioncounters[i][j] = 0;

	while(1)
	{
#if __GNUC__ && NUM_THREADS > 1
		pthread_mutex_lock(&threaddatalock);
#endif
		if(iterations==0)
		{
#if __GNUC__ && NUM_THREADS > 1
			pthread_mutex_unlock(&threaddatalock);
#endif
			break; //nothing left to be done
		}
		iterations--;
		gs.dealnewgame();
		for(int gr=PREFLOP; gr<=RIVER; gr++)
		{
			threaddata[id].cardsi[gr][P0] = gs.getcardsi(P0,gr);
			threaddata[id].cardsi[gr][P1] = gs.getcardsi(P1,gr);
		}
		threaddata[id].twoprob0wins = gs.gettwoprob0wins();
#if __GNUC__ && NUM_THREADS > 1
		pthread_mutex_lock(&cardsilocks[PREFLOP][threaddata[id].cardsi[PREFLOP][P0]*2 + P0]);
		pthread_mutex_lock(&cardsilocks[PREFLOP][threaddata[id].cardsi[PREFLOP][P1]*2 + P1]);
		pthread_mutex_lock(&cardsilocks[FLOP][threaddata[id].cardsi[FLOP][P0]*2 + P0]);
		pthread_mutex_lock(&cardsilocks[FLOP][threaddata[id].cardsi[FLOP][P1]*2 + P1]);
		pthread_mutex_lock(&cardsilocks[TURN][threaddata[id].cardsi[TURN][P0]*2 + P0]);
		pthread_mutex_lock(&cardsilocks[TURN][threaddata[id].cardsi[TURN][P1]*2 + P1]);
		pthread_mutex_lock(&cardsilocks[RIVER][threaddata[id].cardsi[RIVER][P0]*2 + P0]);
		pthread_mutex_lock(&cardsilocks[RIVER][threaddata[id].cardsi[RIVER][P1]*2 + P1]);
		pthread_mutex_unlock(&threaddatalock);
#endif
		walker((int)myid,PREFLOP,0,0,1,1);
#if __GNUC__ && NUM_THREADS > 1
		pthread_mutex_unlock(&cardsilocks[PREFLOP][threaddata[id].cardsi[PREFLOP][P0]*2 + P0]);
		pthread_mutex_unlock(&cardsilocks[PREFLOP][threaddata[id].cardsi[PREFLOP][P1]*2 + P1]);
		pthread_mutex_unlock(&cardsilocks[FLOP][threaddata[id].cardsi[FLOP][P0]*2 + P0]);
		pthread_mutex_unlock(&cardsilocks[FLOP][threaddata[id].cardsi[FLOP][P1]*2 + P1]);
		pthread_mutex_unlock(&cardsilocks[TURN][threaddata[id].cardsi[TURN][P0]*2 + P0]);
		pthread_mutex_unlock(&cardsilocks[TURN][threaddata[id].cardsi[TURN][P1]*2 + P1]);
		pthread_mutex_unlock(&cardsilocks[RIVER][threaddata[id].cardsi[RIVER][P0]*2 + P0]);
		pthread_mutex_unlock(&cardsilocks[RIVER][threaddata[id].cardsi[RIVER][P1]*2 + P1]);
#endif
		for(int i=0; i<4; i++) 
			for(int j=0; j<MAX_ACTIONS-1; j++)
				threaddata[id].actioncounters[i][j] = 0;
	}
	return NULL;
}

void doloops(long long iter)
{
	iterations = iter; //set the number of iterations to do
#if __GNUC__ && NUM_THREADS>1
	//pop those babies off
	pthread_t threads[NUM_THREADS-1];
	for(int i=0; i<NUM_THREADS-1; i++)
		if(pthread_create(&(threads[i]), NULL, threadloop, (void *)i))
			REPORT("error creating thread");
#endif
	//use current thread too
	threadloop((void*)(NUM_THREADS-1)); //the nth thread is the main one
#if __GNUC__ && NUM_THREADS>1
	//wait for them all to return
	for(int i=0; i<NUM_THREADS-1; i++)
		if(pthread_join(threads[i],NULL))
			REPORT("error joining thread");
#endif
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

	time_t t = time(NULL);
	doloops(iter);
	double diff = difftime(time(NULL), t);
	prevrate = iter / diff;

	cout << "...took " << diff << " seconds. ("
		<< prevrate << " per second)." << endl;
}

#define SAVESTRATEGYFILE 0
#define TESTING 0

inline void playgame()
{
	cout << "starting work..." << endl;
	long long i=25*THOUSAND;
	long long total=0;
	simulate(1000);
	simulate(i-1000);
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
#if __GNUC__ && NUM_THREADS > 1
	initlocks();
#endif

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

