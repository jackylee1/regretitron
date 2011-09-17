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
CardMachine * Solver::cardmachine = NULL;
BettingTree * Solver::tree = NULL;
Vertex Solver::treeroot;
MemoryManager * Solver::memory = NULL;
boost::scoped_array< Solver > Solver::solvers;
unsigned Solver::num_threads = 0;
unsigned Solver::n_lookahead = 0;
Working_type Solver::aggression_static_mult = 0;
Working_type Solver::aggression_selective_mult = 0;
string Solver::m_rakename;
int Solver::m_rakecap = 0;
#ifdef DO_THREADS
pthread_mutex_t Solver::threaddatalock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t Solver::signaler = PTHREAD_COND_INITIALIZER;
list<Solver::iteration_data_t> Solver::dataqueue;
vector< bool > Solver::dataguardpflopp0;
vector< bool > Solver::dataguardpflopp1;
vector< bool > Solver::dataguardriver; //needed for memory contention in HugeBuffer, works for imperfect recall too
#if IMPERFECT_RECALL /*then we need to account for all data in use, not just first round*/
vector< bool > Solver::dataguardflopp0;
vector< bool > Solver::dataguardflopp1;
vector< bool > Solver::dataguardturnp0;
vector< bool > Solver::dataguardturnp1;
#endif
#endif


