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


class Strategy
{
public:
	//constructor reads xml
	Strategy(string xmlfilename);
	~Strategy();

	//access to filename of this bot
	inline string getfilename() const { return _xmlfilename; }

	//access to BettingTree functions
	inline const BettingTree& gettree() const { return *tree; }

	//access to CardMachine functions
	inline CardMachine& getcardmach() const { return *cardmach; }

	//my own strategy file reading implementation
	void getprobs(int gr, int actioni, int numa, const vector<CardMask> &cards, vector<double> &probs);

	int getactionmax(int gr, int nodei) const { return actionmax[gr][nodei]; }

private:
	//we do not support copying.
	Strategy(const Strategy& rhs);
	Strategy& operator=(const Strategy& rhs);

	//for diagnostics, and so i know who's playing
	string _xmlfilename;

	//the betting tree
	BettingTree * tree;
	vector< vector<int> > actionmax;

	//the card machine
	CardMachine * cardmach;

	//the strategy file
	ifstream strategyfile;
	vector< vector<int64> > dataoffset;
};

#endif

