#include "stdafx.h"
#include "solver.h"
#include <sstream>
#include <iomanip>
#include <math.h> //sqrt
#include <algorithm> //sort
#include <numeric> //accumulate
#include "../TinyXML++/tinyxml.h"
using namespace std;

//static data members
int64 Solver::iterations; //number of iterations remaining
int64 Solver::total = 0; //number of iterations done total
double Solver::inittime;
CardMachine * Solver::cardmachine = NULL;
BettingTree * Solver::tree = NULL;
Vertex Solver::treeroot;
MemoryManager * Solver::memory = NULL;
#ifdef DO_THREADS
pthread_mutex_t Solver::threaddatalock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t Solver::signaler;
list<Solver::iteration_data_t> Solver::dataqueue(N_LOOKAHEAD);
bool Solver::datainuse0[PFLOP_CARDSI_MAX*2];
#if !USE_HISTORY //then we need to account for all data in use, not just first round
#error hello
bool Solver::datainuse1[FLOP_CARDSI_MAX*2];
bool Solver::datainuse2[TURN_CARDSI_MAX*2];
bool Solver::datainuse3[RIVER_CARDSI_MAX*2];
#endif
#endif


void Solver::initsolver()
{
	cardmachine = new CardMachine(CARDSETTINGS, true, SEED_RAND, SEED_WITH); //settings taken from solveparams.h
	tree = new BettingTree(TREESETTINGS); //the settings are taken from solveparams.h
	treeroot = createtree(*tree);
	memory = new MemoryManager(*tree, *cardmachine);
	inittime = getdoubletime();

#ifdef DO_THREADS
	pthread_cond_init(&signaler, NULL);

	for(int i=0; i<PFLOP_CARDSI_MAX*2; i++)
		datainuse0[i] = false;
#if !USE_HISTORY 
	for(int i=0; i<FLOP_CARDSI_MAX*2; i++)
		datainuse1[i] = false;
	for(int i=0; i<TURN_CARDSI_MAX*2; i++)
		datainuse2[i] = false;
	for(int i=0; i<RIVER_CARDSI_MAX*2; i++)
		datainuse3[i] = false;
#endif
	for(list<iteration_data_t>::iterator data = dataqueue.begin(); data!=dataqueue.end(); data++)
		cardmachine->getnewgame(data->cardsi, data->twoprob0wins); //get new data
#endif
}

void Solver::destructsolver()
{
	delete cardmachine;
	delete tree;
	delete memory;
}

//function called by main()
//returns seconds taken to do this many iter
double Solver::solve(int64 iter)  
{ 
	double starttime = getdoubletime();
	doiter(iter);
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

	//link xml node..

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

	TiXmlElement * metanode = new TiXmlElement("meta");
	metanode->SetAttribute("stacksize", get_property(*tree, settings_tag()).stacksize);
	metanode->SetAttribute("pushfold", get_property(*tree, settings_tag()).pushfold);
	metanode->SetAttribute("limit", get_property(*tree, settings_tag()).limit);
	mytree->LinkEndChild(metanode);

	TiXmlElement * blinds = new TiXmlElement("blinds");
	blinds->SetAttribute("sblind", get_property(*tree, settings_tag()).sblind);
	blinds->SetAttribute("bblind", get_property(*tree, settings_tag()).bblind);
	mytree->LinkEndChild(blinds);

	TiXmlElement * bets = new TiXmlElement("bets");
	mytree->LinkEndChild(bets);
	for(int N=0; N<6; N++)
	{
		ostringstream raiseN, BN;
		raiseN << "raise" << N+1;
		BN << 'B' << N+1;
		bets->SetAttribute(BN.str(), get_property(*tree, settings_tag()).bets[N]);
		TiXmlElement * raise = new TiXmlElement(raiseN.str());
		for(int M=0; M<6; M++)
		{
			ostringstream RNM;
			RNM << 'R' << N+1 << M+1;
			raise->SetAttribute(RNM.str(), get_property(*tree, settings_tag()).raises[N][M]);
		}
		mytree->LinkEndChild(raise);
	}

	// save the xml file.

	doc.SaveFile((filename+".xml").c_str());
}

