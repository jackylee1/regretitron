#include <stdio.h> //atol
#include <stdlib.h> //atol
#include <string.h> //strcmp
#include <string> //c++ strings
#include <iostream>
#include <poker_defs.h>
#include <inlines/eval.h>
#include "../PokerPlayerMFC/botapi.h"
#include "../PokerLibrary/constants.h"
#include "../MersenneTwister.h"
#include "../utility.h"
#include "analysis.h"
#include <math.h> //sqrt
#include <numeric> //accumulate
#include <algorithm> //sort
#include <iomanip>
#include <list>
using namespace std;

const double LOAD_LIMIT = 3.9; //load average number
const int LOAD_TO_USE = 1; //1, 2, or 3, for 1 min, 5 min, or 15 min.
const int SLEEP_ON_THREADSTART = 60; // in seconds, sleep this much after starting a thread before looking for more work to do
const int SLEEP_ON_NOWORK = 15; // in seconds, sleep this much after not finding any work to do.

enum Result
{
	P0FOLD,
	P1FOLD,
	CALLALLIN,
	GO
};


class Playoff
{
public:
	Playoff(string file1, string file2, double rakepct = 0, double rakecapbb = 0, double ssbb = 0);
	~Playoff();

	void playsomegames(int64 number_game_pairs);
	double getev() { return 1000.0 * (totalwinningsbot0/_bblind) / (2*totalpairsplayed); } //EV of file on LEFT
	void getfullresult( double & leftresult, double & rightresult, double & rakeresult )
	{ 
		leftresult = 1000.0 * (totalwinningsbot0/_bblind) / (2*totalpairsplayed);
		rightresult = 1000.0 * (totalwinningsbot1/_bblind) / (2*totalpairsplayed);
		rakeresult = 1000.0 * (totalwinningsrake/_bblind) / (2*totalpairsplayed);
	}
	double get95interval() { return 4500.0 / sqrt(totalpairsplayed); } //95% confidence interval
	int64 gettotalpairsplayed() { return totalpairsplayed; }

private:

	// i do not support copying this object

	Playoff(const Playoff& rhs);
	Playoff& operator=(const Playoff& rhs);

	// functions that actually play the game

	void playgamepair();
	Result playoutaround(Player nexttogo, double &pot);
	double playonegame(CardMask priv[2], CardMask board[4], int twoprob0wins, int & gr); //return utility to P0

	// private data members

	vector<BotAPI*> _bots; //_bots[0] always plays as P0, same for _bots[1].
	static const double _sblind;
	static const double _bblind;
	double _stacksize;
	double _rakepct;
	double _rakecapbb;
	void assignrake( const double prerakewinnings, const int maxround, double & postrakewinnings, double & raketaken );
	void assignwinnings( const double bot0prerakewinnings, const int maxround );
	double totalwinningsbot0; //this refers to the bot ORIGINALLY placed in slot 0. (swaps always done in pairs)
	double totalwinningsbot1;
	double totalwinningsrake;
	int64 totalpairsplayed;
#if PLAYOFFDEBUG > 0
	MTRand &mersenne;
#else
	MTRand mersenne; //used for card dealing, default initialized...that uses clock and time
#endif
};

const double Playoff::_sblind = 1.0;
const double Playoff::_bblind = 2.0;

Playoff::Playoff(string file1, string file2, double rakepct, double rakecapbb, double ssbb)
	: _bots(2, NULL),
	  _rakepct( rakepct ),
	  _rakecapbb( rakecapbb ),
	  totalwinningsbot0(0),
	  totalwinningsbot1(0),
	  totalwinningsrake(0),
	  totalpairsplayed(0),
#if PLAYOFFDEBUG > 0
	  mersenne(playoff_rand) //alias to globally defined object
#else
	  mersenne() //my own local object seeded with time and clock
