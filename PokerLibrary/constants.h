#ifndef __constants_h__
#define __constants_h__

#include "../HandIndexingV1/constants.h"

const int PREFLOP = 0;
const int FLOP = 1;
const int TURN = 2;
const int RIVER = 3;

enum Player
{
	P0 = 0,//first to act post-flop
	P1 = 1 //first to act pre-flop
};

const int MAX_ACTIONS = 9;
const int MAX_NODETYPES = MAX_ACTIONS-1; //because there are no single membered nodes

const int SAVE_FORMAT_VERSION = 3; //will increment on changes

#endif