void Solver::doiter(int64 iter) 
{ 
	total += iter; 
	iterations = iter - N_LOOKAHEAD;

	Solver solvers[NUM_THREADS];
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

	//do the last few unthreaded for consistency (to clear the queue)
	iterations = N_LOOKAHEAD;
	solvers[0].threadloop();
}


//must be static because pthreads do not understand classes.
//the void* parameter is a pointer to solver instance (i.e. the 'this' pointer).
void* Solver::callthreadloop(void* mysolver)
{
	((Solver *) mysolver)->threadloop();
	return NULL;
}

inline bool check(int a1, int a2, int b1, int b2)
{
	return a1 != b1 && a1 != b2 && a2 != b1 && a2 != b2;
}

void Solver::threadloop()
{
	fpu_fix_start(NULL);

#ifdef DO_THREADS //MULTI THREADED
	pthread_mutex_lock(&threaddatalock);
#endif //ALL THEADEDNESS

	while(1) //each loop does one iteration
	{
#ifdef DO_THREADS //MULTI THREADED
		list<iteration_data_t>::iterator my_data_it; //iterator pointing to my data I want to use
		while(1) //each loop checks for available data
		{
			if(iterations==0) //check if we're all done.
			{
				pthread_cond_broadcast(&signaler); // wake up any sleeping threads
				pthread_mutex_unlock(&threaddatalock);
				return;
			}

			for(my_data_it = dataqueue.begin(); my_data_it!=dataqueue.end(); my_data_it++)
			{
				if(!datainuse0[2*my_data_it->cardsi[PREFLOP][P0]] && !datainuse0[2*my_data_it->cardsi[PREFLOP][P1]+1]
#if !USE_HISTORY
					&& !datainuse1[2*my_data_it->cardsi[FLOP][P0]] && !datainuse1[2*my_data_it->cardsi[FLOP][P1]+1]
					&& !datainuse2[2*my_data_it->cardsi[TURN][P0]] && !datainuse2[2*my_data_it->cardsi[TURN][P1]+1]
					&& !datainuse3[2*my_data_it->cardsi[RIVER][P0]] && !datainuse3[2*my_data_it->cardsi[RIVER][P1]+1]
#endif
				)
				{
					for(list<iteration_data_t>::iterator to_be_skipped=dataqueue.begin(); to_be_skipped != my_data_it; to_be_skipped++)
					{
						if(my_data_it->cardsi[PREFLOP][P0] == to_be_skipped->cardsi[PREFLOP][P0] 
							|| my_data_it->cardsi[PREFLOP][P1] == to_be_skipped->cardsi[PREFLOP][P1]
#if !USE_HISTORY
							|| my_data_it->cardsi[FLOP][P0] == to_be_skipped->cardsi[FLOP][P0]
							|| my_data_it->cardsi[FLOP][P1] == to_be_skipped->cardsi[FLOP][P1]
							|| my_data_it->cardsi[TURN][P0] == to_be_skipped->cardsi[TURN][P0]
							|| my_data_it->cardsi[TURN][P1] == to_be_skipped->cardsi[TURN][P1]
							|| my_data_it->cardsi[RIVER][P0] == to_be_skipped->cardsi[RIVER][P0]
							|| my_data_it->cardsi[RIVER][P1] == to_be_skipped->cardsi[RIVER][P1]
#endif
						)
						{
							goto cannot_skip;
						}
					}
					goto found_good_data;
				}
cannot_skip:
				continue; // just has to be here as a statement for the cannot_skip label
			}
			pthread_cond_wait(&signaler, &threaddatalock);
		}
found_good_data:

		if(THREADLOOPTRACE && false)
		{
			int j=0;
			for(list<iteration_data_t>::iterator i=dataqueue.begin(); i != my_data_it; i++)
				j++;
			if(j>0)
				cerr << this << ": ..skipped " << j << "data.." << endl;
		}
		memcpy(cardsi, my_data_it->cardsi, sizeof(cardsi)); //copy that data into our non-static variables
		twoprob0wins = my_data_it->twoprob0wins;

		datainuse0[2*cardsi[PREFLOP][P0]] = true;
		datainuse0[2*cardsi[PREFLOP][P1]+1] = true;
#if !USE_HISTORY
		datainuse1[2*cardsi[FLOP][P0]] = true;
		datainuse1[2*cardsi[FLOP][P1]+1] = true;
		datainuse2[2*cardsi[TURN][P0]] = true;
		datainuse2[2*cardsi[TURN][P1]+1] = true;
		datainuse3[2*cardsi[RIVER][P0]] = true;
		datainuse3[2*cardsi[RIVER][P1]+1] = true;
#endif
		cardmachine->getnewgame(my_data_it->cardsi, my_data_it->twoprob0wins); //get new data
		dataqueue.splice(dataqueue.end(), dataqueue, my_data_it); //move that node to the end of the list
		iterations--;
		if(THREADLOOPTRACE)
			cerr << /*this <<*/ ": Solving " << cardsi[PREFLOP][P0] << " - " << cardsi[PREFLOP][P1] << endl;
		pthread_mutex_unlock(&threaddatalock);
#else //SINGLE THREADED
		if(iterations-- == 0)
			break;
		cardmachine->getnewgame(cardsi, twoprob0wins);
		if(THREADLOOPTRACE)
			cerr << /*this <<*/ ": Solving " << cardsi[PREFLOP][P0] << " - " << cardsi[PREFLOP][P1] << endl;
#endif //ALL THREADEDNESS
		walker(PREFLOP,0,treeroot,1,1);
#ifdef DO_THREADS //MULTI THREADED
		pthread_mutex_lock(&threaddatalock);
		if(THREADLOOPTRACE && false)
			cerr << this << ": Done with " << cardsi[PREFLOP][P0] << " - " << cardsi[PREFLOP][P1] << endl;
		//set datainuse of whatever we were doing to false
		datainuse0[2*cardsi[PREFLOP][P0]] = false;
		datainuse0[2*cardsi[PREFLOP][P1]+1] = false;
#if !USE_HISTORY
		datainuse1[2*cardsi[FLOP][P0]] = false;
		datainuse1[2*cardsi[FLOP][P1]+1] = false;
		datainuse2[2*cardsi[TURN][P0]] = false;
		datainuse2[2*cardsi[TURN][P1]+1] = false;
		datainuse3[2*cardsi[RIVER][P0]] = false;
		datainuse3[2*cardsi[RIVER][P1]+1] = false;
#endif
		pthread_cond_signal(&signaler); //new data is availble to be touched, so signal
#endif
	}
}

