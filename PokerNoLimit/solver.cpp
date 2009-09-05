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
const BettingTree * Solver::tree = NULL;
MemoryManager * Solver::memory = NULL;
#ifdef DO_THREADS
pthread_mutex_t Solver::threaddatalock = PTHREAD_MUTEX_INITIALIZER;
#if USE_HISTORY
list<Solver::iteration_data_t> Solver::dataqueue(N_LOOKAHEAD);
bool Solver::datainuse[PFLOP_CARDSI_MAX*2];
pthread_cond_t Solver::signaler;
#else
#endif
#endif
vector< vector<int> > Solver::staticactioncounters(4, vector<int>(MAX_NODETYPES, 0));
vector< fpworking_type > Solver::accumulated_regret;


void Solver::initsolver()
{
	cardmachine = new CardMachine(CARDSETTINGS, true, SEED_RAND, SEED_WITH); //settings taken from solveparams.h
	tree = new BettingTree(TREESETTINGS); //the settings are taken from solveparams.h
	memory = new MemoryManager(*tree, *cardmachine);
	inittime = getdoubletime();

#ifdef DO_THREADS
#if USE_HISTORY 
	pthread_cond_init(&signaler, NULL);
	for(int i=0; i<PFLOP_CARDSI_MAX*2; i++)
		datainuse[i] = false;
	for(list<iteration_data_t>::iterator data = dataqueue.begin(); data!=dataqueue.end(); data++)
		cardmachine->getnewgame(data->cardsi, data->twoprob0wins); //get new data
#else 
	for(int gr=0; gr<4; gr++) //need locks for all rounds
	{
		cardsilocks[gr] = new pthread_mutex_t[2*cardmachine->getcardsimax(gr)];
		for(int i=0; i<2*cardmachine->getcardsimax(gr); i++)
			pthread_mutex_init(&cardsilocks[gr][i], NULL);
	}
#endif
#endif
}

void Solver::destructsolver()
{
	delete cardmachine;
	delete tree;
	delete memory;
#ifdef DO_THREADS
#if !USE_HISTORY
	for(int gr=0; gr<4; gr++)
		if(cardsilocks[gr]!=NULL)
			delete[] cardsilocks[gr];
#endif
#endif
}

//function called by main()
//returns seconds taken to do this many iter
double Solver::solve(int64 iter)  
{ 
	double starttime = getdoubletime();

	//logic to stop every DO_BOUNDS_EVERY iterations, and print bound info
	//otherwise, just do 'iter' iterations

	int64 goal = total + iter;
	int64 numleft = iter;
	while(total < goal)
	{
		// last time we did bounds: (total/DO_BOUNDS_EVERY) * DO_BOUNDS_EVERY
		// next time we do bounds: (total/DO_BOUNDS_EVERY + 1) * DO_BOUNDS_EVERY
		int64 tillbound = (total/DO_BOUNDS_EVERY + 1) * DO_BOUNDS_EVERY - total;
		if(numleft < tillbound)
			doiter(numleft);
		else
		{
			doiter(tillbound);
			numleft -= tillbound;
			cout << endl << " Calculating bounds on solution..." << endl;
			cout << getstatus() << endl;
		}
	}

	if(total!=goal) 
		REPORT("can't count.");

	return getdoubletime() - starttime;
}

#ifdef __GNUC__ //needed to print __float128 on linux
inline ostream& operator << (ostream& os, const __float128& n) { return os << (__float80)n; }
#endif

