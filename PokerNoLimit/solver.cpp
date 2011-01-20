#include "stdafx.h"
#include "solver.h"
#include <sstream>
#include <iomanip>
#include <math.h> //sqrt
#include <algorithm> //sort
#include <numeric> //accumulate
#include "../TinyXML++/tinyxml.h"
#include <boost/static_assert.hpp>
#include <signal.h>
using namespace std;
using boost::tuple;

//static data members
int64 Solver::iterations; //number of iterations remaining
int64 Solver::total = 0; //number of iterations done total
double Solver::inittime;
double Solver::secondscompacting = 0;
int64 Solver::numbercompactings = 0;
CardMachine * Solver::cardmachine = NULL;
BettingTree * Solver::tree = NULL;
Vertex Solver::treeroot;
MemoryManager * Solver::memory = NULL;
#ifdef DO_THREADS
pthread_mutex_t Solver::threaddatalock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t Solver::signaler = PTHREAD_COND_INITIALIZER;
list<Solver::iteration_data_t> Solver::dataqueue(N_LOOKAHEAD);
bitset<PFLOP_CARDSI_MAX> Solver::dataguardpflopp0; //default constructor sets these to all false
bitset<PFLOP_CARDSI_MAX> Solver::dataguardpflopp1;
bitset<RIVER_CARDSI_MAX> Solver::dataguardriver; //needed for memory contention in HugeBuffer, works for imperfect recall too
#if IMPERFECT_RECALL /*then we need to account for all data in use, not just first round*/
bitset<FLOP_CARDSI_MAX> Solver::dataguardflopp0;
bitset<FLOP_CARDSI_MAX> Solver::dataguardflopp1;
bitset<TURN_CARDSI_MAX> Solver::dataguardturnp0;
bitset<TURN_CARDSI_MAX> Solver::dataguardturnp1;
#endif
#endif

void Solver::control_c(int sig)
{
	cout << "\033[2D"; // back up 2 chars to erase the ^C
	if(memory!=NULL)
	{
		cout << "User requested compact.." << endl;
		memory->SetMasterCompactFlag();
	}
}

void Solver::initsolver()
{
//	signal(SIGINT, control_c); // Register the signal handler for the SIGINT signal (Ctrl+C)

	tree = new BettingTree(TREESETTINGS); //the settings are taken from solveparams.h
	treeroot = createtree(*tree, MAX_ACTIONS_SOLVER);
	cardmachine = new CardMachine(CARDSETTINGS, true, SEED_RAND, SEED_WITH); //settings taken from solveparams.h
	memory = new MemoryManager(*tree, *cardmachine);
	inittime = getdoubletime();

#ifdef DO_THREADS
	for(list<iteration_data_t>::iterator data = dataqueue.begin(); data!=dataqueue.end(); data++)
		cardmachine->getnewgame(data->cardsi, data->twoprob0wins); //get new data
#endif
}

void Solver::destructsolver()
{
#ifdef DO_THREADS
	pthread_mutex_destroy(&threaddatalock);
	pthread_cond_destroy(&signaler);
#endif
	delete memory;
	delete cardmachine;
	delete tree;
}

#ifdef __GNUC__ //needed to print __float128 on linux
inline ostream& operator << (ostream& os, const __float128& n) { return os << (__float80)n; }
#endif

