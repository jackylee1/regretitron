#include "stdafx.h"
#include "solver.h"
#include <sstream>
#include <iomanip>
#include "../TinyXML++/tinyxml.h"
using namespace std;

//static data members
int64 Solver::iterations; //number of iterations remaining
int64 Solver::total = 0; //number of iterations done total
double Solver::inittime;
CardMachine * Solver::cardmachine = NULL;
const BettingTree * Solver::tree = NULL;
MemoryManager * Solver::memory = NULL;
#ifdef DO_THREADS
pthread_mutex_t * Solver::cardsilocks[4] = {NULL,NULL,NULL,NULL}; //one lock per player per gameround per cardsi
pthread_mutex_t Solver::threaddatalock = PTHREAD_MUTEX_INITIALIZER;
#endif


void Solver::initsolver()
{
	cardmachine = new CardMachine(CARDSETTINGS, true, SEED_RAND, SEED_WITH); //settings taken from solveparams.h
	tree = new BettingTree(TREESETTINGS); //the settings are taken from solveparams.h
	memory = new MemoryManager(*tree, *cardmachine);
	inittime = getdoubletime();

#ifdef DO_THREADS
	for(int gr=0; gr<4; gr++)
	{
		cardsilocks[gr] = new pthread_mutex_t[2*cardmachine->getcardsimax(gr)];
		for(int i=0; i<2*cardmachine->getcardsimax(gr); i++)
			pthread_mutex_init(&cardsilocks[gr][i], NULL);
	}
#endif
}

void Solver::destructsolver()
{
	delete cardmachine;
	delete tree;
	delete memory;
#ifdef DO_THREADS
	for(int gr=0; gr<4; gr++)
		delete[] cardsilocks[gr];
#endif
}

double Solver::solve(int64 iter) 
{ 
	total += iterations = iter; 

	Solver solvers[NUM_THREADS];
	double starttime = getdoubletime();
#ifdef DO_THREADS
	pthread_t threads[NUM_THREADS-1];
	for(int i=0; i<NUM_THREADS-1; i++)
		if(pthread_create(&threads[i], NULL, callthreadloop, &solvers[i+1]))
			REPORT("error creating thread");
#endif
	//use current thread too
	solvers[0].threadloop();
#ifdef DO_THREADS
	//wait for them all to return
	for(int i=0; i<NUM_THREADS-1; i++)
		if(pthread_join(threads[i],NULL))
			REPORT("error joining thread");
#endif
	return getdoubletime() - starttime;
}

#ifdef __GNUC__ //needed to print __float128 on linux
inline ostream& operator << (ostream& os, const __float128& n) { return os << (__float80)n; }
#endif


