#include "stdafx.h"
#include "treenolimit.h"
#include "constants.h"

// For extern vs static and const,
// see: http://www.gamedev.net/community/forums/topic.asp?topic_id=318620

//defines the granularity with which the engine sees the pot
//only even values are possible since we assume bets are size of big blind
int getpoti(int gr, int pot)
{
	//they can only ever bet in multiples of the big blind, so all the numbers i'll ever
	//have should be even, because they can not agree upon a small blind bet.
	switch(pot)
	{
	case 1*BB:
		return 0;

	case 2*BB:
		return 1;

	case 3*BB:
		return 2;

	default:
		REPORT("invalid pot size in poti for flop");
	}
}

//returns number valid actions
inline int getvalidity(const int pot, betnode const * mynode, bool isvalid[9])
{
	int numa=0;

	switch(mynode->numacts)
	{
	default: REPORT("invalid maxa");
	case 9:
		if(isvalid[8] = (pot + mynode->potcontrib[8] < STACKSIZE)) numa++;
	case 8:
		if(isvalid[7] = (pot + mynode->potcontrib[7] < STACKSIZE)) numa++;
		if(isvalid[6] = (pot + mynode->potcontrib[6] < STACKSIZE)) numa++;
		if(isvalid[5] = (pot + mynode->potcontrib[5] < STACKSIZE)) numa++;
		if(isvalid[4] = (pot + mynode->potcontrib[4] < STACKSIZE)) numa++;
		if(isvalid[3] = (pot + mynode->potcontrib[3] < STACKSIZE)) numa++;
	case 3:
		if(isvalid[2] = (pot + mynode->potcontrib[2] < STACKSIZE)) numa++;
	case 2:
		if(isvalid[1] = (pot + mynode->potcontrib[1] < STACKSIZE)) numa++;
		if(isvalid[0] = (pot + mynode->potcontrib[0] < STACKSIZE)) numa++;
	}

	return numa;
}


//THESE ARE THE GLOBAL VARIABLES THAT MAKE UP THE BETTING TREE.
//ONE IS FOR POST FLOP, THE OTHER IS FOR PREFLOP.

#define NA6 NA,NA,NA,NA,NA,NA
#define NA7 NA,NA,NA,NA,NA,NA,NA
#define P0 0
#define P1 1

