#ifndef _treenolimit_h_
#define _treenolimit_h_

#include "constants.h"


//the data in this struct defines which actions are available at any point
//and what the consequences of an action are.
struct betnode
{  
	char playertoact; // zero or one, may be casted to Player type
	char numacts; // the total number of actions available at this node

	//FD for fold, GO is next round, AI is called all-in, child number for more betting
	unsigned char result[MAX_ACTIONS]; 

	//how much is needed for this action, 
	//same as how much the pot gains if this action ends the round
	unsigned char potcontrib[MAX_ACTIONS]; 
};

//used to get a condensed betnode type structure.
extern inline void getnode(int gr, int pot, int beti, betnode &bn);

//must be called before anything else
void initbettingtrees();

//tells you if a node is betting all in based on the result and potcontrib values
bool isallin(int result, int potcontrib, int gr);

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