fpworking_type Solver::getpfloppushprob(int pushfold_index)
{
	if(!tree->getparams().pushfold)
		REPORT("getpfloppushprob only designed for pushfold");

	//pushfold_index is combine(index2_index, INDEX2_MAX, sblind_bblind)
	// where sblind_bblind is 0 for sblind and 1 for bblind
	BetNode mynode;
	const int beti = (pushfold_index / INDEX2_MAX == 0) ? 0 : 98;
	tree->getnode(PREFLOP, 0, beti, mynode);
	const int &numa = mynode.numacts;
	if(numa != 2)
		REPORT("getpfloppushprobs found a node with not 2 actions.");
	const int cardsi = pushfold_index % INDEX2_MAX;

	//get pointers to data

	fpstore_type * stratn, * regret;
#if STORE_DENOM
	fpstore_type * stratd;
#endif
	const int actioni = (pushfold_index / INDEX2_MAX == 0) ? 0 : 1;
	memory->dataindexing(PREFLOP, numa, actioni, cardsi, stratn, regret
#if STORE_DENOM
			, stratd
#endif
			);

	//compute the denominator

	fpworking_type denominator = 0.0;
	for(int i=0; i<numa; i++)
		denominator += stratn[i];

	//return probability of pushing

	if(!tree->isallin(mynode.result[1], mynode.potcontrib[1], PREFLOP)
		   	&& mynode.result[1] != BetNode::AI)
		REPORT("getpfloppushprob found not quite an all-in node");
	return stratn[1] / denominator;
}

struct sortingvector
{
	sortingvector(int vector_size) : vect(vector_size) {}
	bool operator() (int i, int j) { return vect[i] > vect[j]; }
	vector<fpworking_type> vect;
};

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

	//get bounder report

	TiXmlText * bounderreport = new TiXmlText("\n"+getstatus());
	bounderreport ->SetCDATA(true);
	TiXmlElement * boundernode = new TiXmlElement("bounderreport");
	boundernode->LinkEndChild(bounderreport);

	//link xml node..

	TiXmlElement * strat = new TiXmlElement("solver");
	strat->LinkEndChild(file);
	strat->LinkEndChild(run);
	strat->LinkEndChild(data);
	strat->LinkEndChild(boundernode);
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
	metanode->SetAttribute("stacksize", (int)tree->getparams().stacksize);
	metanode->SetAttribute("pushfold", tree->getparams().pushfold);
	metanode->SetAttribute("limit", tree->getparams().limit);
	mytree->LinkEndChild(metanode);

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

	// print the pushfold case analysis to an ostringstream string

	ostringstream text;
	text << endl << endl;

	if(tree->getparams().pushfold)
	{
		text << "Performing pushfold analysis:" << endl;

		//hard coded CORRECT data

		//verifydata_xx_yy_SB/BB
		//these are the CORRECT probabilities of PUSHING in pushfold scenario
		// with xx to yy stacksize to bblind ratio,
		// from either the SB or the BB,
		// in INDEX2 order:
		// 22, 32, 33, ... , K2 .. KQ KK, A2 .. AA, 32s, 42s, 43s, 52s .. 54s, A2s .. AKs
		const fpworking_type verifydata_40_6_SB[INDEX2_MAX] = 
		{
			//small blind values: 
			1.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0,
			0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
			1.0, 0.0, 0.0, 0.0, 0.0, 0.5688307975983433, 1.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0,
			0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0,
			1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0,
			1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
		};

		/*
			//big blind values
			1.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0625, 1.0,
			0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
			1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0,
			0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0,
			1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
			1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
		*/

		// keys for sorting
		vector<int> key(INDEX2_MAX, -1);
		for(int i=0; i<INDEX2_MAX; i++)
			key[i]=i;

		// scores, compared above vs actual
		sortingvector score(INDEX2_MAX);
		const fpworking_type * validdata = NULL;
		if(abs((double)tree->getparams().stacksize / tree->getparams().bblind - 40.0/6.0) < 1e-5)
			validdata = verifydata_40_6_SB;
		else
			REPORT("No correct pushfold data found for stacksize "
					+tostring(tree->getparams().stacksize)+" and bblind "
					+tostring(tree->getparams().bblind)+".");

		for(int i=0; i<INDEX2_MAX; ++i)
			score.vect[i] = abs(getpfloppushprob(i) - validdata[i]);

		// find a convergence score
		text << "Total sum of error (over " << INDEX2_MAX << " calculated numbers): "
			 << accumulate(score.vect.begin(), score.vect.end(), 0.0) << endl;

		// sort the keys by the scores (same method as in Playoff)
		sort(key.begin(), key.end(), score);

		const int n_bullshit = 30;
		text << "Top " << n_bullshit << " most erroneous cases:" << endl << endl;
		for(int i=0; i<n_bullshit; i++)
		{
			string handstring;
			if(cardmachine->preflophandinfo(key[i]%INDEX2_MAX, handstring) != key[i]%INDEX2_MAX)
				REPORT("consistency check on cardsi and index2_indexes failed");
			text << "  " << handstring << ' ' << (key[i]>=INDEX2_MAX ? "BB" : "SB") << ':' << endl;
			text << "  correct: ";
			if(validdata[key[i]] == 0.0)
				text << "fold.";
			else if(validdata[key[i]] == 1.0)
				text << "jam.";
			else 
				text << 1.0-validdata[key[i]] << " fold, " << validdata[key[i]] << " jam.";
			text << endl << "  calculated: " << 1.0-getpfloppushprob(key[i]) << " fold, " 
				<< getpfloppushprob(key[i]) << " jam." << endl;
			text << "  error: " << score.vect[key[i]] << endl << endl;
		}
	}

	// print the first node strategy to an ostringstream

	for(int actioni=0; actioni<=1; actioni++)
	{
		BetNode mynode;
		if(actioni==0)
		{
			tree->getnode(PREFLOP, 0, 0, mynode);
			text << endl << "Small Blind actions:" << endl << endl;
		}
		else if(actioni==1)
		{
			if(tree->getparams().pushfold)
			{
				tree->getnode(PREFLOP, 0, 98, mynode); //big blind after sb pushes
				text << endl << "Big Blind actions:" << endl << endl;
			}
			else
				break;
		}

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
			memory->dataindexing(PREFLOP, numa, actioni, cardsi, stratn, regret
#if STORE_DENOM
					, stratd
#endif
					);

			//print out a heading including example hand

			if(tree->getparams().pushfold)
				text << setw(5) << left << handstring + ":";
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
				if(tree->getparams().pushfold)
				{
					if(myprob > 0.98) //surety
					{
						if(a==numa-1)
							text << "jam" << endl;
						else
							text << "fold" << endl;
					}
					else if(myprob > 0.02) //unsurety
					{
						if (a==0) 
							text << endl << setw(14) << tree->actionstring(PREFLOP,a,mynode,1.0)+": " << 100.0*myprob << "%" << endl;
						else
							text << setw(14) << tree->actionstring(PREFLOP,a,mynode,1.0)+": " << 100.0*myprob << "%" << endl;
					}
				}
				else
					text << setw(14) << tree->actionstring(PREFLOP,a,mynode,1.0)+": " << 100.0*myprob << "%" << endl;
			}

			//print out denominator info if exists