void Solver::save(const string &filename, bool writedata)
{
	cout << "saving XML settings file..." << endl;

	// save the data first to get the filesize

	int64 stratfilesize = writedata ? memory->save(filename, *cardmachine) : 0;

	// open up the xml

	TiXmlDocument doc;
	TiXmlDeclaration * decl = new TiXmlDeclaration("1.0","","");  
	doc.LinkEndChild(decl);
	TiXmlElement * root = new TiXmlElement("strategy");  
	root->SetAttribute("version", SAVE_FORMAT_VERSION);
	doc.LinkEndChild(root);  

	//strategy file, if saved

	TiXmlElement * file = new TiXmlElement("savefile");
	file->SetAttribute("filesize", stratfilesize);
	TiXmlText * filenametext = 
		new TiXmlText(writedata ? "strategy/"+filename+".strat" : "");
	file->LinkEndChild(filenametext);

	//run parameters; number of iterations

	TiXmlElement * run = new TiXmlElement("run");
	ostringstream totaltext1;
	totaltext1 << total;
	string totaltext2 = totaltext1.str();
	for(int i=totaltext2.length()-3; i>0; i-=3)
		totaltext2.insert(i, 1, ',');
	run->SetAttribute("iterations", totaltext2);

	//total time elapsed

	double elapsed = getdoubletime() - inittime;
	ostringstream timestr;
	timestr << fixed << setprecision(1);
	if(elapsed > 60.0*60.0*24.0*2.5)
		timestr << elapsed / (60.0*60.0*24.0) << " days";
	else if(elapsed > 60.0*60.0*2.0)
		timestr << elapsed / (60.0*60.0) << " hours";
	else if(elapsed > 60.0*2.0)
		timestr << elapsed / (60.0) << " mins";
	else
		timestr << elapsed << " secs";
	run->SetAttribute("totaltime", timestr.str());

	//rand seed and computer used

	if(SEED_RAND)
		run->SetAttribute("randseededwith", SEED_WITH);
	else
		run->SetAttribute("randseededwith", "time & clock");
	run->SetAttribute("numthreads", NUM_THREADS);
#if _WIN32
	run->SetAttribute("system", "win32");
#elif __GNUC__
	run->SetAttribute("system", "linux");
#else
	run->SetAttribute("system", "???");
#endif

	//floating point data types

	TiXmlElement * data = new TiXmlElement("data");
	data->SetAttribute("fpworking_type", FPWORKING_TYPENAME);
	data->SetAttribute("fpstore_type", FPSTORE_TYPENAME);

	//link xml node

	TiXmlElement * strat = new TiXmlElement("solver");
	strat->LinkEndChild(file);
	strat->LinkEndChild(run);
	strat->LinkEndChild(data);
	root->LinkEndChild(strat);

	//cards parameters

	TiXmlElement * cardbins = new TiXmlElement("cardbins");
	root->LinkEndChild(cardbins);

	TiXmlElement * meta = new TiXmlElement("meta");
	meta->SetAttribute("usehistory", cardmachine->getparams().usehistory);
	meta->SetAttribute("useflopalyzer", cardmachine->getparams().useflopalyzer);
	cardbins->LinkEndChild(meta);
	for(int gr=0; gr<4; gr++)
	{
		ostringstream o;
		o << "round" << gr;
		TiXmlElement * round = new TiXmlElement(o.str());
		round->SetAttribute("nbins", cardmachine->getparams().bin_max[gr]);
		round->SetAttribute("filesize", cardmachine->getparams().filesize[gr]);
		TiXmlText * roundtext = new TiXmlText(cardmachine->getparams().filename[gr]);
		round->LinkEndChild(roundtext);
		cardbins->LinkEndChild(round);
	}

	// tree parameters

	TiXmlElement * mytree = new TiXmlElement("tree");
	root->LinkEndChild(mytree);

	TiXmlElement * blinds = new TiXmlElement("blinds");
	blinds->SetAttribute("sblind", (int)tree->getparams().sblind);
	blinds->SetAttribute("bblind", (int)tree->getparams().bblind);
	mytree->LinkEndChild(blinds);

	TiXmlElement * bets = new TiXmlElement("bets");
	mytree->LinkEndChild(bets);
	for(int N=0; N<6; N++)
	{
		ostringstream raiseN, BN;
		raiseN << "raise" << N+1;
		BN << 'B' << N+1;
		bets->SetAttribute(BN.str(), (int)tree->getparams().bets[N]);
		TiXmlElement * raise = new TiXmlElement(raiseN.str());
		for(int M=0; M<6; M++)
		{
			ostringstream RNM;
			RNM << 'R' << N+1 << M+1;
			raise->SetAttribute(RNM.str(), (int)tree->getparams().raises[N][M]);
		}
		mytree->LinkEndChild(raise);
	}

	TiXmlElement * metanode = new TiXmlElement("meta");
	metanode->SetAttribute("stacksize", (int)tree->getparams().stacksize);
	metanode->SetAttribute("pushfold", tree->getparams().pushfold);
	metanode->SetAttribute("limit", tree->getparams().limit);
	mytree->LinkEndChild(metanode);

	// print the first node strategy to an ostringstream

	ostringstream text;
	text << endl << endl;
	BetNode mynode;
	tree->getnode(PREFLOP, 0, 0, mynode);
	const int &numa = mynode.numacts;
	for(int index=INDEX2_MAX-1; index>=0; index--)
	{
		//get string and cardsi

		string handstring;
		int cardsi = cardmachine->preflophandinfo(index, handstring);

		//get pointers to data

		fpstore_type * stratn, * regret;
#if STORE_DENOM
		fpstore_type * stratd;
#endif
		memory->dataindexing(PREFLOP, numa, 0, cardsi, stratn, regret
#if STORE_DENOM
				, stratd
#endif
				);

		//print out a heading including example hand

		if(tree->getparams().pushfold)
			text << setw(5) << left << ":" << handstring;
		else
		{
			text << endl << "cardsi: " << cardsi << ":" << endl;
			text << handstring << ":" << endl;
		}

		//compute the denominator
		
		fpworking_type denominator = 0;
		for(int i=0; i<numa; i++)
			denominator += stratn[i];

		//print out each action

		for (int a=0; a<numa; a++)
		{
			fpworking_type myprob = (fpworking_type)stratn[a] / denominator;

			//aiming for http://www.daimi.au.dk/~bromille/pokerdata/SingleHandSmallBlind/SHSB.4000
			if(tree->getparams().pushfold && myprob>0.98 && a==numa-1) 
				text << "jam" << endl;
			else if(tree->getparams().pushfold && myprob>0.98) 
				text << "fold" << endl;
			else if(!tree->getparams().pushfold || myprob>0.02)
				text << setw(14) << tree->actionstring(PREFLOP,a,mynode,1.0)+": " << 100*myprob << "%" << endl;
		}

		//print out denominator info if exists

#if STORE_DENOM
		text << "Sum of strats:  " << denominator / *stratd << endl;
		text << "stratd:         " << *stratd << endl;
		text << "sum(stratn[a]): " << denominator << endl;
		text << "difference:     " << *stratd - denominator << endl;
#endif
	}
	text << endl << endl;

	// add that string as the first node strategy

	TiXmlText * text2 = new TiXmlText(text.str());
	text2->SetCDATA(true);
	TiXmlElement * firstnode = new TiXmlElement("smallblindpreflopstrategy");
	firstnode->LinkEndChild(text2);
	root->LinkEndChild(firstnode);

	// save the xml file.

	doc.SaveFile(("output/"+filename+".xml").c_str());
}