inline pair<fpworking_type, fpworking_type> utilpair(fpworking_type p0utility)
{
	if(p0utility>0)
		return make_pair(AGGRESSION_FACTOR*p0utility, -p0utility);
	else
		return make_pair(p0utility, -AGGRESSION_FACTOR*p0utility);
}


pair<fpworking_type,fpworking_type> Solver::walker(int gr, int pot, Vertex node, fpworking_type prob0, fpworking_type prob1)
{
	const int numa = out_degree(node, *tree);
	const int playeri = playerindex((*tree)[node].type);

	//obtain pointers to data for this turn

#if STORE_DENOM
	fpstore_type * stratn, * regret, * stratd;
	memory->dataindexing(gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], stratn, regret, stratd);
#else
	fpstore_type * stratn, * regret;
	memory->dataindexing(gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], stratn, regret);
#endif
	
	//find total regret

	fpworking_type totalregret=0.0;

	for(int i=0; i<numa; i++)
		if (regret[i]>0.0) totalregret += regret[i];

	//set strategy proportional to positive regret, or 1/numa if no positive regret

	fpworking_type stratt[MAX_ACTIONS];

	if (totalregret > 0.0)
		for(int i=0; i<numa; i++)
			(regret[i]>0.0) ? stratt[i] = regret[i] / totalregret : stratt[i] = 0.0;
	else
		for(int i=0; i<numa; i++)
			stratt[i] = (fpworking_type)1/(fpworking_type)numa;

	//debug printing

	if(WALKERDEBUG)
	{
		cout << "Starting Gameround: " << gr << /*" Pot: " << pot <<*/ " Actioni: " << (*tree)[node].actioni << " Cardsi: " << cardsi[gr][playeri] << endl;
		cout << "Prob0: " << prob0 << " Prob1: " << prob1 << " Stratt[]: { ";
		for(int i=0; i<numa; i++) cout << stratt[i] << " ";
		cout << "}" << endl << endl;
	}

	//recursively find utility of each action

	pair<fpworking_type,fpworking_type> utility[MAX_ACTIONS];
	pair<fpworking_type,fpworking_type> avgutility(0,0);

	EIter e, elast;
	int i=0;
	for(tie(e, elast) = out_edges(node, *tree); e!=elast; e++, i++)
	{
		if(stratt[i]==0.0 && (((*tree)[node].type==P0Plays && prob1==0.0) || ((*tree)[node].type==P1Plays && prob0==0.0)) )
			continue; //utility will be unused, children's regret will be unaffected

		switch((*tree)[*e].type)
		{
		case CallAllin: //showdown
			utility[i] = utilpair(get_property(*tree, settings_tag()).stacksize * (twoprob0wins-1));
			break;

		case Call:
			if(gr==RIVER) //showdown
				utility[i] = utilpair((pot+(*tree)[*e].potcontrib) * (twoprob0wins-1));
			else //next game round
			{
				if((*tree)[node].type==P0Plays)
					utility[i] = walker(gr+1, pot + (*tree)[*e].potcontrib, target(*e, *tree), prob0*stratt[i], prob1);
				else
					utility[i] = walker(gr+1, pot + (*tree)[*e].potcontrib, target(*e, *tree), prob0, prob1*stratt[i]);
			}
			break;

		case Fold:
			utility[i] = utilpair((pot+(*tree)[*e].potcontrib) * (2*playeri - 1)); //acting player is loser, utility is integer
			break;

		case Bet:
		case BetAllin:
			if((*tree)[node].type==P0Plays)
				utility[i] = walker(gr, pot, target(*e, *tree), prob0*stratt[i], prob1);
			else
				utility[i] = walker(gr, pot, target(*e, *tree), prob0, prob1*stratt[i]);
		}

		avgutility.first += stratt[i]*utility[i].first;
		avgutility.second += stratt[i]*utility[i].second;
	}

	//debug printing

	if(WALKERDEBUG)
	{
		cout << "Ending Gameround: " << gr << /*" Pot: " << pot <<*/ " Actioni: " << (*tree)[node].actioni << " Cardsi: " << cardsi[gr][playeri] << endl;
		cout << "Utility[].first: { ";
		for(int i=0; i<numa; i++) cout << utility[i].first << " ";
		cout << "} Utility[].second: { ";
		for(int i=0; i<numa; i++) cout << utility[i].second << " ";
		cout << "}" << endl << endl;
	}

	//update the average strategy and the regret totals from the results from walking the tree above.

	if ((*tree)[node].type==P0Plays) //P0 playing, use prob1, proability of player 1 getting here.
	{
		//shortcut, along with the related one below, speeds up by 10% or so.
		if(prob0!=0.0)
		{
			for(int a=0; a<numa; a++)
				stratn[a] += prob0 * stratt[a];
#if STORE_DENOM
			*stratd += prob0;
#endif
		}

		//shortcut
		if(prob1==0.0) return avgutility;

		for(int a=0; a<numa; a++)
			regret[a] += prob1 * (utility[a].first - avgutility.first);
	}
	else // P1 playing, so his regret values are negative of P0's regret values.
	{
		//shortcut
		if(prob1!=0.0)
		{
			for(int a=0; a<numa; a++)
				stratn[a] += prob1 * stratt[a];
#if STORE_DENOM
			*stratd += prob1;
#endif
		}

		//shortcut
		if(prob0==0.0) return avgutility;

		for(int a=0; a<numa; a++)
			regret[a] += prob0 * (utility[a].second - avgutility.second);
	}


	//FINALLY, OUR LAST ACTION. WE WILL RETURN THE AVERAGE UTILITY, UNDER THE COMPUTED STRAT
	//SO THAT THE CALLER CAN KNOW THE EXPECTED VALUE OF THIS PATH. GOD SPEED CALLER.

	return avgutility;
}