void Solver::save(const string &filename, bool writedata)
{
	cout << "saving XML settings file..." << endl;

	// save the data first to get the filesize

	int64 stratfilesize = writedata ? memory->save(filename) : 0;

	// open up the xml

	TiXmlDocument doc;
	TiXmlDeclaration * decl = new TiXmlDeclaration("1.0","","");  
	doc.LinkEndChild(decl);
	TiXmlElement * root = new TiXmlElement("strategy");  
	root->SetAttribute("version", SAVE_FORMAT_VERSION);
	doc.LinkEndChild(root);  

	//strategy file, if saved

	TiXmlElement * file = new TiXmlElement("savefile");
	file->SetAttribute("filesize", tostring(stratfilesize)); //TiXML borks at the 64-bit int
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
	for(unsigned i=0; i<sizeof(TYPENAMES)/sizeof(TYPENAMES[0]); i++)
		data->SetAttribute(TYPENAMES[i][0], TYPENAMES[i][1]);

	//solve parameters

	TiXmlElement * params = new TiXmlElement("parameters");
	params->SetAttribute("aggression", AGGRESSION_FACTOR);
	params->SetAttribute("raketype", RAKE_TYPE);

	//link xml node..

	TiXmlElement * strat = new TiXmlElement("solver");
	strat->LinkEndChild(file);
	strat->LinkEndChild(run);
	strat->LinkEndChild(params);
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

Solver solvers[NUM_THREADS]; //global so threadlooptrace can work

//function called by main()
tuple<
	double, //time taken
	double, //seconds compacting this cycle
	double, //cumulative seconds compacting
	int64, //number of compactings this cycle
	int64, //bytes used
	int64 //total bytes available
>
Solver::solve(int64 iter) 
{ 
	double starttime = getdoubletime();
	double secondscompactingstarted = secondscompacting;
	int64 ncompactingsstarted = numbercompactings;
	total += iter - N_LOOKAHEAD; 
	iterations = iter - N_LOOKAHEAD;

	//Solver solvers[NUM_THREADS];
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
	total += N_LOOKAHEAD; 
	iterations = N_LOOKAHEAD;
	solvers[0].threadloop();

	//return much useful data
	return boost::make_tuple(
			getdoubletime() - starttime,
			secondscompacting - secondscompactingstarted,
			secondscompacting,
			numbercompactings - ncompactingsstarted,
			memory->CompactMemory(),
			memory->GetHugeBufferSize()
			);
}


//must be static because pthreads do not understand classes.
//the void* parameter is a pointer to solver instance (i.e. the 'this' pointer).
void* Solver::callthreadloop(void* mysolver)
{
	((Solver *) mysolver)->threadloop();
	return NULL;
}

inline void Solver::docompact()
{
	double timestart = getdoubletime();
	memory->CompactMemory();
	memory->ClearMasterCompactFlag();
	numbercompactings++;
	secondscompacting += (getdoubletime() - timestart);
}

#ifdef DO_THREADS /*covers next few functions*/

inline bool Solver::getreadydata(list<iteration_data_t>::iterator & newdata)
{
	for(newdata = dataqueue.begin(); newdata!=dataqueue.end(); newdata++)
	{
		if(!dataguardpflopp0[newdata->cardsi[PREFLOP][P0]] && !dataguardpflopp1[newdata->cardsi[PREFLOP][P1]]
		&& !dataguardriver[newdata->cardsi[RIVER][P0]] && !dataguardriver[newdata->cardsi[RIVER][P1]]
#if PERFECT_RECALL
		&& !dataguardflopp0[newdata->cardsi[FLOP][P0]] && !dataguardflopp1[newdata->cardsi[FLOP][P1]]
		&& !dataguardturnp0[newdata->cardsi[TURN][P0]] && !dataguardturnp1[newdata->cardsi[TURN][P1]]
#endif
		  )
		{
			for(list<iteration_data_t>::iterator to_be_skipped=dataqueue.begin(); to_be_skipped != newdata; to_be_skipped++)
			{
				if(newdata->cardsi[PREFLOP][P0] == to_be_skipped->cardsi[PREFLOP][P0] 
				|| newdata->cardsi[PREFLOP][P1] == to_be_skipped->cardsi[PREFLOP][P1]
#if PERFECT_RECALL
				|| newdata->cardsi[FLOP][P0] == to_be_skipped->cardsi[FLOP][P0]
				|| newdata->cardsi[FLOP][P1] == to_be_skipped->cardsi[FLOP][P1]
				|| newdata->cardsi[TURN][P0] == to_be_skipped->cardsi[TURN][P0]
				|| newdata->cardsi[TURN][P1] == to_be_skipped->cardsi[TURN][P1]
				|| newdata->cardsi[RIVER][P0] == to_be_skipped->cardsi[RIVER][P0] //I don't care about the order here as far as memory contention goes
				|| newdata->cardsi[RIVER][P1] == to_be_skipped->cardsi[RIVER][P1] //I don't care about the order here as far as memory contention goes
#endif
				  )
				{
					goto cannot_skip; //continue from outer for-loop
				}
			}
			return true; //found a new data that does not conflict!
		}
cannot_skip:
		continue; //a statement for the label to work, attached to outer for-loop
	}
	return false; //we have dropped through the outer loop without finding any good data
}

bool Solver::isalldataclear()
{
	//if the preflop is clear then everything should be clear
	return (dataguardpflopp0.none() && dataguardpflopp1.none());
}

/***********
  For reference: here is my threading algorithm:

	void threadloop()
	{
		lock;

		while(iterations != 0)
		{
			if(needtocompact && all flags are clear)
			{
				//no point in: setting all flags, unlocking, signalling, locking, clearing flags -- but we could.
				compact;
				clear compact flag;
			}
			else if(!needtocompact && any flags are clear)
			{
				set data's flag;
				unlock;
				signal conditional variable;
				work on data;
				lock;
				clear data's flag;
			}
			else
				sleep on conditional variable;
		}

		unlock;
	}

***********/

inline void Solver::ThreadDebug(string debugstring)
{
	if(THREADLOOPTRACE)
	{
		const int myindex = (this - solvers);
		string star = "                 ";
		star[myindex] = '*';
		cout << "Thread " << myindex+1 << " <" << star.substr(0, NUM_THREADS) << "> : " << debugstring << endl;
	}
}

void Solver::threadloop()
{
	list<iteration_data_t>::iterator my_data_it; //iterator pointing to my data I want to use

	pthread_mutex_lock(&threaddatalock);

	ThreadDebug("starting while loop....");

	while(iterations!=0) //each loop does one iteration or compacts or sleeps
	{
		if(memory->GetMasterCompactFlag() && isalldataclear())
		{
			ThreadDebug("....COMPACTING MEMORY....");
			docompact();
			ThreadDebug("....DONE COMPACTING MEMORY....");
		}
		else if(!memory->GetMasterCompactFlag() && getreadydata(my_data_it))
		{

			memcpy(cardsi, my_data_it->cardsi, sizeof(cardsi)); //copy that data into our non-static variables
			twoprob0wins = my_data_it->twoprob0wins;

			dataguardpflopp0[cardsi[PREFLOP][P0]] = true;
			dataguardpflopp1[cardsi[PREFLOP][P1]] = true;
			dataguardriver[cardsi[RIVER][P0]] = true; //could be same bin as
			dataguardriver[cardsi[RIVER][P1]] = true; //this one
#if IMPERFECT_RECALL
			dataguardflopp0[cardsi[FLOP][P0]] = true;
			dataguardflopp1[cardsi[FLOP][P1]] = true;
			dataguardturnp0[cardsi[TURN][P0]] = true;
			dataguardturnp1[cardsi[TURN][P1]] = true;
#endif
			cardmachine->getnewgame(my_data_it->cardsi, my_data_it->twoprob0wins); //replace the data we used
			dataqueue.splice(dataqueue.end(), dataqueue, my_data_it); //move that node to the end of the list

			iterations--;

			ThreadDebug("....doing iteration cardsi "+tostring(cardsi[0][0])+"/"+tostring(cardsi[0][1]));
			if(WALKERDEBUG) cout << "ITERATION: " << total-iterations << endl << endl;
			pthread_mutex_unlock(&threaddatalock);
			pthread_cond_signal(&signaler); //I found data, maybe you can too!

			walker(PREFLOP,0,treeroot,1,1);

			pthread_mutex_lock(&threaddatalock);
			ThreadDebug("finished iteration cardsi "+tostring(cardsi[0][0])+"/"+tostring(cardsi[0][1])+"....");
			//set datainuse of whatever we were doing to false
			dataguardpflopp0[cardsi[PREFLOP][P0]] = false;
			dataguardpflopp1[cardsi[PREFLOP][P1]] = false;
			dataguardriver[cardsi[RIVER][P0]] = false; //could be same bin as 
			dataguardriver[cardsi[RIVER][P1]] = false; //this one
#if IMPERFECT_RECALL
			dataguardflopp0[cardsi[FLOP][P0]] = false;
			dataguardflopp1[cardsi[FLOP][P1]] = false;
			dataguardturnp0[cardsi[TURN][P0]] = false;
			dataguardturnp1[cardsi[TURN][P1]] = false;
#endif
		}
		else
		{
			if(memory->GetMasterCompactFlag()) 
				ThreadDebug("....sleeping (waiting for compact)");
			else if(!memory->GetMasterCompactFlag()) 
				ThreadDebug("....sleeping (waiting for iteration data)");
			pthread_cond_wait(&signaler, &threaddatalock); //sleep with lock, wake up with lock
			ThreadDebug("waking up! ....");
		}
	} //while loop

	pthread_mutex_unlock(&threaddatalock);
	pthread_cond_broadcast(&signaler); // wake up any sleeping threads

}

#else /*DO_THREADS*/

void Solver::threadloop()
{
	while(iterations!=0) //each loop does one iteration or compacts or sleeps
	{
		if(memory->GetMasterCompactFlag())
		{
			docompact();
		}
		else
		{
			cardmachine->getnewgame(cardsi, twoprob0wins);
			iterations--;
			if(WALKERDEBUG) cout << "ITERATION: " << total-iterations << endl << endl;
			walker(PREFLOP,0,treeroot,1,1);
		}
	}
}

#endif  /*DO_THREADS*/

inline tuple<Working_type, Working_type> utiltuple( int p0utility, bool awardthebonus )
{
	//NOTE:  we rely on awardthebonus to be cast to an int that is either 0 or 1.
	const Working_type aggression_multiplier = (Working_type)1 + awardthebonus * (Working_type)AGGRESSION_FACTOR/100.0;

	if(p0utility>0)
		return tuple<Working_type,Working_type>( aggression_multiplier * (Working_type)rake(p0utility), -(Working_type)p0utility);
	else
		return tuple<Working_type,Working_type>( (Working_type)p0utility, aggression_multiplier * (Working_type)rake(-p0utility));
}

struct NodeData
{
	int gr;
	int numa;
	int actioni;
	int cardsi;
	inline bool operator == (const NodeData & nd) const { return gr == nd.gr && numa == nd.numa && actioni == nd.actioni && cardsi == nd.cardsi; }
};
bool isdebugnode(int gr, int numa, int actioni, int cardsi)
{
	const NodeData gooddata[] = {
		{ 3, 2, 1925, 624 },
		{ 3, 2, 1924, 624 },
		{ 2, 2, 214, 124 },
		{ 2, 3, 320, 124 },
		{ 2, 3, 319, 124 },
		{ 2, 3, 318, 124 },
		{ 2, 2, 213, 124 },
		{ 2, 2, 212, 124 },
		{ 1, 2, 23, 24 },
		{ 1, 3, 35, 24 },
		{ 1, 3, 34, 24 },
		{ 1, 3, 33, 24 },
		{ 1, 2, 20, 24 },
		{ 0, 3, 4, 4 },
		{ 0, 3, 3, 4 },
		{ 0, 3, 0, 4 }};
	const NodeData mydata = { gr, numa, actioni, cardsi };
	for(unsigned i=0; i<sizeof(gooddata)/sizeof(NodeData); i++)
		if(gooddata[i] == mydata)
			return true;
	return false;
}


tuple<Working_type,Working_type> Solver::walker(const int gr, const int pot, const Vertex node, const Working_type prob0, const Working_type prob1)
{
	const int numa = out_degree(node, *tree);
	const int playeri = playerindex((*tree)[node].type);

	//read stratt from the data store. 

	Working_type stratt[MAX_ACTIONS_SOLVER];
	memory->readstratt( stratt, gr, numa, (*tree)[node].actioni, cardsi[gr][playeri] );

	//debug printing

	if(WALKERDEBUG && isdebugnode(gr, numa, (*tree)[node].actioni, cardsi[gr][playeri]) )
	{
		cout << "Starting Gameround: " << gr << /*" Pot: " << pot <<*/ " Actioni: " << (*tree)[node].actioni << " Cardsi: " << cardsi[gr][playeri]
			<< " Player(0/1): P" << playeri << endl;
		cout << "Prob0: " << prob0 << " Prob1: " << prob1 << " Stratt[]: { ";
		for(int i=0; i<numa; i++) cout << stratt[i] << " ";
		cout << "}" << endl << endl;
	}

	//recursively find utility of each action

	tuple<Working_type,Working_type> utility[MAX_ACTIONS_SOLVER];
	tuple<Working_type,Working_type> avgutility(0,0);

	EIter e, elast;
	int i=0;
	for(tie(e, elast) = out_edges(node, *tree); e!=elast; e++, i++)
	{
		if( stratt[i]==0.0 && (((*tree)[node].type==P0Plays && prob1==0.0) || ((*tree)[node].type==P1Plays && prob0==0.0)) )
			continue; //utility will be unused, children's regret will be unaffected

		switch((*tree)[*e].type)
		{
			//NOTE: awardthebonus is the 2nd parameter to utiltuple. utiltuple will always award the bonus to the winner only
			// and only if awardthebonus is true. We get to decide when to awardthebonus here. 
			//
			// On a call, we want to awardthebonus to the winner ONLY IF the winner did not make the call. We want to award the
			// behavior when we trick the opponent into calling with a worse hand. We do not ant to award the behavior of calling
			// itself, even with a better hand, as it MAY open up an exploitability where the bot calls too much. 
			//
			// In code, this is most easily and quicky computed via ( 2*playeri == twoprob0wins ) as shown:
			//
			// winner is 0 and 0 is caller, twoprob0wins=2, playeri=0, awardthebonus is false
			// winner is 1 and 0 is caller, twoprob0wins=0, playeri=0, awardthebonus is true
			// winner is 0 and 1 is caller, twoprob0wins=2, playeri=1, awardthebonus is true
			// winner is 1 and 1 is caller, twoprob0wins=0, playeri=1, awardthebonus is false
			// if it is a tie, twoprob0wins=1, awardthebonus is false, and it would have no effect in this case anyway
			//
		case CallAllin: //showdown
			utility[i] = utiltuple(get_property(*tree, settings_tag()).stacksize * (twoprob0wins-1), 2*playeri == twoprob0wins );
			break;

		case Call:
			if(gr==RIVER) //showdown
				utility[i] = utiltuple((pot+(*tree)[*e].potcontrib) * (twoprob0wins-1), 2*playeri == twoprob0wins );
			else //moving to next game round
			{
				if((*tree)[node].type==P0Plays)
					utility[i] = walker(gr+1, pot + (*tree)[*e].potcontrib, target(*e, *tree), prob0*stratt[i], prob1);
				else
					utility[i] = walker(gr+1, pot + (*tree)[*e].potcontrib, target(*e, *tree), prob0, prob1*stratt[i]);
			}
			break;

		case Fold:
			//NOTE: awardthebonus is the 2nd parameter to utiltuple. utiltuple will always award the bonus to the winner only
			// and only if awardthebonus is true. We get to decide when to awardthebonus here. 
			//
			// On a fold, we always want to award the bonus, as the folder is never the winner. And we want to award the
			// behavior where we get the opponent to fold.
			//
			utility[i] = utiltuple((pot+(*tree)[*e].potcontrib) * (2*playeri - 1), true ); //acting player is loser, utility is integer
			break;

		case Bet:
		case BetAllin: //NOT moving to new round -> same types as this instance
			if((*tree)[node].type==P0Plays)
				utility[i] = walker(gr, pot, target(*e, *tree), prob0*stratt[i], prob1);
			else
				utility[i] = walker(gr, pot, target(*e, *tree), prob0, prob1*stratt[i]);
		}

		avgutility.get<0>() += stratt[i]*utility[i].get<0>();
		avgutility.get<1>() += stratt[i]*utility[i].get<1>();
	}

	//debug printing

	if(WALKERDEBUG && isdebugnode(gr, numa, (*tree)[node].actioni, cardsi[gr][playeri]) )
	{
		cout << "Ending Gameround: " << gr << /*" Pot: " << pot <<*/ " Actioni: " << (*tree)[node].actioni << " Cardsi: " << cardsi[gr][playeri]
			<< " Player(0/1): P" << playeri << endl;
		cout << "Utility[].get<0>(): { ";
		for(int i=0; i<numa; i++) cout << utility[i].get<0>() << " ";
		cout << "} Utility[].get<1>(): { ";
		for(int i=0; i<numa; i++) cout << utility[i].get<1>() << " ";
		cout << "}" << endl << endl;
	}

	//update the average strategy and the regret totals from the results from walking the tree above.

	if ((*tree)[node].type==P0Plays) //P0 playing, use prob1, proability of player 1 getting here.
	{
		if(prob0 != 0)
			memory->writestratn( gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], prob0, stratt );

		if(prob1 != 0)
			memory->writeregret0( gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], prob1, avgutility.get<0>(), utility );
	}
	else // P1 playing, so his regret values are negative of P0's regret values.
	{
		if(prob1 != 0)
			memory->writestratn( gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], prob1, stratt );

		if(prob0 != 0)
			memory->writeregret1( gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], prob0, avgutility.get<1>(), utility );
	}


	//FINALLY, OUR LAST ACTION. WE WILL RETURN THE AVERAGE UTILITY, UNDER THE COMPUTED STRAT
	//SO THAT THE CALLER CAN KNOW THE EXPECTED VALUE OF THIS PATH. GOD SPEED CALLER.

	return avgutility;
}


