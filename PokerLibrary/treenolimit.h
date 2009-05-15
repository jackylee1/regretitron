#ifndef _treenolimit_h_
#define _treenolimit_h_

#include "constants.h"

// For extern vs static and const,
// see: http://www.gamedev.net/community/forums/topic.asp?topic_id=318620

//THIS IS THE STRUCTURE THAT MAKES UP ALL THE BETTING NODES 
//IN THE TREE THAT I WILL PLACE INTO MEMORY
struct betnode
{  
	char playertoact; // integer of the player whose turn it is
	char numacts; // the total number of actions available at this node
	unsigned char result[9]; //FD for fold, GO is next round, AI is called all-in, child number for more betting
	unsigned char potcontrib[9]; //if this action ends the betting sequence, how much the pot gains from each player
	//ALSO: actions are only valid if pot (before this round) + potcontrib < stacksize; Thus, potcontrib is needed for ALL actions
	//potcontrib as pot contribution only makes sense when we DON'T have a child node below us,
	//i.e. when the betting ends. potcontrib as amt needed SHOULD be superfulous when we are NOT
	//increasing the bet amount, i.e. betting or raising. Any time the bet amount is raised, 
	//there MUST be a child node, as the other player needs to confirm the new bet amount.
	//Summary: if a child is present, potcontrib tells us how much we need to make that action valid.
	//Otherwise, if this action is valid, then any other non-child invoking actions should be valid,
	//as they do not increase the pot.
	//AND, in these cases potcontrib may be less than is needed to be at that node, 
	//in case of folding to a bet. This wouldn't be an issue anyway, as the action really
	//still only requires potcontrib to be taken.
};

//to initialize the below variables
void initpfn();

//fills isvalid and returns num actions.
extern inline int getvalidity(const int pot, betnode const * mynode, bool isvalid[9]);

//returns index of a certain pot value
int getpoti(int gr, int pot);

//THESE ARE THE GLOBAL VARIABLES THAT MAKE UP THE BETTING TREE.
//ONE IS FOR POST FLOP, THE OTHER IS FOR PREFLOP.
extern const betnode n[N_NODES];
extern       betnode pfn[N_NODES];

#endif