//must be static because pthreads do not understand classes.
//the void* parameter is a pointer to solver instance (i.e. the 'this' pointer).
void* Solver::callthreadloop(void* mysolver)
{
	((Solver *) mysolver)->threadloop();
	return NULL;
}

void Solver::threadloop()
{
	while(1)
	{
		for(int i=0; i<4; i++) 
			for(int j=0; j<MAX_NODETYPES; j++)
				actioncounters[i][j] = 0;
#ifdef DO_THREADS
		pthread_mutex_lock(&threaddatalock);
#endif
		if(iterations==0)
		{
#ifdef DO_THREADS
			pthread_mutex_unlock(&threaddatalock);
#endif
			break; //nothing left to be done
		}
		iterations--;
		cardmachine->getnewgame(cardsi, twoprob0wins);
#ifdef DO_THREADS
		pthread_mutex_lock(&cardsilocks[PREFLOP][cardsi[PREFLOP][P0]*2 + P0]);
		pthread_mutex_lock(&cardsilocks[PREFLOP][cardsi[PREFLOP][P1]*2 + P1]);
		pthread_mutex_lock(&cardsilocks[FLOP][cardsi[FLOP][P0]*2 + P0]);
		pthread_mutex_lock(&cardsilocks[FLOP][cardsi[FLOP][P1]*2 + P1]);
		pthread_mutex_lock(&cardsilocks[TURN][cardsi[TURN][P0]*2 + P0]);
		pthread_mutex_lock(&cardsilocks[TURN][cardsi[TURN][P1]*2 + P1]);
		pthread_mutex_lock(&cardsilocks[RIVER][cardsi[RIVER][P0]*2 + P0]);
		pthread_mutex_lock(&cardsilocks[RIVER][cardsi[RIVER][P1]*2 + P1]);
		pthread_mutex_unlock(&threaddatalock);
#endif
		walker(PREFLOP,0,0,1,1);
#ifdef DO_THREADS
		pthread_mutex_unlock(&cardsilocks[PREFLOP][cardsi[PREFLOP][P0]*2 + P0]);
		pthread_mutex_unlock(&cardsilocks[PREFLOP][cardsi[PREFLOP][P1]*2 + P1]);
		pthread_mutex_unlock(&cardsilocks[FLOP][cardsi[FLOP][P0]*2 + P0]);
		pthread_mutex_unlock(&cardsilocks[FLOP][cardsi[FLOP][P1]*2 + P1]);
		pthread_mutex_unlock(&cardsilocks[TURN][cardsi[TURN][P0]*2 + P0]);
		pthread_mutex_unlock(&cardsilocks[TURN][cardsi[TURN][P1]*2 + P1]);
		pthread_mutex_unlock(&cardsilocks[RIVER][cardsi[RIVER][P0]*2 + P0]);
		pthread_mutex_unlock(&cardsilocks[RIVER][cardsi[RIVER][P1]*2 + P1]);
#endif
	}
}


