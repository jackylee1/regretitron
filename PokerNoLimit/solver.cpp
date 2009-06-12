#include "stdafx.h"
#include "solver.h"
#include <sstream>
#include <iomanip>
#include <numeric> //for accumulate
#include "../TinyXML++/tinyxml.h"
using namespace std;

//static data members
int64 Solver::iterations; //number of iterations remaining
int64 Solver::total = 0; //number of iterations done total
double Solver::inittime;
CardMachine * Solver::cardmachine = NULL;
BettingTree * Solver::tree = NULL;
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
	GETDBLTIME(inittime);

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
	double starttime, finishtime;
	GETDBLTIME(starttime);
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
	GETDBLTIME(finishtime);
	return finishtime-starttime;
}

#ifdef __GNUC__ //needed to print __float128 on linux
inline ostream& operator << (ostream& os, const __float128& n) { return os << (__float80)n; }
#endif


void Solver::save(const string &filename, bool writedata)
{

	// save the data first to get the filesize

	int64 stratfilesize = writedata ? memory->save(filename, *cardmachine) : 0;

	// open up the xml

	TiXmlDocument doc;
	TiXmlDeclaration * decl = new TiXmlDeclaration("1.0","","");  
	doc.LinkEndChild(decl);
	TiXmlElement * root = new TiXmlElement("paramsv1");  
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

	double elapsed;
	GETDBLTIME(elapsed);
	elapsed -= inittime;
	ostringstream timestr;
	timestr << fixed << setprecision(1);
	if(elapsed > 60.0*60.0*24.0*2.5)
		timestr << elapsed / (60.0*60.0*24.0*2.5) << " days";
	else if(elapsed > 60.0*60.0*2.0)
		timestr << elapsed / (60.0*60.0*2.0) << " hours";
	else if(elapsed > 60.0*2.0)
		timestr << elapsed / (60.0*2.0) << " mins";
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

	//bin parameters

	TiXmlElement * bins = new TiXmlElement("bins");
	root->LinkEndChild(bins);

	for(int gr=0; gr<4; gr++)
	{
		ostringstream o;
		o << "round" << gr;
		TiXmlElement * round = new TiXmlElement(o.str());
		round->SetAttribute("nbins", CARDSETTINGS.bin_max[gr]);
		round->SetAttribute("filesize", CARDSETTINGS.filesize[gr]);
		TiXmlText * roundtext = new TiXmlText(CARDSETTINGS.filename[gr]);
		round->LinkEndChild(roundtext);
		bins->LinkEndChild(round);
	}

	// tree parameters

	TiXmlElement * blinds = new TiXmlElement("blinds");
	blinds->SetAttribute("sblind", (int)TREESETTINGS.sblind);
	blinds->SetAttribute("bblind", (int)TREESETTINGS.bblind);

	TiXmlElement * bets = new TiXmlElement("bets");
	bets->SetAttribute("B1", (int)TREESETTINGS.bets[0]);
	bets->SetAttribute("B2", (int)TREESETTINGS.bets[1]);
	bets->SetAttribute("B3", (int)TREESETTINGS.bets[2]);
	bets->SetAttribute("B4", (int)TREESETTINGS.bets[3]);
	bets->SetAttribute("B5", (int)TREESETTINGS.bets[4]);
	bets->SetAttribute("B6", (int)TREESETTINGS.bets[5]);

	TiXmlElement * raise1 = new TiXmlElement("raise1");
	raise1->SetAttribute("R11", (int)TREESETTINGS.raises[0][0]);
	raise1->SetAttribute("R12", (int)TREESETTINGS.raises[0][1]);
	raise1->SetAttribute("R13", (int)TREESETTINGS.raises[0][2]);
	raise1->SetAttribute("R14", (int)TREESETTINGS.raises[0][3]);
	raise1->SetAttribute("R15", (int)TREESETTINGS.raises[0][4]);
	raise1->SetAttribute("R16", (int)TREESETTINGS.raises[0][5]);

	TiXmlElement * raise2 = new TiXmlElement("raise2");
	raise2->SetAttribute("R21", (int)TREESETTINGS.raises[1][0]);
	raise2->SetAttribute("R22", (int)TREESETTINGS.raises[1][1]);
	raise2->SetAttribute("R23", (int)TREESETTINGS.raises[1][2]);
	raise2->SetAttribute("R24", (int)TREESETTINGS.raises[1][3]);
	raise2->SetAttribute("R25", (int)TREESETTINGS.raises[1][4]);
	raise2->SetAttribute("R26", (int)TREESETTINGS.raises[1][5]);

	TiXmlElement * raise3 = new TiXmlElement("raise3");
	raise3->SetAttribute("R31", (int)TREESETTINGS.raises[2][0]);
	raise3->SetAttribute("R32", (int)TREESETTINGS.raises[2][1]);
	raise3->SetAttribute("R33", (int)TREESETTINGS.raises[2][2]);
	raise3->SetAttribute("R34", (int)TREESETTINGS.raises[2][3]);
	raise3->SetAttribute("R35", (int)TREESETTINGS.raises[2][4]);
	raise3->SetAttribute("R36", (int)TREESETTINGS.raises[2][5]);

	TiXmlElement * raise4 = new TiXmlElement("raise4");
	raise4->SetAttribute("R41", (int)TREESETTINGS.raises[3][0]);
	raise4->SetAttribute("R42", (int)TREESETTINGS.raises[3][1]);
	raise4->SetAttribute("R43", (int)TREESETTINGS.raises[3][2]);
	raise4->SetAttribute("R44", (int)TREESETTINGS.raises[3][3]);
	raise4->SetAttribute("R45", (int)TREESETTINGS.raises[3][4]);
	raise4->SetAttribute("R46", (int)TREESETTINGS.raises[3][5]);

	TiXmlElement * raise5 = new TiXmlElement("raise5");
	raise5->SetAttribute("R51", (int)TREESETTINGS.raises[4][0]);
	raise5->SetAttribute("R52", (int)TREESETTINGS.raises[4][1]);
	raise5->SetAttribute("R53", (int)TREESETTINGS.raises[4][2]);
	raise5->SetAttribute("R54", (int)TREESETTINGS.raises[4][3]);
	raise5->SetAttribute("R55", (int)TREESETTINGS.raises[4][4]);
	raise5->SetAttribute("R56", (int)TREESETTINGS.raises[4][5]);

	TiXmlElement * raise6 = new TiXmlElement("raise6");
	raise6->SetAttribute("R61", (int)TREESETTINGS.raises[5][0]);
	raise6->SetAttribute("R62", (int)TREESETTINGS.raises[5][1]);
	raise6->SetAttribute("R63", (int)TREESETTINGS.raises[5][2]);
	raise6->SetAttribute("R64", (int)TREESETTINGS.raises[5][3]);
	raise6->SetAttribute("R65", (int)TREESETTINGS.raises[5][4]);
	raise6->SetAttribute("R66", (int)TREESETTINGS.raises[5][5]);

	TiXmlElement * sspushfold = new TiXmlElement("sspf");
	sspushfold->SetAttribute("stacksize", (int)TREESETTINGS.stacksize);
	sspushfold->SetAttribute("pushfold", TREESETTINGS.pushfold);

	TiXmlElement * mytree = new TiXmlElement("tree");
	mytree->LinkEndChild(blinds);
	mytree->LinkEndChild(bets);
	mytree->LinkEndChild(raise1);
	mytree->LinkEndChild(raise2);
	mytree->LinkEndChild(raise3);
	mytree->LinkEndChild(raise4);
	mytree->LinkEndChild(raise5);
	mytree->LinkEndChild(raise6);
	mytree->LinkEndChild(sspushfold);
	root->LinkEndChild(mytree);

	// print the first node strategy to an ostringstream

	ostringstream text;
	text << endl << endl;
	BetNode mynode;
	tree->getnode(PREFLOP, 0, 0, mynode);
	const int &numa = mynode.numacts;
	for(int cardsi=cardmachine->getcardsimax(PREFLOP)-1; cardsi>=0; cardsi--)
	{

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
			text << setw(5) << left << ":" << cardmachine->preflophandstring(cardsi);
		else
		{
			text << endl << "cardsi: " << cardsi << ":" << endl;
			text << cardmachine->preflophandstring(cardsi) << ":" << endl;
		}

		//compute the denominator
		
		fpworking_type denominator = accumulate(stratn, stratn+numa, (fpworking_type)0);

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