#endif
{
	_bots[0] = new BotAPI(file1, true, MTRand::gettimeclockseed(), BotAPI::botapinulllogger, BotAPI::botapinulllogger );
	_bots[1] = new BotAPI(file2, true, MTRand::gettimeclockseed(), BotAPI::botapinulllogger, BotAPI::botapinulllogger );
	if(_bots[0]->islimit() != _bots[1]->islimit())
		REPORT("You can't play a limit bot against a no-limit bot!");
	if( ! fpequal( ssbb, 0 ) )
		_stacksize = ssbb * _bblind;
	else
	{
		double stacksizemult1 = _bots[0]->getstacksizemult();
		double stacksizemult2 = _bots[1]->getstacksizemult();
		if(!fpequal(stacksizemult1, stacksizemult2))
		{

			_stacksize = _bots[0]->islimit() ? min(stacksizemult1,stacksizemult2) : (stacksizemult1+stacksizemult2) / 2.0;
			cerr << "Warning: "+file1+" has stacksize "+tostring(stacksizemult1)+"bb while "+file2+" has stacksize "
				+tostring(stacksizemult2)+"bb for "+(_bots[0]->islimit() ? "limit" : "no-limit")
				+" poker... using "+tostring(_stacksize)+"bb." << endl;
			_stacksize *= _bblind;
		}
		else
			_stacksize = stacksizemult1 * _bblind;
	}
}

Playoff::~Playoff()
{
	delete _bots[0];
	delete _bots[1];
}

void Playoff::playsomegames(int64 number_game_pairs)
{
	totalpairsplayed += number_game_pairs;
	for(; number_game_pairs>0; number_game_pairs--)
		playgamepair();
}


void Playoff::playgamepair()
{

	//generate the random cards

	CardMask priv[2], board[4];
	int random_vector[52] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,
		30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
	for(int i=0; i<2; i++) CardMask_RESET(priv[i]);
	for(int i=0; i<4; i++) CardMask_RESET(board[i]);
	for(int i=0; i<9; i++) swap(random_vector[i], random_vector[i+mersenne.randInt(51-i)]);
	CardMask_SET(priv[P0],random_vector[0]);
	CardMask_SET(priv[P0],random_vector[1]);
	CardMask_SET(priv[P1],random_vector[2]);
	CardMask_SET(priv[P1],random_vector[3]);
	CardMask_SET(board[FLOP],random_vector[4]);
	CardMask_SET(board[FLOP],random_vector[5]);
	CardMask_SET(board[FLOP],random_vector[6]);
	CardMask_SET(board[TURN],random_vector[7]);
	CardMask_SET(board[RIVER],random_vector[8]);

	//figure the winner

	CardMask full0, full1;
	CardMask_OR(full0, board[FLOP], board[TURN])
	CardMask_OR(full0, full0, board[RIVER])
	full1 = full0;
	CardMask_OR(full0, full0, priv[P0]);
	CardMask_OR(full1, full1, priv[P1]);
	HandVal r0, r1;
	r0=Hand_EVAL_N(full0, 7);
	r1=Hand_EVAL_N(full1, 7);

	int twoprob0wins;
	if (r0>r1)
		twoprob0wins = 2;
	else if(r1>r0)
		twoprob0wins = 0;
	else
		twoprob0wins = 1;

	//play a game each way, make sure to never swap once, as then we'd lose track of which bot is which
	//_bots of zero is always player zero, _bots of one is always player one

	int gr1, gr2; //max gameround that the game attained
	double game1 = playonegame(priv, board, twoprob0wins, gr1); //returns utility of player/bot 0
	swap(_bots[0], _bots[1]);
	double game2 = -playonegame(priv, board, twoprob0wins, gr2); //returns utility of player/bot 0
	swap(_bots[0], _bots[1]);
#if PLAYOFFDEBUG > 0
	cout << game1 << "  " << (game2 != 0 ? game2 : 0) << endl; //avoid "-0" printing
#endif
	assignwinnings( game1, gr1 );
	assignwinnings( game2, gr2 );
}

void Playoff::assignrake( const double prerakewinnings, const int maxround, double & postrakewinnings, double & raketaken )
{
	if( maxround >= FLOP && prerakewinnings > 0 )
		raketaken = mymin( _rakecapbb * _bblind, _rakepct * ( 2 * prerakewinnings ) );
	else
		raketaken = 0;

	postrakewinnings = prerakewinnings - raketaken;
}

