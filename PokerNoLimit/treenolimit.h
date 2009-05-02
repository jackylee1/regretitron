#ifndef _treenolimit_h_
#define _treenolimit_h_

// For extern vs static and const,
// see: http://www.gamedev.net/community/forums/topic.asp?topic_id=318620

//stacksize of the smallest stack, in small blinds, 
//as HU poker is only as good as its smaller stack.
const int stacksize = 26; //26 should match that paper, with 13 big blinds each.

const unsigned char SB=1, BB=2;

const unsigned char B1=4,  R11=8,  R12=12, R13=14, R14=18, R15=22, R16=26;
const unsigned char B2=6,  R21=10, R22=14, R23=18, R24=22, R25=26, R26=30;
const unsigned char B3=8,  R31=12, R32=16, R33=20, R34=24, R35=28, R36=32;
const unsigned char B4=12, R41=16, R42=20, R43=24, R44=28, R45=32, R46=36;
const unsigned char B5=16, R51=20, R52=24, R53=28, R54=32, R55=36, R56=40;
const unsigned char B6=24, R61=28, R62=32, R63=36, R64=40, R65=44, R66=48;

const unsigned char FD=0xFC;
const unsigned char GO=0xFD;
const unsigned char AI=0xFE;
const unsigned char NA=0xFF;

//THE SIZE OF THE BETTING TREE
const int N_NODES 172;

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

//THESE ARE THE GLOBAL VARIABLES THAT MAKE UP THE BETTING TREE.
//ONE IS FOR POST FLOP, THE OTHER IS FOR PREFLOP.
extern const betnode n[N_NODES];
extern       betnode pfn[N_NODES];

#endif