//Allowed result values: 
// FD if the player to act folds.
// GO if the play goes on in the next game round (including showdowns)
// AI if an all-in bet is called
// the number of a child node if the betting continues in the present round
// or NA if this action is not used.
extern const betnode n[N_NODES] =
{
	// {P#, #A, {result}, {potcontrib}}
	
	//0-1, 8 available actions, these are intitial node, and response to check.
	{P0,8, { 1, 2, 4, 6, 8, 10, 12, 98, NA}, {0, B1, B2, B3, B4, B5, B6, 0, NA}}, //0
	{P1,8, {GO1, 3, 5, 7, 9, 11, 13, 99, NA}, {0, B1, B2, B3, B4, B5, B6, 0, NA}}, //1

	//2-13, 9 available actions, responding to a first bet
	{P1,9, {FD, GO2, 14, 15, 16, 17, 18, 19, 86}, {0, B1, R11, R12, R13, R14, R15, R16, 0}}, //2
	{P0,9, {FD, GO3, 20, 21, 22, 23, 24, 25, 92}, {0, B1, R11, R12, R13, R14, R15, R16, 0}}, //3
	{P1,9, {FD, GO2, 26, 27, 28, 29, 30, 31, 87}, {0, B2, R21, R22, R23, R24, R25, R26, 0}}, //4
	{P0,9, {FD, GO3, 32, 33, 34, 35, 36, 37, 93}, {0, B2, R21, R22, R23, R24, R25, R26, 0}}, //5

	{P1,9, {FD, GO2, 38, 39, 40, 41, 42, 43, 88}, {0, B3, R31, R32, R33, R34, R35, R36, 0}}, //6
	{P0,9, {FD, GO3, 44, 45, 46, 47, 48, 49, 94}, {0, B3, R31, R32, R33, R34, R35, R36, 0}}, //7
	{P1,9, {FD, GO2, 50, 51, 52, 53, 54, 55, 89}, {0, B4, R41, R42, R43, R44, R45, R46, 0}}, //8
	{P0,9, {FD, GO3, 56, 57, 58, 59, 60, 61, 95}, {0, B4, R41, R42, R43, R44, R45, R46, 0}}, //9

	{P1,9, {FD, GO2, 62, 63, 64, 65, 66, 67, 90}, {0, B5, R51, R52, R53, R54, R55, R56, 0}}, //10
	{P0,9, {FD, GO3, 68, 69, 70, 71, 72, 73, 96}, {0, B5, R51, R52, R53, R54, R55, R56, 0}}, //11
	{P1,9, {FD, GO2, 74, 75, 76, 77, 78, 79, 91}, {0, B6, R61, R62, R63, R64, R65, R66, 0}}, //12
	{P0,9, {FD, GO3, 80, 81, 82, 83, 84, 85, 97}, {0, B6, R61, R62, R63, R64, R65, R66, 0}}, //13

	//14-19 & 20-25, 3 available actions, responding to raises to a bet B1
	{P0,3, {FD, GO4, 100, NA6}, {B1, R11, 0, NA6}}, //14
	{P0,3, {FD, GO4, 101, NA6}, {B1, R12, 0, NA6}}, //15
	{P0,3, {FD, GO4, 102, NA6}, {B1, R13, 0, NA6}}, //16
	{P0,3, {FD, GO4, 103, NA6}, {B1, R14, 0, NA6}}, //17
	{P0,3, {FD, GO4, 104, NA6}, {B1, R15, 0, NA6}}, //18
	{P0,3, {FD, GO4, 105, NA6}, {B1, R16, 0, NA6}}, //19

	{P1,3, {FD, GO5, 106, NA6}, {B1, R11, 0, NA6}}, //20
	{P1,3, {FD, GO5, 107, NA6}, {B1, R12, 0, NA6}}, //21
	{P1,3, {FD, GO5, 108, NA6}, {B1, R13, 0, NA6}}, //22
	{P1,3, {FD, GO5, 109, NA6}, {B1, R14, 0, NA6}}, //23
	{P1,3, {FD, GO5, 110, NA6}, {B1, R15, 0, NA6}}, //24
	{P1,3, {FD, GO5, 111, NA6}, {B1, R16, 0, NA6}}, //25

	//26-31 & 32-37, 3 available actions, responding to raises to a bet B2
	{P0,3, {FD, GO4, 112, NA6}, {B2, R21, 0, NA6}}, //26
	{P0,3, {FD, GO4, 113, NA6}, {B2, R22, 0, NA6}}, //27
	{P0,3, {FD, GO4, 114, NA6}, {B2, R23, 0, NA6}}, //28
	{P0,3, {FD, GO4, 115, NA6}, {B2, R24, 0, NA6}}, //29
	{P0,3, {FD, GO4, 116, NA6}, {B2, R25, 0, NA6}}, //30
	{P0,3, {FD, GO4, 117, NA6}, {B2, R26, 0, NA6}}, //31

	{P1,3, {FD, GO5, 118, NA6}, {B2, R21, 0, NA6}}, //32
	{P1,3, {FD, GO5, 119, NA6}, {B2, R22, 0, NA6}}, //33
	{P1,3, {FD, GO5, 120, NA6}, {B2, R23, 0, NA6}}, //34
	{P1,3, {FD, GO5, 121, NA6}, {B2, R24, 0, NA6}}, //35
	{P1,3, {FD, GO5, 122, NA6}, {B2, R25, 0, NA6}}, //36
	{P1,3, {FD, GO5, 123, NA6}, {B2, R26, 0, NA6}}, //37

	//38-43 & 44-49, 3 available actions, responding to raises to a bet B3
	{P0,3, {FD, GO4, 124, NA6}, {B3, R31, 0, NA6}}, //38
	{P0,3, {FD, GO4, 125, NA6}, {B3, R32, 0, NA6}}, //39
	{P0,3, {FD, GO4, 126, NA6}, {B3, R33, 0, NA6}}, //40
	{P0,3, {FD, GO4, 127, NA6}, {B3, R34, 0, NA6}}, //41
	{P0,3, {FD, GO4, 128, NA6}, {B3, R35, 0, NA6}}, //42
	{P0,3, {FD, GO4, 129, NA6}, {B3, R36, 0, NA6}}, //43

	{P1,3, {FD, GO5, 130, NA6}, {B3, R31, 0, NA6}}, //44
	{P1,3, {FD, GO5, 131, NA6}, {B3, R32, 0, NA6}}, //45
	{P1,3, {FD, GO5, 132, NA6}, {B3, R33, 0, NA6}}, //46
	{P1,3, {FD, GO5, 133, NA6}, {B3, R34, 0, NA6}}, //47
	{P1,3, {FD, GO5, 134, NA6}, {B3, R35, 0, NA6}}, //48
	{P1,3, {FD, GO5, 135, NA6}, {B3, R36, 0, NA6}}, //49

	//50-55 & 56-61, 3 available actions, responding to raises to a bet B4
	{P0,3, {FD, GO4, 136, NA6}, {B4, R41, 0, NA6}}, //50
	{P0,3, {FD, GO4, 137, NA6}, {B4, R42, 0, NA6}}, //51
	{P0,3, {FD, GO4, 138, NA6}, {B4, R43, 0, NA6}}, //52
	{P0,3, {FD, GO4, 139, NA6}, {B4, R44, 0, NA6}}, //53
	{P0,3, {FD, GO4, 140, NA6}, {B4, R45, 0, NA6}}, //54
	{P0,3, {FD, GO4, 141, NA6}, {B4, R46, 0, NA6}}, //55

	{P1,3, {FD, GO5, 142, NA6}, {B4, R41, 0, NA6}}, //56
	{P1,3, {FD, GO5, 143, NA6}, {B4, R42, 0, NA6}}, //57
	{P1,3, {FD, GO5, 144, NA6}, {B4, R43, 0, NA6}}, //58
	{P1,3, {FD, GO5, 145, NA6}, {B4, R44, 0, NA6}}, //59
	{P1,3, {FD, GO5, 146, NA6}, {B4, R45, 0, NA6}}, //60
	{P1,3, {FD, GO5, 147, NA6}, {B4, R46, 0, NA6}}, //61

	//62-67 & 68-73, 3 available actions, responding to raises to a bet B5
	{P0,3, {FD, GO4, 148, NA6}, {B5, R51, 0, NA6}}, //62
	{P0,3, {FD, GO4, 149, NA6}, {B5, R52, 0, NA6}}, //63
	{P0,3, {FD, GO4, 150, NA6}, {B5, R53, 0, NA6}}, //64
	{P0,3, {FD, GO4, 151, NA6}, {B5, R54, 0, NA6}}, //65
	{P0,3, {FD, GO4, 152, NA6}, {B5, R55, 0, NA6}}, //66
	{P0,3, {FD, GO4, 153, NA6}, {B5, R56, 0, NA6}}, //67

	{P1,3, {FD, GO5, 154, NA6}, {B5, R51, 0, NA6}}, //68
	{P1,3, {FD, GO5, 155, NA6}, {B5, R52, 0, NA6}}, //69
	{P1,3, {FD, GO5, 156, NA6}, {B5, R53, 0, NA6}}, //70
	{P1,3, {FD, GO5, 157, NA6}, {B5, R54, 0, NA6}}, //71
	{P1,3, {FD, GO5, 158, NA6}, {B5, R55, 0, NA6}}, //72
	{P1,3, {FD, GO5, 159, NA6}, {B5, R56, 0, NA6}}, //73

	//74-79 & 80-85, 3 available actions, responding to raises to a bet B6
	{P0,3, {FD, GO4, 160, NA6}, {B6, R61, 0, NA6}}, //74
	{P0,3, {FD, GO4, 161, NA6}, {B6, R62, 0, NA6}}, //75
	{P0,3, {FD, GO4, 162, NA6}, {B6, R63, 0, NA6}}, //76
	{P0,3, {FD, GO4, 163, NA6}, {B6, R64, 0, NA6}}, //77
	{P0,3, {FD, GO4, 164, NA6}, {B6, R65, 0, NA6}}, //78
	{P0,3, {FD, GO4, 165, NA6}, {B6, R66, 0, NA6}}, //79

	{P1,3, {FD, GO5, 166, NA6}, {B6, R61, 0, NA6}}, //80
	{P1,3, {FD, GO5, 167, NA6}, {B6, R62, 0, NA6}}, //81
	{P1,3, {FD, GO5, 168, NA6}, {B6, R63, 0, NA6}}, //82
	{P1,3, {FD, GO5, 169, NA6}, {B6, R64, 0, NA6}}, //83
	{P1,3, {FD, GO5, 170, NA6}, {B6, R65, 0, NA6}}, //84
	{P1,3, {FD, GO5, 171, NA6}, {B6, R66, 0, NA6}}, //85

	//86-91, 2 available actions, P0 responding to P1's all-in from P0's BN
	{P0,2, {FD, AI, NA7}, {B1, 0, NA7}}, //86
	{P0,2, {FD, AI, NA7}, {B2, 0, NA7}}, //87
	{P0,2, {FD, AI, NA7}, {B3, 0, NA7}}, //88
	{P0,2, {FD, AI, NA7}, {B4, 0, NA7}}, //89
	{P0,2, {FD, AI, NA7}, {B5, 0, NA7}}, //90
	{P0,2, {FD, AI, NA7}, {B6, 0, NA7}}, //91

	//92-97, 2 available actions, P1 responding to P0's all-in from P1's BN
	{P1,2, {FD, AI, NA7}, {B1, 0, NA7}}, //92
	{P1,2, {FD, AI, NA7}, {B2, 0, NA7}}, //93
	{P1,2, {FD, AI, NA7}, {B3, 0, NA7}}, //94
	{P1,2, {FD, AI, NA7}, {B4, 0, NA7}}, //95
	{P1,2, {FD, AI, NA7}, {B5, 0, NA7}}, //96
	{P1,2, {FD, AI, NA7}, {B6, 0, NA7}}, //97

	//98-99, 2 available actions, responding to P0 all-in off the bat, or P0 check - P1 allin
	{P1,2, {FD, AI, NA7}, {0, 0, NA7}}, //98
	{P0,2, {FD, AI, NA7}, {0, 0, NA7}}, //99

	//100-105 & 106-111, 2 available actions, responding to all-in off of R1N
	{P1,2, {FD, AI, NA7}, {R11, 0, NA7}}, //100
	{P1,2, {FD, AI, NA7}, {R12, 0, NA7}}, //101
	{P1,2, {FD, AI, NA7}, {R13, 0, NA7}}, //102
	{P1,2, {FD, AI, NA7}, {R14, 0, NA7}}, //103
	{P1,2, {FD, AI, NA7}, {R15, 0, NA7}}, //104
	{P1,2, {FD, AI, NA7}, {R16, 0, NA7}}, //105

	{P0,2, {FD, AI, NA7}, {R11, 0, NA7}}, //106
	{P0,2, {FD, AI, NA7}, {R12, 0, NA7}}, //107
	{P0,2, {FD, AI, NA7}, {R13, 0, NA7}}, //108
	{P0,2, {FD, AI, NA7}, {R14, 0, NA7}}, //109
	{P0,2, {FD, AI, NA7}, {R15, 0, NA7}}, //110
	{P0,2, {FD, AI, NA7}, {R16, 0, NA7}}, //111

	//112-117 & 118-123, 2 available actions, responding to all-in off of R2N
	{P1,2, {FD, AI, NA7}, {R21, 0, NA7}}, //112
	{P1,2, {FD, AI, NA7}, {R22, 0, NA7}}, //113
	{P1,2, {FD, AI, NA7}, {R23, 0, NA7}}, //114
	{P1,2, {FD, AI, NA7}, {R24, 0, NA7}}, //115
	{P1,2, {FD, AI, NA7}, {R25, 0, NA7}}, //116
	{P1,2, {FD, AI, NA7}, {R26, 0, NA7}}, //117

	{P0,2, {FD, AI, NA7}, {R21, 0, NA7}}, //118
	{P0,2, {FD, AI, NA7}, {R22, 0, NA7}}, //119
	{P0,2, {FD, AI, NA7}, {R23, 0, NA7}}, //120
	{P0,2, {FD, AI, NA7}, {R24, 0, NA7}}, //121
	{P0,2, {FD, AI, NA7}, {R25, 0, NA7}}, //122
	{P0,2, {FD, AI, NA7}, {R26, 0, NA7}}, //123

	//124-129 & 130-135, 2 available actions, responding to all-in off of R3N
	{P1,2, {FD, AI, NA7}, {R31, 0, NA7}}, //124
	{P1,2, {FD, AI, NA7}, {R32, 0, NA7}}, //125
	{P1,2, {FD, AI, NA7}, {R33, 0, NA7}}, //126
	{P1,2, {FD, AI, NA7}, {R34, 0, NA7}}, //127
	{P1,2, {FD, AI, NA7}, {R35, 0, NA7}}, //128
	{P1,2, {FD, AI, NA7}, {R36, 0, NA7}}, //129

	{P0,2, {FD, AI, NA7}, {R31, 0, NA7}}, //130
	{P0,2, {FD, AI, NA7}, {R32, 0, NA7}}, //131
	{P0,2, {FD, AI, NA7}, {R33, 0, NA7}}, //132
	{P0,2, {FD, AI, NA7}, {R34, 0, NA7}}, //133
	{P0,2, {FD, AI, NA7}, {R35, 0, NA7}}, //134
	{P0,2, {FD, AI, NA7}, {R36, 0, NA7}}, //135

	//136-141 & 142-147, 2 available actions, responding to all-in off of R4N
	{P1,2, {FD, AI, NA7}, {R41, 0, NA7}}, //136
	{P1,2, {FD, AI, NA7}, {R42, 0, NA7}}, //137
	{P1,2, {FD, AI, NA7}, {R43, 0, NA7}}, //138
	{P1,2, {FD, AI, NA7}, {R44, 0, NA7}}, //139
	{P1,2, {FD, AI, NA7}, {R45, 0, NA7}}, //140
	{P1,2, {FD, AI, NA7}, {R46, 0, NA7}}, //141

	{P0,2, {FD, AI, NA7}, {R41, 0, NA7}}, //142
	{P0,2, {FD, AI, NA7}, {R42, 0, NA7}}, //143
	{P0,2, {FD, AI, NA7}, {R43, 0, NA7}}, //144
	{P0,2, {FD, AI, NA7}, {R44, 0, NA7}}, //145
	{P0,2, {FD, AI, NA7}, {R45, 0, NA7}}, //146
	{P0,2, {FD, AI, NA7}, {R46, 0, NA7}}, //147

	//148-153 & 154-159, 2 available actions, responding to all-in off of R5N
	{P1,2, {FD, AI, NA7}, {R51, 0, NA7}}, //148
	{P1,2, {FD, AI, NA7}, {R52, 0, NA7}}, //149
	{P1,2, {FD, AI, NA7}, {R53, 0, NA7}}, //150
	{P1,2, {FD, AI, NA7}, {R54, 0, NA7}}, //151
	{P1,2, {FD, AI, NA7}, {R55, 0, NA7}}, //152
	{P1,2, {FD, AI, NA7}, {R56, 0, NA7}}, //153

	{P0,2, {FD, AI, NA7}, {R51, 0, NA7}}, //154
	{P0,2, {FD, AI, NA7}, {R52, 0, NA7}}, //155
	{P0,2, {FD, AI, NA7}, {R53, 0, NA7}}, //156
	{P0,2, {FD, AI, NA7}, {R54, 0, NA7}}, //157
	{P0,2, {FD, AI, NA7}, {R55, 0, NA7}}, //158
	{P0,2, {FD, AI, NA7}, {R56, 0, NA7}}, //159

	//160-165 & 166-171, 2 available actions, responding to all-in off of R6N
	{P1,2, {FD, AI, NA7}, {R61, 0, NA7}}, //160
	{P1,2, {FD, AI, NA7}, {R62, 0, NA7}}, //161
	{P1,2, {FD, AI, NA7}, {R63, 0, NA7}}, //162
	{P1,2, {FD, AI, NA7}, {R64, 0, NA7}}, //163
	{P1,2, {FD, AI, NA7}, {R65, 0, NA7}}, //164
	{P1,2, {FD, AI, NA7}, {R66, 0, NA7}}, //165

	{P0,2, {FD, AI, NA7}, {R61, 0, NA7}}, //166
	{P0,2, {FD, AI, NA7}, {R62, 0, NA7}}, //167
	{P0,2, {FD, AI, NA7}, {R63, 0, NA7}}, //168
	{P0,2, {FD, AI, NA7}, {R64, 0, NA7}}, //169
	{P0,2, {FD, AI, NA7}, {R65, 0, NA7}}, //170
	{P0,2, {FD, AI, NA7}, {R66, 0, NA7}}  //171
}; 
	

