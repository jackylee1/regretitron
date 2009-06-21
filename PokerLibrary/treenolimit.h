#ifndef _treenolimit_h_
#define _treenolimit_h_

#include "constants.h" //needed for MAX_ACTIONS and gamerounds
#include <vector> //needed for GetTreeSize
#include <string>
using namespace std;


//used to set the betting values, and hence the entire tree shape
struct treesettings_t
{
	unsigned char sblind, bblind;
	unsigned char bets[6];
	unsigned char raises[6][6];
	unsigned char stacksize;
	bool pushfold;
	bool limit;
};

struct BetNode
{  
	char playertoact; // zero or one, may be casted to Player type
	char numacts; // the total number of actions available at this node

	//this is hardcoded in the tree
	unsigned char result[MAX_ACTIONS]; 

	//values used for result[], if not a child node
	static const unsigned char NA=0xFF; //invalid action
	static const unsigned char AI=0xFE; //action is called all-in
	static const unsigned char FD=0xFD; //action is fold
	static const unsigned char GO=0xFC; //action ends betting for this round

	//how much is needed for this action / how much pot gains if betting ends
	unsigned char potcontrib[MAX_ACTIONS]; 
};

class BettingTree
{
public:

	BettingTree(const treesettings_t &mysettings);
	~BettingTree();

	//given a beti, pot, and gameround, gives you available bets
	//including children (of actions) referenced by beti
	inline void getnode(int gr, int pot, int beti, BetNode &bn) const;

	bool isallin(int result, int potcontrib, int gr) const;
	string actionstring(int gr, int action, const BetNode &bn, double multiplier) const;
	inline const treesettings_t& getparams() const { return myparams; }

private:
	//we do not support copying.
	BettingTree(const BettingTree& rhs);
	BettingTree& operator=(const BettingTree& rhs);

	//none of these are altered after the constructor is done
	BetNode * tree[4];
	const treesettings_t myparams;
};

inline void BettingTree::getnode(int gr, int pot, int beti, BetNode &bn) const
{
	bn.playertoact = tree[gr][beti].playertoact;
	bn.numacts = 0;
	for(int i=0; i < tree[gr][beti].numacts; i++)
	{
		if(pot + tree[gr][beti].potcontrib[i] < myparams.stacksize)
		{
			bn.potcontrib[(int)bn.numacts] = tree[gr][beti].potcontrib[i];
			bn.result[(int)bn.numacts] = tree[gr][beti].result[i];
			bn.numacts++;
		}
	}
}

class GetTreeSize
{
public:
	GetTreeSize(const BettingTree &tree, vector<vector<int> > &actionmax);
private:
	void walkercount(int gr, int pot, int beti);
	const BettingTree &mytree;
	vector<vector<int> > &myactionmax;
};







//------------------- code i may use in future --------------------

// may switch to this type of system in the future
#if 0
enum Action
{
	NONE, //used for previous action at beginning of gameround
	FOLD, //a player has folded
	CALL, //ends the betting, continuing at next round
	BET,  //keeps the betting going. could be check
	BETALL, //special actions to quickly identify all-in scenarios
	CALLALL
};

struct actiondef
{
	int numactions; //actual size of array
	Action action[MAX_ACTIONS];
	int value[MAX_ACTIONS]; // negative if not applicable.
};

extern inline void getnode(int gameround, Action prevact, int betturn, int invprev, int invacting, 
						   int moneyleft, int prevbeti, actiondef &ad);
#endif


#endif
