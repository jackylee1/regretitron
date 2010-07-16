#ifndef __strategy_h__
#define __strategy_h__

#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h"
#include "../utility.h"
#include <poker_defs.h>

#include <vector>
#include <utility>
#include <string>
using namespace std;

class CardMachine;


class Strategy
{
public:
	//constructor reads xml
	Strategy(string xmlfilename, bool preload);
	~Strategy();

	//access to filename of this bot
	inline string getfilename() const { return _xmlfilename; }

	//access to BettingTree
	inline const BettingTree& gettree() const { return *tree; }
	inline int getactmax(int r, int nact) const { return get_property(gettree(), maxorderindex_tag())[r][nact]; }
	inline const Vertex& gettreeroot() const { return treeroot; }

	//access to CardMachine functions
	inline CardMachine& getcardmach() const { return *cardmach; }

	//my own strategy file reading implementation
	void getprobs(int gr, int actioni, int numa, const vector<CardMask> &cards, vector<double> &probs);

	InFile& getstratfile() { return strategyfile; }

private:
	//we do not support copying.
	Strategy(const Strategy& rhs);
	Strategy& operator=(const Strategy& rhs);

	//for diagnostics, and so i know who's playing
	string _xmlfilename;

	//the betting tree
	BettingTree * tree;
	Vertex treeroot;

	//the card machine
	CardMachine * cardmach;

	//the strategy file
	InFile strategyfile;
	vector< vector<int64> > dataoffset;
};

#endif

