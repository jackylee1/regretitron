#if 0
#ifndef TREE_LIMIT
#define TREE_LIMIT

// For extern vs static and const,
// see: http://www.gamedev.net/community/forums/topic.asp?topic_id=318620

//THIS IS THE STRUCTURE THAT MAKES UP ALL THE BETTING NODES 
//IN THE TREE THAT I WILL PLACE INTO MEMORY
struct betnode
{  
	char playertoact; // integer of the player whose turn it is
	char numacts; // the total number of actions available at this node
	char child[3]; // for the max of 3 actions.
	//if child[i] is null, that's becauuse that action leads to a terminal node, 
	//and in that case, and only that case, these will be used:
	char winner[3]; //after this action: -1 means continue to next round, 0 is P0 wins, 1 is P1 wins
	char utility[3]; //if this action ends the betting sequence, how much the pot gains
						//in units of 2*betsize (small or big, depending on round) to accomodate
						//small blind
};

//THIS IS A STUPID VALUE THAT I WILL PLACE ON THINGS THAT ARE NOT USED.
#define NA -2

//THESE ARE THE GLOBAL VARIABLES THAT MAKE UP THE BETTING TREE.
//ONE IS FOR POST FLOP, THE OTHER IS FOR PREFLOP.
extern const betnode n[10]; //10 is how many betindex values.
extern const betnode pfn[8]; //preflop is different.

#endif
#endif
