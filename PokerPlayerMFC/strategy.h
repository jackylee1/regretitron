#ifndef __strategy_h__
#define __strategy_h__

#include "../PokerLibrary/constants.h"
#include "../utility.h"
#include <poker_defs.h>
#include <fstream>
#include <vector>
#include <utility>
#include <string>
using namespace std;

class CardMachine;
class BettingTree;

typedef vector< pair<int,int> > betihist_t;

class Strategy
{
public:
	//constructor reads xml
	Strategy(string xmlfilename);
	~Strategy();

	//access to BettingTree functions
	inline const BettingTree& gettree() const { return *tree; }

	//access to CardMachine functions
	inline CardMachine& getcardmach() const { return *cardmach; }

	//my own strategy file reading implementation
	void getprobs(const betihist_t &bh, const vector<CardMask> &cards, vector<double> &probs);

private:
	//we do not support copying.
	Strategy(const Strategy& rhs);
	Strategy& operator=(const Strategy& rhs);

	//the betting tree
	BettingTree * tree;
	vector< vector<int> > actionmax;

	//the card machine
	CardMachine * cardmach;

	//the strategy file
	ifstream strategyfile;
	vector< vector<int64> > dataoffset;

	//another walker incarnation used for indexing
	class Indexer
	{
		public:
			Indexer(const betihist_t &betihist, Strategy * strat);
			inline pair<int,int> getindex() { return go(0,0,0); }

		private:
			pair<int,int> go(int gr, int pot, int beti);
			int depth;
			const betihist_t &bh;
			vector< vector<int> > counters;
			Strategy * thisstrat;
	};
};

#endif

