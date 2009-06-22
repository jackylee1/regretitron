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
#include <math.h> //sqrt
#include <numeric> //accumulate
#include <algorithm> //sort
#include <iomanip>
using namespace std;

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
	Playoff(string file1, string file2);
	~Playoff();

	void playsomegames(int64 number_game_pairs);
	double getev() { return 1000.0 * (totalwinningsbot0/_bblind) / (2*totalpairsplayed); } //EV of file on LEFT
	double get95interval() { return 4500.0 / sqrt(totalpairsplayed); } //95% confidence interval
	int64 gettotalpairsplayed() { return totalpairsplayed; }

private:

	// i do not support copying this object

	Playoff(const Playoff& rhs);
	Playoff& operator=(const Playoff& rhs);

	// functions that actually play the game

	void playgamepair();
	Result playoutaround(Player nexttogo, double &pot);
	double playonegame(CardMask priv[2], CardMask board[4], int twoprob0wins); //return utility to P0

	// private data members

	vector<BotAPI*> _bots; //_bots[0] always plays as P0, same for _bots[1].
	static const double _sblind = 1.0;
	static const double _bblind = 2.0;
	double _stacksize;
	double totalwinningsbot0; //this refers to the bot ORIGINALLY placed in slot 0. (swaps always done in pairs)
	int64 totalpairsplayed;
	MTRand mersenne; //used for card dealing
};

Playoff::Playoff(string file1, string file2)
	: _bots(2, NULL),
	  totalwinningsbot0(0),
	  totalpairsplayed(0)
{
	_bots[0] = new BotAPI(file1);
	_bots[1] = new BotAPI(file2);
	if(_bots[0]->islimit() != _bots[1]->islimit())
		REPORT("You can't play a limit bot against a no-limit bot!");
	double stacksize1 = _bots[0]->getstacksizemult();
	double stacksize2 = _bots[1]->getstacksizemult();
	if(stacksize1 != stacksize2)
	{
		_stacksize = (stacksize1 + stacksize2) / 2;
		string gametype = _bots[0]->islimit() ? "limit" : "no-limit";
		REPORT(file1+" has stacksize "+tostring(stacksize1)+" while "+file2+" has stacksize "
				+tostring(stacksize2)+" for "+gametype+" poker... using "+tostring(_stacksize)+".", WARN);
		_stacksize *= _bblind;
	}
	else
		_stacksize = stacksize1 * _bblind;
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

	CardMask priv[2]; //these are for PLAYER 0 and 1
	CardMask board[4];
	CardMask_RESET(board[PREFLOP]); //never used

	CardMask usedcards;

	CardMask_RESET(usedcards);
	MONTECARLO_N_CARDS_D(priv[P0], usedcards, 2, 1, );
	CardMask_OR(usedcards, usedcards, priv[P0]);

	MONTECARLO_N_CARDS_D(priv[P1], usedcards, 2, 1, );
	CardMask_OR(usedcards, usedcards, priv[P1]);

	MONTECARLO_N_CARDS_D(board[FLOP], usedcards, 3, 1, );
	CardMask_OR(usedcards, usedcards, board[FLOP]);

	MONTECARLO_N_CARDS_D(board[TURN], usedcards, 1, 1, );
	CardMask_OR(usedcards, usedcards, board[TURN]);

	MONTECARLO_N_CARDS_D(board[RIVER], usedcards, 1, 1, );
	CardMask_OR(usedcards, usedcards, board[RIVER]);

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

	totalwinningsbot0 += playonegame(priv, board, twoprob0wins); //returns utility of player/bot 0
	swap(_bots[0], _bots[1]);
	totalwinningsbot0 -= playonegame(priv, board, twoprob0wins); //returns utility of player/bot 0
	swap(_bots[0], _bots[1]);
}

double Playoff::playonegame(CardMask priv[2], CardMask board[4], int twoprob0wins) //return utility to player/bot 0
{
	_bots[P0]->setnewgame(P0, priv[P0], _sblind, _bblind, _stacksize);
	_bots[P1]->setnewgame(P1, priv[P1], _sblind, _bblind, _stacksize);
	
	double pot=0;
	switch(playoutaround(P1, pot))
	{
		case P0FOLD: return -pot; //return utility to player/bot 0
		case P1FOLD: return pot;
		case CALLALLIN: if(_bots[P0].islimit()) REPORT("limit allin!"); return (twoprob0wins-1) * _stacksize;
		case GO: break;
	}

	for(int gr=FLOP; gr<4; gr++)
	{
		_bots[P0]->setnextround(gr, board[gr], pot);
		_bots[P1]->setnextround(gr, board[gr], pot);
		switch(playoutaround(P0, pot))
		{
			case P0FOLD: return -pot;
			case P1FOLD: return pot;
			case CALLALLIN: if(_bots[P0].islimit()) REPORT("limit allin!"); return (twoprob0wins-1) * _stacksize;
			case GO: break;
		}
	}
	return (twoprob0wins-1) * pot; //this is the showdown
}

