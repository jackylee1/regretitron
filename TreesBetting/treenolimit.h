#ifndef TREE_NOLIMIT
#define TREE_NOLIMIT

// For extern vs static and const,
// see: http://www.gamedev.net/community/forums/topic.asp?topic_id=318620

//THIS IS A STUPID VALUE THAT I WILL PLACE ON THINGS THAT ARE NOT USED.
#define NA -2
//THIS IS FOR RESULT VALUES. SEE BELOW FOR DESCRIPTION.
#define GO 2
#define AI 3

//THE SIZE OF THE BETTING TREE
#define N_NODES 150 //tbd

//THIS IS THE STRUCTURE THAT MAKES UP ALL THE BETTING NODES 
//IN THE TREE THAT I WILL PLACE INTO MEMORY
struct betnode
{  
	char playertoact; // integer of the player whose turn it is
	char numacts; // the total number of actions available at this node
	char child[9]; // N_BETS+3 is max number of actions for any node.
	//if child[i] is NA, that's becauuse that action leads to a terminal node, 
	//and in that case, and only that case, these will be used:
	char result[9]; //after this action: 0 is P0 wins, 1 is P1 wins, GO is next round, AI is called all-in
	char potcontrib[9]; //if this action ends the betting sequence, how much the pot gains from each player
	char need[9]; //only a valid action if pot (before this round) + need < stacksize
};

//THESE ARE THE GLOBAL VARIABLES THAT MAKE UP THE BETTING TREE.
//ONE IS FOR POST FLOP, THE OTHER IS FOR PREFLOP.
extern const int stacksize[2]; //stack size, in small blinds, of each player.
extern const betnode n[N_NODES];
extern const betnode pfn[N_NODES];

#endif