void Solver::initsolver( const treesettings_t & treesettings, const cardsettings_t & cardsettings, MTRand::uint32 randseed, unsigned nthreads, unsigned nlook, double aggstatic, double aggselective, string rakename, int rakecap )
{
	//threads

	num_threads = nthreads;
	n_lookahead = nlook;

	if( num_threads == 0 || num_threads > 100 )
		REPORT( "trying to use " + tostr( num_threads ) + " threads...", KNOWN );
	if( n_lookahead == 0 || n_lookahead > 3000 )
		REPORT( "trying to use " + tostr( n_lookahead ) + " for n_lookahead...", KNOWN );

	boost::scoped_array< Solver > solver_temp( new Solver[ num_threads ] );
	solvers.swap( solver_temp );

	//aggression

	aggression_static_mult = static_cast< Working_type >( 1.0 + aggstatic / 100.0 );
	aggression_selective_mult = static_cast< Working_type >( 1.0 + ( aggstatic + aggselective ) / 100.0 );

	//rake

	m_rakename = rakename;
	m_rakecap = rakecap;

	//other init

	tree = new BettingTree(treesettings); //the settings are taken from solveparams.h
	ConsoleLogger treelogger;
	treeroot = createtree(*tree, MAX_ACTIONS_SOLVER, treelogger);
	cardmachine = new CardMachine(cardsettings, true, randseed); //settings taken from solveparams.h
	memory = new MemoryManager(*tree, *cardmachine);
	inittime = getdoubletime();

#ifdef DO_THREADS
	//if these were bitsets, the default constructor set these to all false
	//but now they are vector<bool>'s, so that I can use more runtime settings,
	//and initialize their size here and set them all to false
	dataguardpflopp0.resize( cardmachine->getcardsimax( PREFLOP ), false ); 
	dataguardpflopp1.resize( cardmachine->getcardsimax( PREFLOP ), false ); 
	dataguardriver.resize( cardmachine->getcardsimax( RIVER ), false );
#if IMPERFECT_RECALL /*then we need to account for all data in use, not just first round*/
	dataguardflopp0.resize( cardmachine->getcardsimax( FLOP ), false ); 
	dataguardflopp1.resize( cardmachine->getcardsimax( FLOP ), false ); 
	dataguardturnp0.resize( cardmachine->getcardsimax( TURN ), false ); 
	dataguardturnp1.resize( cardmachine->getcardsimax( TURN ), false ); 
#endif

	for( unsigned i = 0; i < n_lookahead; i++ )
	{
		dataqueue.push_back( iteration_data_t( ) );
		iteration_data_t & data = dataqueue.back( );
		cardmachine->getnewgame(data.cardsi, data.twoprob0wins); //get new data
	}
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

	//date and time solved

	time_t rawtime;
	struct tm * timeinfo;
	time( & rawtime );
	timeinfo = localtime( & rawtime );
	char * currtime = asctime( timeinfo );
	currtime[ strlen( currtime ) - 1 ] = 0; //remove the \n
	run->SetAttribute("currtime", currtime);

	//rand seed and computer used

	run->SetAttribute("randseededwith", tostr( cardmachine->getrandseed( ) ) );

	run->SetAttribute("numthreads", num_threads);
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
	params->SetAttribute("aggstatic", 
			tostr2( ( aggression_static_mult - 1 ) * 100.0 ) );
	params->SetAttribute("aggselective", 
			tostr2( ( aggression_selective_mult - aggression_static_mult ) * 100.0 ) );
	params->SetAttribute("raketype", m_rakename);
	params->SetAttribute("rakecap", m_rakecap);

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
	meta->SetAttribute("useboardbins", cardmachine->getparams().useboardbins);
	cardbins->LinkEndChild(meta);
	for(int gr=0; gr<4; gr++) //regular bins
	{
		ostringstream o;
		o << "round" << gr;
		TiXmlElement * round = new TiXmlElement(o.str());
		round->SetAttribute("nbins", cardmachine->getparams().bin_max[gr]);
		round->SetAttribute("filesize", tostr( cardmachine->getparams().filesize[gr] ) );
		TiXmlText * roundtext = new TiXmlText(cardmachine->getparams().filename[gr]);
		round->LinkEndChild(roundtext);
		cardbins->LinkEndChild(round);
	}
	for(int gr=0; gr<4; gr++) //board bins
	{
		ostringstream o;
		o << "boardround" << gr;
		TiXmlElement * round = new TiXmlElement(o.str());
		round->SetAttribute("nbins", cardmachine->getparams().board_bin_max[gr]);
		round->SetAttribute("filesize", cardmachine->getparams().boardbinsfilesize[gr]);
		TiXmlText * roundtext = new TiXmlText(cardmachine->getparams().boardbinsfilename[gr]);
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

//function called by main()
double //time taken
Solver::solve(int64 iter) 
{ 
	double starttime = getdoubletime();
	if( iter <= n_lookahead )
	{
		total += iter;
		iterations = iter;
		solvers[0].threadloop();
	}
	else
	{
		total += iter - n_lookahead; 
		iterations = iter - n_lookahead;

#ifdef DO_THREADS
		boost::scoped_array< pthread_t > threads( new pthread_t[ num_threads - 1 ] );
		for(unsigned i=0; i<num_threads-1; i++)
			if(pthread_create(&threads[i], NULL, callthreadloop, &solvers[i+1]))
				REPORT("error creating thread");
#endif
		//use current thread too
		solvers[0].threadloop();
#ifdef DO_THREADS
		//wait for them all to return
		for(unsigned i=0; i<num_threads-1; i++)
			if(pthread_join(threads[i],NULL))
				REPORT("error joining thread");
#endif

		//do the last few unthreaded for consistency (to clear the queue)
		total += n_lookahead; 
		iterations = n_lookahead;
		solvers[0].threadloop();
	}

	//return time taken
	return getdoubletime() - starttime;
}


//must be static because pthreads do not understand classes.
//the void* parameter is a pointer to solver instance (i.e. the 'this' pointer).
void* Solver::callthreadloop(void* mysolver)
{
	((Solver *) mysolver)->threadloop();
	return NULL;
}


#ifdef DO_THREADS /*covers next few functions*/

inline bool Solver::getreadydata(list<iteration_data_t>::iterator & newdata)
{
	for(newdata = dataqueue.begin(); newdata!=dataqueue.end(); newdata++)
	{
		if(!dataguardpflopp0[newdata->cardsi[PREFLOP][P0]] && !dataguardpflopp1[newdata->cardsi[PREFLOP][P1]]
		&& !dataguardriver[newdata->cardsi[RIVER][P0]] && !dataguardriver[newdata->cardsi[RIVER][P1]]
#if IMPERFECT_RECALL
		&& !dataguardflopp0[newdata->cardsi[FLOP][P0]] && !dataguardflopp1[newdata->cardsi[FLOP][P1]]
		&& !dataguardturnp0[newdata->cardsi[TURN][P0]] && !dataguardturnp1[newdata->cardsi[TURN][P1]]
#endif
		  )
		{
			for(list<iteration_data_t>::iterator to_be_skipped=dataqueue.begin(); to_be_skipped != newdata; to_be_skipped++)
			{
				if(newdata->cardsi[PREFLOP][P0] == to_be_skipped->cardsi[PREFLOP][P0] 
				|| newdata->cardsi[PREFLOP][P1] == to_be_skipped->cardsi[PREFLOP][P1]
#if IMPERFECT_RECALL
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
	vector< bool >::iterator i;
	for( i = dataguardpflopp0.begin( ); i != dataguardpflopp0.end( ); i++ )
		if( * i )
			return false;
	for( i = dataguardpflopp1.begin( ); i != dataguardpflopp1.end( ); i++ )
		if( * i )
			return false;
	return true;

	//this is my old bitset logic: (does the same thing)
	//return (dataguardpflopp0.none() && dataguardpflopp1.none());
}

/***********
  For reference: here is my threading algorithm:

	void threadloop()
	{
		lock;

		while(iterations != 0)
		{
			if(any flags are clear)
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
		const int myindex = ( this - & solvers[ 0 ] );
		string star = "                 ";
		star[myindex] = '*';
		cout << "Thread " << myindex+1 << " <" << star.substr(0, num_threads) << "> : " << debugstring << endl;
	}
}

void Solver::threadloop()
{
	list<iteration_data_t>::iterator my_data_it; //iterator pointing to my data I want to use

	pthread_mutex_lock(&threaddatalock);

	ThreadDebug("starting while loop....");

	while(iterations!=0) //each loop does one iteration or sleeps
	{
		if(getreadydata(my_data_it))
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
	while(iterations!=0) //each loop does one iteration
	{
		cardmachine->getnewgame(cardsi, twoprob0wins);
		iterations--;
		if(WALKERDEBUG) cout << "ITERATION: " << total-iterations << endl << endl;
		walker(PREFLOP,0,treeroot,1,1);
	}
}

#endif  /*DO_THREADS*/

inline Working_type calculaterake( int winningutility, bool haveseenflop, int rakecap )
{
#if 0
	// this method is accurate for all rakecap amounts
	const Working_type raketaken = mymin< Working_type >( 
			static_cast< Working_type >( haveseenflop * rakecap ),  
			static_cast< Working_type >( 0.05 ) * ( 2 * winningutility ) );
	return static_cast< Working_type >( winningutility ) - raketaken;
#endif
#if 1
	// this method is only accurate for small rakecap's (i.e. high limit poker)
	const Working_type raketaken = static_cast< Working_type >( haveseenflop * rakecap );
	return static_cast< Working_type >( winningutility ) - raketaken;
#endif
}


inline tuple<Working_type, Working_type> Solver::utiltuple( int p0utility, bool awardthebonus, bool haveseenflop )
{
#ifdef DO_AGGRESSION

	const Working_type aggression_multiplier = awardthebonus ? aggression_selective_mult : aggression_static_mult;

	if(p0utility>0)
		return tuple<Working_type,Working_type>( 
			aggression_multiplier * calculaterake( p0utility, haveseenflop, m_rakecap ),
			static_cast< Working_type >( - p0utility ) );
	else
		return tuple< Working_type, Working_type >(
			static_cast< Working_type >( p0utility ),
			aggression_multiplier * calculaterake( - p0utility, haveseenflop, m_rakecap ) );

#else 
	//optimize for the case where aggression_multiplier is 1, 
	// hope awardthebonus is optimized away

	if(p0utility>0)
		return tuple< Working_type, Working_type >( 
			calculaterake( p0utility, haveseenflop, m_rakecap ),
			static_cast< Working_type >( - p0utility ) );
	else
		return tuple< Working_type, Working_type >( 
			static_cast< Working_type >( p0utility ),
			calculaterake( - p0utility, haveseenflop, m_rakecap ) );
#endif
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
			utility[i] = utiltuple(get_property(*tree, settings_tag()).stacksize * (twoprob0wins-1), 2*playeri == twoprob0wins, true );
			break;

		case Call:
			if(gr==RIVER) //showdown
				utility[i] = utiltuple((pot+(*tree)[*e].potcontrib) * (twoprob0wins-1), 2*playeri == twoprob0wins, true );
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
			utility[i] = utiltuple((pot+(*tree)[*e].potcontrib) * (2*playeri - 1), true, gr != PREFLOP ); //acting player is loser, utility is integer
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
			memory->writeregret< 0 >( gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], prob1, avgutility.get<0>(), utility );
	}
	else // P1 playing, so his regret values are negative of P0's regret values.
	{
		if(prob1 != 0)
			memory->writestratn( gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], prob1, stratt );

		if(prob0 != 0)
			memory->writeregret< 1 >( gr, numa, (*tree)[node].actioni, cardsi[gr][playeri], prob0, avgutility.get<1>(), utility );
	}


	//FINALLY, OUR LAST ACTION. WE WILL RETURN THE AVERAGE UTILITY, UNDER THE COMPUTED STRAT
	//SO THAT THE CALLER CAN KNOW THE EXPECTED VALUE OF THIS PATH. GOD SPEED CALLER.

	return avgutility;
}