Result Playoff::playoutaround(Player nexttogo, double &pot)
{
	bool prev_act_was_allin = false;
	while(1)
	{
		//get the bot action

		double amount;
		Action act = _bots[nexttogo]->getbotaction(amount);

		if(pot+amount >= _stacksize)
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

		//The bot bet: either BET or ALLIN. Either way, continue the loop.

		_bots[P0]->doaction(nexttogo, act, amount);
		_bots[P1]->doaction(nexttogo, act, amount);
		nexttogo = (nexttogo == P0) ? P1 : P0;
		if(act == ALLIN) prev_act_was_allin = true;
	}
}

//this struct allows me to sort just an index array, by the values in this vector
//See the code below for usage or here for example: http://www.cplusplus.com/reference/algorithm/sort/
struct sortingvector
{
	sortingvector(int vector_size) : vect(vector_size) {}
	bool operator() (int i, int j) { return vect[i] < vect[j]; }
	vector<double> vect;
};

string bot(int i) 
{ 
	string ret = "A";
	ret[0]+=i;
	return ret; 
}

int main(int argc, char **argv)
{

	//two files supplied. play them against each other, repeatedly.

	if(argc==4)
	{
		Playoff * myplayoff = new Playoff(argv[1], argv[2]);
		while(1)
		{
			myplayoff->playsomegames(atol(argv[3])); 
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
			cout << ev << " +- " << std << endl;
		}
	}

	//three or more files supplied. play all of them against each other, then sort the results and
	//print a table

	else if (argc > 4)
	{
		const int numfiles = argc - 2;
		vector<string> myfiles;
		myfiles.reserve(numfiles);
		
		//initialize islimit so we can error if they aren't all the same

		bool islimit = BotAPI(argv[3]).islimit(); //creates temporary BotAPI object

		//load up the files

		for(int i=2; i<argc; i++)
		{
			if(BotAPI(argv[i]).islimit() != islimit) //tests if file is okay, and checks limit.
				REPORT("you entered both limit and no-limit bots!");
			myfiles.push_back(argv[i]);
		}

		//play the games, get the results

		const int64 numgames = atol(argv[1]);
		double interval=0;
		vector<vector<double> > results(numfiles, vector<double>(numfiles, 0));
		for(int i=0; i<numfiles; i++)
		{
			for(int j=i+1; j<numfiles; j++)
			{
				cout << "playing " << myfiles[i] << " against " << myfiles[j] << "..." << endl;
				Playoff playoff(myfiles[i], myfiles[j]);
				playoff.playsomegames(numgames);
				results[i][j] = playoff.getev(); //ev of file on LEFT, index on LEFT
				results[j][i] = -playoff.getev();
				interval = playoff.get95interval(); //same for all playoffs
			}
		}
		cout << endl;

		//get average results

		sortingvector avgresult(numfiles);
		vector<int> sorted(numfiles); //indexes into avgresult.vect
		for(int i=0; i<numfiles; i++)
		{
			avgresult.vect[i] = accumulate(results[i].begin(), results[i].end(), 0.0) / numfiles;
			sorted[i] = i;
		}

		//sort

		sort(sorted.begin(), sorted.end(), avgresult); //see comment above sortingvector

		//print info

		cout << "units are milli-big-blinds/hand, with respect to the ROW bot " << endl;
		cout << "and are good to plus/minus " << interval << " mb/hand (95% confidence)" << endl;
		cout << endl;

		//print file labels

		for(int i=0; i<numfiles; i++)
			cout << bot(i) << ": " << myfiles[sorted[i]] << endl;
		cout << endl;

		//print column headings

		cout << setw(10) << ' ';
		for(int i=0; i<numfiles; i++)
			cout << setw(10) << left << bot(i);
		cout << "Average" << endl;

		//print rest of table body
		
		for(int i=0; i<numfiles; i++)
		{
			cout << setw(10) << right << bot(i)+":  ";
			for(int j=0; j<numfiles; j++)
			{
				cout << fixed << setprecision(1) << left << setw(10) << results[sorted[i]][sorted[j]];
			}
			cout << fixed << setprecision(1) << avgresult.vect[sorted[i]] << endl;
		}

	}
	else
	{
		cout << "Usage: " << argv[0] << " xmlfile1 xmlfile2 numgames" << endl;
		cout << "   playes numgames and prints the results, over and over.." << endl;
		cout << "Usage: " << argv[0] << " numgames xmlfile1 ... xmlfileN" << endl;
		cout << "   builds a fuckin chart." << endl;
	}
}