void Playoff::assignwinnings( const double bot0prerakewinnings, const int maxround )
{
	const double bot1prerakewinnings = - bot0prerakewinnings;

	double botpostrakewinnings[ 2 ], raketaken[ 2 ];
	assignrake( bot0prerakewinnings, maxround, botpostrakewinnings[ 0 ], raketaken[ 0 ] );
	assignrake( bot1prerakewinnings, maxround, botpostrakewinnings[ 1 ], raketaken[ 1 ] );

	totalwinningsbot0 += botpostrakewinnings[ 0 ];
	totalwinningsbot1 += botpostrakewinnings[ 1 ];
	totalwinningsrake += raketaken[ 0 ] + raketaken[ 1 ];
}

double Playoff::playonegame(CardMask priv[2], CardMask board[4], int twoprob0wins, int & gr) //return utility to player/bot 0
{
	gr = PREFLOP;

	_bots[P0]->setnewgame(P0, priv[P0], _sblind, _bblind, _stacksize);
	_bots[P1]->setnewgame(P1, priv[P1], _sblind, _bblind, _stacksize);
	
	double pot=0;
	switch(playoutaround(P1, pot))
	{
		case P0FOLD: return -pot; //return utility to player/bot 0
		case P1FOLD: return pot;
		case CALLALLIN: gr=4; return (twoprob0wins-1) * _stacksize;
		case GO: break;
	}

	for(gr=FLOP; gr<4; gr++)
	{
		_bots[P0]->setnextround(gr, board[gr], pot);
		_bots[P1]->setnextround(gr, board[gr], pot);
		switch(playoutaround(P0, pot))
		{
			case P0FOLD: return -pot;
			case P1FOLD: return pot;
			case CALLALLIN: gr=4; return (twoprob0wins-1) * _stacksize;
			case GO: break;
		}
	}
	return (twoprob0wins-1) * pot; //this is the showdown
}

//pot starts at the agreed upon pot size (per player) at the start of the round
//we update it here to the that value that it takes upon completion of this round
Result Playoff::playoutaround(Player nexttogo, double &pot)
{
	//usually false, but an exceptional true case, only in preflop. Don't have to handle case when stacksize is 
	// equal or less than small blind, as that does not result in a playable game that I can make a bot for,
	// and this program only plays at stacksizes that at least one of the bots was trained at.
	bool prev_act_was_allin = fpgreatereq(_bblind, _stacksize);
	while(1)
	{
		//get the bot action

		double amount;
		Action act = _bots[nexttogo]->getbotaction(amount);

		if(fpgreater(pot+amount, _stacksize))
			REPORT("my bot bet more than the set stacksize");

		//handle each type of bot action, telling the bots only if further action needed

		if(act == FOLD)
		{
			pot += amount;
			return nexttogo == P0 ? P0FOLD : P1FOLD;
		}
		else if(act == CALL)
		{
			if(prev_act_was_allin)
				return CALLALLIN;
			else
			{
				_bots[P0]->doaction(nexttogo, act, amount);
				_bots[P1]->doaction(nexttogo, act, amount);
				pot += amount;
				return GO;
			}
		}

		//The bot bet: either regular or all-in. Either way, continue the loop.

		_bots[P0]->doaction(nexttogo, act, amount);
		_bots[P1]->doaction(nexttogo, act, amount);
		nexttogo = (nexttogo == P0) ? P1 : P0;
		if(fpequal(pot+amount, _stacksize)) prev_act_was_allin = true;
	}
}

//this struct allows me to sort just an index array, by the values in this vector
//See the code below for usage or here for example: http://www.cplusplus.com/reference/algorithm/sort/
struct sortingvector
{
	sortingvector(int vector_size) : vect(vector_size) {}
	bool operator() (int i, int j) { return vect[i] > vect[j]; }
	vector<double> vect;
};

string bot(int i) 
{ 
	string ret = "A";
	ret[0]+=i;
	return ret; 
}

struct ThreadData
{
	ThreadData(string f1, string f2, int i_, int j_) : file1(f1), file2(f2), i(i_), j(j_) {}
	string file1, file2;
	int i,j;