fpworking_type Solver::walker(int gr, int pot, int beti, fpworking_type prob0, fpworking_type prob1)
{

	//obtain definition of possible bets for this turn

	BetNode mynode;
	tree->getnode(gr, pot, beti, mynode);
	const int &numa = mynode.numacts; //for ease of typing

	//obtain pointers to data for this turn

	fpstore_type * stratn, * regret;
#if STORE_DENOM
	fpstore_type * stratd;
#endif
	if(numa-2 < 0 || numa-2 >= MAX_NODETYPES) REPORT("gcc was right.");
	memory->dataindexing(gr, numa, actioncounters[gr][numa-2]++, cardsi[gr][(int)mynode.playertoact], 
		stratn, regret
#if STORE_DENOM
		, stratd
#endif
		);//try out some gcc builtins here for prefetching


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
			case BetNode::NA:
				REPORT("invalid tree");
			case BetNode::FD:
			case BetNode::AI:
				continue;
			case BetNode::GO:
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
		case BetNode::NA: //error
			REPORT("Invalid betting tree node action reached."); //will exit

		case BetNode::AI: //called allin
			utility[i] = tree->getparams().stacksize * (twoprob0wins-1);
			break;

		case BetNode::GO: //next game round
			if(gr==RIVER)// showdown
				utility[i] = (pot+mynode.potcontrib[i]) * (twoprob0wins-1);
			else 
			{
				if(mynode.playertoact==0)
					utility[i] = walker(gr+1, pot+mynode.potcontrib[i], 0, prob0*stratt[i], prob1);
				else
					utility[i] = walker(gr+1, pot+mynode.potcontrib[i], 0, prob0, prob1*stratt[i]);
			}
			break;

		case BetNode::FD: //fold
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
			for(int a=0; a<numa; a++)
				stratn[a] += prob0 * stratt[a];
#if STORE_DENOM
			*stratd += prob0;
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
			for(int a=0; a<numa; a++)
				stratn[a] += prob1 * stratt[a];
#if STORE_DENOM
			*stratd += prob1;
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

void Solver::dummywalker(int gr, int pot, int beti)
{
	BetNode mynode;
	tree->getnode(gr, pot, beti, mynode);
	const int &numa = mynode.numacts;

	if(numa-2 < 0 || numa-2 >= MAX_NODETYPES) REPORT("gcc was right.");
	actioncounters[gr][numa-2]++;

	for(int a=0; a<numa; a++)
	{
		switch(mynode.result[a])
		{
		case BetNode::NA:
			REPORT("invalid tree");
		case BetNode::FD:
		case BetNode::AI:
			continue;
		case BetNode::GO:
			if(gr!=RIVER)
				dummywalker(gr+1, pot+mynode.potcontrib[a], 0);
			continue;

		default://child node
			dummywalker(gr, pot, mynode.result[a]);
			continue;
		}
	}
}