#if STORE_DENOM
			text << "Sum of strats:  " << denominator / *stratd << endl;
			text << "stratd:         " << *stratd << endl;
			text << "sum(stratn[a]): " << denominator << endl;
			text << "difference:     " << *stratd - denominator << endl;
#endif
		}
		text << endl;
	}
	text << endl;

	// add that string as the first node strategy

	TiXmlText * text2 = new TiXmlText(text.str());
	text2->SetCDATA(true);
	TiXmlElement * firstnode = new TiXmlElement("smallblindpreflopstrategy");
	firstnode->LinkEndChild(text2);
	root->LinkEndChild(firstnode);

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
#ifdef DO_THREADS
	pthread_mutex_lock(&threaddatalock);
#endif

	fpu_fix_start(NULL);

	while(1)
	{
#ifdef DO_THREADS
		list<iteration_data_t>::iterator my_data_it; //iterator pointing to my data I want to use
		while(1) //loop till we find something
		{
			if(iterations==0) //check if we're all done.
			{
				pthread_cond_broadcast(&signaler); // wake up any sleeping threads
				pthread_mutex_unlock(&threaddatalock);
				return;
			}

			for(my_data_it = dataqueue.begin(); my_data_it!=dataqueue.end(); my_data_it++)
			{
				if(!datainuse[2*my_data_it->cardsi[PREFLOP][P0]] && !datainuse[2*my_data_it->cardsi[PREFLOP][P1]+1])
				{
					for(list<iteration_data_t>::iterator to_be_skipped=dataqueue.begin(); to_be_skipped != my_data_it; to_be_skipped++)
					{
						if(my_data_it->cardsi[PREFLOP][P0] == to_be_skipped->cardsi[PREFLOP][P0] ||
								my_data_it->cardsi[PREFLOP][P1] == to_be_skipped->cardsi[PREFLOP][P1])
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
		datainuse[2*cardsi[PREFLOP][P0]] = true;
		datainuse[2*cardsi[PREFLOP][P1]+1] = true;
		cardmachine->getnewgame(my_data_it->cardsi, my_data_it->twoprob0wins); //get new data
		dataqueue.splice(dataqueue.end(), dataqueue, my_data_it); //move that node to the end of the list
		iterations--;
		if(THREADLOOPTRACE)
			cerr << /*this <<*/ ": Solving " << cardsi[PREFLOP][P0] << " - " << cardsi[PREFLOP][P1] << endl;
		pthread_mutex_unlock(&threaddatalock);
#else
		if(iterations-- == 0)
			break;
		cardmachine->getnewgame(cardsi, twoprob0wins);
		if(THREADLOOPTRACE)
			cerr << /*this <<*/ ": Solving " << cardsi[PREFLOP][P0] << " - " << cardsi[PREFLOP][P1] << endl;
#endif
		for(int i=0; i<4; i++) 
			for(int j=0; j<MAX_NODETYPES; j++)
				actioncounters[i][j] = 0;
		walker(PREFLOP,0,0,1,1);
#ifdef DO_THREADS
		pthread_mutex_lock(&threaddatalock);
		if(THREADLOOPTRACE && false)
			cerr << this << ": Done with " << cardsi[PREFLOP][P0] << " - " << cardsi[PREFLOP][P1] << endl;
		//set datainuse of whatever we were doing to false
		if(!datainuse[2*cardsi[PREFLOP][P0]] || !datainuse[2*cardsi[PREFLOP][P1]+1])
			REPORT("we didn't have the lock... sadness...");
		datainuse[2*cardsi[PREFLOP][P0]] = false;
		datainuse[2*cardsi[PREFLOP][P1]+1] = false;
		pthread_cond_signal(&signaler); //new data is availble to be touched, so signal
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

	if(numa-2 < 0) REPORT("gcc was right");
#if STORE_DENOM
	fpstore_type * stratn, * regret, * stratd;
	memory->dataindexing(gr, numa, actioncounters[gr][numa-2]++, cardsi[gr][(int)mynode.playertoact], stratn, regret, stratd);
#else
	fpstore_type * stratn, * regret;
	memory->dataindexing(gr, numa, actioncounters[gr][numa-2]++, cardsi[gr][(int)mynode.playertoact], stratn, regret);
#endif
	
	//TODO: try out some gcc builtins here for prefetching

	// OK, NOW WE WILL APPLY EQUATION (8) FROM TECH REPORT, COMPUTE STRATEGY T+1

	//find total regret

	fpworking_type totalregret=0.0;

	//FPERROR: SUM LIST OF NUMBERS 
	for(int i=0; i<numa; i++)
		if (regret[i]>0.0) totalregret += regret[i];

	//set strategy proportional to positive regret, or 1/numa if no positive regret

	fpworking_type stratt[MAX_ACTIONS];

	//FPERROR: COMPARE TO ZERO
	//FPERROR: STRAIGHT FORWARD DIVISION
	if (totalregret > 0.0)
		for(int i=0; i<numa; i++)
			(regret[i]>0.0) ? stratt[i] = regret[i] / totalregret : stratt[i] = 0.0;
	else
		for(int i=0; i<numa; i++)
			stratt[i] = (fpworking_type)1/(fpworking_type)numa;


	//NOW, WE WANT TO FIND THE UTILITY OF EACH ACTION. 
	//WE OFTEN DO THIS BY CALLING WALKER RECURSIVELY.

	fpworking_type utility[MAX_ACTIONS];
	fpworking_type avgutility=0;

	for(int i=0; i<numa; i++)
	{
		//utility will be unused, children's regret will be unaffected
		//performance hack
		if(stratt[i]==0.0 && ((mynode.playertoact==0.0 && prob1==0.0) || (mynode.playertoact==1 && prob0==0.0)) )
		{
			//same code as in dummywalker
			switch(mynode.result[i])
			{
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
		case BetNode::AI: //called allin
			utility[i] = tree->getparams().stacksize * (twoprob0wins-1); //utility is integer value
			break;

		case BetNode::GO: //next game round
			if(gr==RIVER)// showdown
				utility[i] = (pot+mynode.potcontrib[i]) * (twoprob0wins-1); //utility is integer value
			else 
			{
				if(mynode.playertoact==0)
					utility[i] = walker(gr+1, pot+mynode.potcontrib[i], 0, prob0*stratt[i], prob1);
				else
					utility[i] = walker(gr+1, pot+mynode.potcontrib[i], 0, prob0, prob1*stratt[i]);
			}
			break;

		case BetNode::FD: //fold
			utility[i] = (pot+mynode.potcontrib[i]) * (2*mynode.playertoact - 1); //acting player is loser, utility is integer
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
			regret[a] += prob1 * (utility[a] - avgutility);
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

	if(numa-2 < 0) REPORT("gcc was right");
	actioncounters[gr][numa-2]++;

	for(int a=0; a<numa; a++)
	{
		switch(mynode.result[a])
		{
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

string Solver::getstatus()
{
	ostringstream out;

	//get info from bounder

	staticactioncounters.assign(4, vector<int>(MAX_NODETYPES, 0)); //zero our 2D array
	accumulated_regret.assign(2, 0); //one per player
	bounder(0,0,0); //boom

	//error checking & info node calc

	vector<vector<int> > myactmax;
	GetTreeSize(*tree, myactmax);
	int64 infonodes = 0;
	for(int i=0; i<4; i++)
	{
		for(int j=0; j<MAX_NODETYPES; j++)
		{
			if(myactmax[i][j] != staticactioncounters[i][j]) //error check
				REPORT("bounder has failed");
			infonodes += myactmax[i][j]*cardmachine->getcardsimax(i);
		}
	}

	//build string and quit

	out << " accumulated regret of player P0: " << accumulated_regret[P0] << " P1: " << accumulated_regret[P1] << endl;
	out << " iterations done: " << total << endl;
	out << " number of info nodes: " << infonodes << endl;
	out << " iterations per info node: " << fixed << setprecision(1) << (double)total/infonodes << endl;
	out << " epsilon: " << fixed << setprecision(1)
		<< 2 * (((double)max(accumulated_regret[P0], accumulated_regret[P1]) / total) / tree->getparams().bblind) * 1000 
		<< " milli-big-blinds" << endl;

	return out.str();
}

void Solver::bounder(int gr, int pot, int beti)
{
	BetNode mynode;
	tree->getnode(gr, pot, beti, mynode);
	const int &numa = mynode.numacts; //for ease of typing

	for(int cardsi = 0; cardsi < cardmachine->getcardsimax(gr); cardsi++)
	{
#if STORE_DENOM
		fpstore_type * stratn, * regret, * stratd;
		memory->dataindexing(gr, numa, staticactioncounters[gr][numa-2], cardsi, stratn, regret, stratd);
#else
		fpstore_type * stratn, * regret;
		memory->dataindexing(gr, numa, staticactioncounters[gr][numa-2], cardsi, stratn, regret);
#endif

		fpstore_type regret_max = 0; // we want max regret or 0, whichever is biggest
		for(int i=0; i<numa; i++)
			if(regret[i] > regret_max)
				regret_max = regret[i];

		fpstore_type bound = 2 * tree->getparams().stacksize * sqrt( total * numa );
		if(regret_max > bound)
			REPORT("NOTE: regret "+tostring(regret_max)+" exceeds bounds "+tostring(bound)+'.');

		accumulated_regret[(int)mynode.playertoact] += (fpworking_type)regret_max;
	}

	staticactioncounters[gr][numa-2]++;

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
				bounder(gr+1, pot+mynode.potcontrib[a], 0);
			continue;

		default://child node
			bounder(gr, pot, mynode.result[a]);
			continue;
		}
	}
}