	static int64 numgames;
	static double interval;
	static vector<vector<double> > results;
	static pthread_mutex_t lock;

	static void * threadfunction(void * param)
	{
		ThreadData* threader = (ThreadData*)param;
		threader->dowork();
		delete threader;
		return NULL;
	}

	void dowork()
	{
		Playoff playoff(file1, file2);
		playoff.playsomegames(numgames);
		pthread_mutex_lock(&lock);
		results[i][j] = playoff.getev(); //ev of file on LEFT, index on LEFT
		results[j][i] = -playoff.getev();
		interval = playoff.get95interval(); //same for all playoffs
		pthread_mutex_unlock(&lock);
	}
};

int64 ThreadData::numgames;
double ThreadData::interval;
vector<vector<double> > ThreadData::results;
pthread_mutex_t ThreadData::lock = PTHREAD_MUTEX_INITIALIZER;

double getloadavg()
{
	double loadavg[3];
	if(getloadavg(loadavg, 3) < LOAD_TO_USE)
		REPORT("load average getting has failed!");
	return loadavg[LOAD_TO_USE-1];
}

int main(int argc, char **argv)
{
	//turnloggingon(true, "cout");

	//analysis will return true if it parsed the commands into something to do
	if(doanalysis(argc, argv))
		return 0;

	//two files supplied, with rake and ss. otherwise same as next case

	else if( ( argc == 6 || argc == 7 ) && string( argv[ 1 ] ) == "rakess" )
	{
		Playoff * myplayoff = new Playoff( argv[ 4 ], argv[ 5 ], 0.05, atof( argv[ 2 ] ), atof( argv[ 3 ] ) );
		const int numgames = argc==7 ? atol(argv[6]) : 10000;
playgamesrake:
		myplayoff->playsomegames(numgames); 
		cout << "After " << myplayoff->gettotalpairsplayed() << " pairs of games, ";
		double leftresult, rightresult, rakeresult;
		myplayoff->getfullresult( leftresult, rightresult, rakeresult );
		double std = myplayoff->get95interval();
		cout 
			<< "left=" << setw( 10 ) << left << leftresult 
			<< " right=" << setw( 10 ) << left << rightresult 
			<< " rake=" << setw( 10 ) << left << rakeresult
			<< "  +- " << std
			<< endl;

		if(argc == 6)
		{
			cout << "\033[1A"; //move cursor up one line
			goto playgamesrake; //infinite loop
		}
	}

	//two files supplied. play them against each other, repeatedly, unless number is given.

	else if(argc==3 || argc==4)
	{
		Playoff * myplayoff = new Playoff(argv[1], argv[2]);
		const int numgames = argc==4 ? atol(argv[3]) : 10000;
playgames:
		myplayoff->playsomegames(numgames); 
		cout << "After " << myplayoff->gettotalpairsplayed() << " pairs of games, ";
		double ev = myplayoff->getev();
		double std = myplayoff->get95interval();
		if(ev > 0)
			cout << "file on left wins at: ";
		else if(ev < 0)
		{
			cout << "file on right wins at: ";
			ev = -ev;
		}
		else 
			cout << "they are tied! at: ";
		cout << ev << " +- " << std << "     " << endl;

		if(argc == 3)
		{
			cout << "\033[1A"; //move cursor up one line
			goto playgames; //infinite loop
		}
	}

	//three or more files supplied. play all of them against each other, then sort the results and
	//print a table

	else if (argc > 4)
	{
		//init local variables

		const int numfiles = argc - 2;
		vector<string> myfiles;
		myfiles.reserve(numfiles);
		for(int i=2; i<argc; i++) myfiles.push_back(argv[i]);
		vector<vector<bool> > haveplayed(numfiles, vector<bool>(numfiles, false));
		list<pthread_t *> mythreads;

		//init ThreadData

		ThreadData::numgames = atol(argv[1]);
		ThreadData::results.assign(numfiles, vector<double>(numfiles, 0));
		ThreadData::interval = 0;

		//look for available work, if so start a thread, rest, and see if done.
		//if not, rest longer.

lookforwork:

		if(getloadavg() < LOAD_LIMIT)
		{
			for(int i=0; i<numfiles; i++)
			{
				for(int j=i+1; j<numfiles; j++)
				{
					if(haveplayed[i][j] == false && file_exists(myfiles[i]) && file_exists(myfiles[j]))
					{
						haveplayed[i][j] = true;
						cerr << "playing " << myfiles[i] << " against " << myfiles[j] << "..." << endl;
						//create a new ThreadData here. it will be deleted by ThreadData::threadfunction
						//which is run by the thread. 
						ThreadData * newthread = new ThreadData(myfiles[i], myfiles[j], i, j);
						mythreads.push_back(new pthread_t);
						//new therad, using the pthread object in the list, using the newthread ThreadData object above
						if(pthread_create(mythreads.back(), NULL, ThreadData::threadfunction, newthread))
							REPORT("error creating thread");
						sleep(SLEEP_ON_THREADSTART); //takes that long for #2 load average to update
						goto amidone;
					}
				}
			}
		}
		sleep(SLEEP_ON_NOWORK); //we could not find work to do

		//check to see if we are done, if so, join threads, if not, look for work.

amidone:
		for(int i=0; i<numfiles; i++)
			for(int j=i+1; j<numfiles; j++) // same loop bounds as are set to true above
				if(haveplayed[i][j] == false)
					goto lookforwork;

		//all the work may not be done, but it has been started. wait for threads to finish

		cerr << endl;
		while(!mythreads.empty())
		{
			if(pthread_join(*mythreads.front(),NULL))
				REPORT("error joining thread");
			delete mythreads.front();
			mythreads.pop_front();
		}

		//get average results

		sortingvector avgresult(numfiles);
		vector<int> sorted(numfiles); //indexes into avgresult.vect
		for(int i=0; i<numfiles; i++)
		{
			avgresult.vect[i] = accumulate(ThreadData::results[i].begin(), ThreadData::results[i].end(), 0.0) / numfiles;
			sorted[i] = i;
		}

		//sort

		sort(sorted.begin(), sorted.end(), avgresult); //see comment above sortingvector

		//print info

		cout << "units are milli-big-blinds/hand, with respect to the ROW bot " << endl;
		cout << "and are good to plus/minus " << ThreadData::interval << " mb/hand (95% confidence)" << endl;
		cout << "(averages may be good to ~" << ThreadData::interval / sqrt(numfiles) << "mb/hand)" << endl;
		cout << endl;

		//print file labels

		for(int i=0; i<numfiles; i++)
			cout << bot(i) << ": " << myfiles[sorted[i]] << endl;
		cout << endl;

		//print column headings

		cout << setw(8) << ' ';
		for(int i=0; i<numfiles; i++)
			cout << setw(8) << left << bot(i);
		cout << "Average" << endl;

		//print rest of table body
		
		const int precision = (ThreadData::interval < 0.3 ? 3 : (ThreadData::interval < 3 ? 2 : 1));
		const int precisionavg = (ThreadData::interval/sqrt(numfiles) < 0.3 ? 3 : (ThreadData::interval/sqrt(numfiles) < 3 ? 2 : 1));

		for(int i=0; i<numfiles; i++)
		{
			cout << setw(8) << right << bot(i)+":  ";
			for(int j=0; j<numfiles; j++)
			{
				cout << fixed << setprecision(precision) << left << setw(8) << ThreadData::results[sorted[i]][sorted[j]];
			}
			cout << fixed << setprecision(precisionavg) << avgresult.vect[sorted[i]] << endl;
		}

	}
	else
	{
		cout << "Playoff Usage: " << endl;
		cout << "  " << argv[0] << " xmlfile1 xmlfile2" << endl;
		cout << "     prints constantly updating display of results to screen, sweet!" << endl;
		cout << "  " << argv[0] << " xmlfile1 xmlfile2 numgames" << endl;
		cout << "     playes numgames and prints the results, done (appropriate for saving output to a file)" << endl;
		cout << "  " << argv[0] << " numgames xmlfile1 ... xmlfileN" << endl;
		cout << "     builds a fuckin chart." << endl;
		printanalysisusage(argv[0]);
	}
}