//PREFLOP
betnode pfn[N_NODES];

//Alas, you will have to initialize!
void initpfn()
{
	//Step 0, copy the tree. fix player to act.
	for(int i=0; i<N_NODES; i++)
	{
		//copy post-flop tree
		pfn[i] = n[i];
		//change player to act to reflect reversed order preflop.
		pfn[i].playertoact = 1-n[i].playertoact;
	}

	//Step 1, bump up all non-NA or 0 (All in, or check/fold nothing) valiues to +BB, 
	//to account for the betting floor
	for(int i=0; i<N_NODES; i++)
	{
		for(int j=0; j<9; j++)
		{
			if(pfn[i].potcontrib[j] != NA && pfn[i].potcontrib[j] != 0)
				pfn[i].potcontrib[j] += BB;
		}
	}

	//FIX NODE ZERO

	//shift the 8 actions of the first node from 0-7 to 1-8 ...
	for(int i=8; i>0; i--)
	{
		pfn[0].potcontrib[i] = pfn[0].potcontrib[i-1];
		pfn[0].result[i] = pfn[0].result[i-1];
	}
		
	//... so that we can accomodate the new small blind folding action
	pfn[0].result[0] = FD;
	pfn[0].potcontrib[0] = SB;

	//change number of actions to 9.
	pfn[0].numacts = 9;

	//finally, change 'checking' value to 'calling' big blind amount
	pfn[0].potcontrib[1] = BB;

	//FIX NODE ONE
	//change 'checking' value to 'calling' big blind amount
	pfn[1].potcontrib[0] = BB;

	//FIX NODES WITH FOLD ZERO (2-13, 98, 99)

	//Now, must change the fold valuations from 0 to big blind.
	//responses to the six initial bets over BB, for each player
	for(int i=2; i<=13; i++)
		pfn[i].potcontrib[0] = BB;

	//and response to all-in over BB, for each player.
	pfn[98].potcontrib[0] = BB;
	pfn[99].potcontrib[0] = BB;
}