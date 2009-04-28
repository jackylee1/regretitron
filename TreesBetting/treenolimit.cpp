#include "treenolimit.h"

// For extern vs static and const,
// see: http://www.gamedev.net/community/forums/topic.asp?topic_id=318620

const int stacksize[2] = {26, 26}; //26, 26 should match that paper, with 13 big blinds each.

const unsigned char B1=2, R11=4, R12=8, R13=12, R14=16, R15=20, R16=24;
const unsigned char B2=4, R21=8, R22=12, R23=16, R24=20, R25=24, R26=28;
const unsigned char B3=8, R31=12, R32=16, R33=20, R34=24, R35=28, R36=32;
const unsigned char B4=12, R41=16, R42=20, R43=24, R44=28, R45=32, R46=36;
const unsigned char B5=16, R51=20, R52=24, R53=28, R54=32, R55=36, R56=40;
const unsigned char B6=24, R61=28, R62=32, R63=36, R64=40, R65=44, R66=48;


//THESE ARE THE GLOBAL VARIABLES THAT MAKE UP THE BETTING TREE.
//ONE IS FOR POST FLOP, THE OTHER IS FOR PREFLOP.

#define NA6 NA,NA,NA,NA,NA,NA
#define NA7 NA,NA,NA,NA,NA,NA,NA

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
	{0,8, { 1, 2, 4, 6, 8, 10, 12, 98}, {0, B1, B2, B3, B4, B5, B6, 0}}, //0
	{1,8, {GO, 3, 5, 7, 9, 11, 13, 99}, {0, B1, B2, B3, B4, B5, B6, 0}}, //1

	//2-13, 9 available actions, responding to a first bet
	{1,9, {FD, GO, 14, 15, 16, 17, 18, 19, 86}, {0, B1, R11, R12, R13, R14, R15, R16, 0}}, //2
	{0,9, {FD, GO, 20, 21, 22, 23, 24, 25, 92}, {0, B1, R11, R12, R13, R14, R15, R16, 0}}, //3
	{1,9, {FD, GO, 26, 27, 28, 29, 30, 31, 87}, {0, B2, R21, R22, R23, R24, R25, R26, 0}}, //4
	{0,9, {FD, GO, 32, 33, 34, 35, 36, 37, 93}, {0, B2, R21, R22, R23, R24, R25, R26, 0}}, //5

	{1,9, {FD, GO, 38, 39, 40, 41, 42, 43, 88}, {0, B3, R31, R32, R33, R34, R35, R36, 0}}, //6
	{0,9, {FD, GO, 44, 45, 46, 47, 48, 49, 94}, {0, B3, R31, R32, R33, R34, R35, R36, 0}}, //7
	{1,9, {FD, GO, 50, 51, 52, 53, 54, 55, 89}, {0, B4, R41, R42, R43, R44, R45, R46, 0}}, //8
	{0,9, {FD, GO, 56, 57, 58, 59, 60, 61, 95}, {0, B4, R41, R42, R43, R44, R45, R46, 0}}, //9

	{1,9, {FD, GO, 62, 63, 64, 65, 66, 67, 90}, {0, B5, R51, R52, R53, R54, R55, R56, 0}}, //10
	{0,9, {FD, GO, 68, 69, 70, 71, 72, 73, 96}, {0, B5, R51, R52, R53, R54, R55, R56, 0}}, //11
	{1,9, {FD, GO, 74, 75, 76, 77, 78, 79, 91}, {0, B6, R61, R62, R63, R64, R65, R66, 0}}, //12
	{0,9, {FD, GO, 80, 81, 82, 83, 84, 85, 97}, {0, B6, R61, R62, R63, R64, R65, R66, 0}}, //13

	//14-19 & 20-25, 3 available actions, responding to raises to a bet B1
	{0,3, {FD, GO, 100, NA6}, {B1, R11, 0, NA6}}, //14
	{0,3, {FD, GO, 101, NA6}, {B1, R12, 0, NA6}}, //15
	{0,3, {FD, GO, 102, NA6}, {B1, R13, 0, NA6}}, //16
	{0,3, {FD, GO, 103, NA6}, {B1, R14, 0, NA6}}, //17
	{0,3, {FD, GO, 104, NA6}, {B1, R15, 0, NA6}}, //18
	{0,3, {FD, GO, 105, NA6}, {B1, R16, 0, NA6}}, //19

	{1,3, {FD, GO, 106, NA6}, {B1, R11, 0, NA6}}, //20
	{1,3, {FD, GO, 107, NA6}, {B1, R12, 0, NA6}}, //21
	{1,3, {FD, GO, 108, NA6}, {B1, R13, 0, NA6}}, //22
	{1,3, {FD, GO, 109, NA6}, {B1, R14, 0, NA6}}, //23
	{1,3, {FD, GO, 110, NA6}, {B1, R15, 0, NA6}}, //24
	{1,3, {FD, GO, 111, NA6}, {B1, R16, 0, NA6}}, //25

	//26-31 & 32-37, 3 available actions, responding to raises to a bet B2
	{0,3, {FD, GO, 112, NA6}, {B2, R21, 0, NA6}}, //26
	{0,3, {FD, GO, 113, NA6}, {B2, R22, 0, NA6}}, //27
	{0,3, {FD, GO, 114, NA6}, {B2, R23, 0, NA6}}, //28
	{0,3, {FD, GO, 115, NA6}, {B2, R24, 0, NA6}}, //29
	{0,3, {FD, GO, 116, NA6}, {B2, R25, 0, NA6}}, //30
	{0,3, {FD, GO, 117, NA6}, {B2, R26, 0, NA6}}, //31

	{1,3, {FD, GO, 118, NA6}, {B2, R21, 0, NA6}}, //32
	{1,3, {FD, GO, 119, NA6}, {B2, R22, 0, NA6}}, //33
	{1,3, {FD, GO, 120, NA6}, {B2, R23, 0, NA6}}, //34
	{1,3, {FD, GO, 121, NA6}, {B2, R24, 0, NA6}}, //35
	{1,3, {FD, GO, 122, NA6}, {B2, R25, 0, NA6}}, //36
	{1,3, {FD, GO, 123, NA6}, {B2, R26, 0, NA6}}, //37

	//38-43 & 44-49, 3 available actions, responding to raises to a bet B3
	{0,3, {FD, GO, 124, NA6}, {B3, R31, 0, NA6}}, //38
	{0,3, {FD, GO, 125, NA6}, {B3, R32, 0, NA6}}, //39
	{0,3, {FD, GO, 126, NA6}, {B3, R33, 0, NA6}}, //40
	{0,3, {FD, GO, 127, NA6}, {B3, R34, 0, NA6}}, //41
	{0,3, {FD, GO, 128, NA6}, {B3, R35, 0, NA6}}, //42
	{0,3, {FD, GO, 129, NA6}, {B3, R36, 0, NA6}}, //43

	{1,3, {FD, GO, 130, NA6}, {B3, R31, 0, NA6}}, //44
	{1,3, {FD, GO, 131, NA6}, {B3, R32, 0, NA6}}, //45
	{1,3, {FD, GO, 132, NA6}, {B3, R33, 0, NA6}}, //46
	{1,3, {FD, GO, 133, NA6}, {B3, R34, 0, NA6}}, //47
	{1,3, {FD, GO, 134, NA6}, {B3, R35, 0, NA6}}, //48
	{1,3, {FD, GO, 135, NA6}, {B3, R36, 0, NA6}}, //49

	//50-55 & 56-61, 3 available actions, responding to raises to a bet B4
	{0,3, {FD, GO, 136, NA6}, {B4, R41, 0, NA6}}, //50
	{0,3, {FD, GO, 137, NA6}, {B4, R42, 0, NA6}}, //51
	{0,3, {FD, GO, 138, NA6}, {B4, R43, 0, NA6}}, //52
	{0,3, {FD, GO, 139, NA6}, {B4, R44, 0, NA6}}, //53
	{0,3, {FD, GO, 140, NA6}, {B4, R45, 0, NA6}}, //54
	{0,3, {FD, GO, 141, NA6}, {B4, R46, 0, NA6}}, //55

	{1,3, {FD, GO, 142, NA6}, {B4, R41, 0, NA6}}, //56
	{1,3, {FD, GO, 143, NA6}, {B4, R42, 0, NA6}}, //57
	{1,3, {FD, GO, 144, NA6}, {B4, R43, 0, NA6}}, //58
	{1,3, {FD, GO, 145, NA6}, {B4, R44, 0, NA6}}, //59
	{1,3, {FD, GO, 146, NA6}, {B4, R45, 0, NA6}}, //60
	{1,3, {FD, GO, 147, NA6}, {B4, R46, 0, NA6}}, //61

	//62-67 & 68-73, 3 available actions, responding to raises to a bet B5
	{0,3, {FD, GO, 148, NA6}, {B5, R51, 0, NA6}}, //62
	{0,3, {FD, GO, 149, NA6}, {B5, R52, 0, NA6}}, //63
	{0,3, {FD, GO, 150, NA6}, {B5, R53, 0, NA6}}, //64
	{0,3, {FD, GO, 151, NA6}, {B5, R54, 0, NA6}}, //65
	{0,3, {FD, GO, 152, NA6}, {B5, R55, 0, NA6}}, //66
	{0,3, {FD, GO, 153, NA6}, {B5, R56, 0, NA6}}, //67

	{1,3, {FD, GO, 154, NA6}, {B5, R51, 0, NA6}}, //68
	{1,3, {FD, GO, 155, NA6}, {B5, R52, 0, NA6}}, //69
	{1,3, {FD, GO, 156, NA6}, {B5, R53, 0, NA6}}, //70
	{1,3, {FD, GO, 157, NA6}, {B5, R54, 0, NA6}}, //71
	{1,3, {FD, GO, 158, NA6}, {B5, R55, 0, NA6}}, //72
	{1,3, {FD, GO, 159, NA6}, {B5, R56, 0, NA6}}, //73

	//74-79 & 80-85, 3 available actions, responding to raises to a bet B6
	{0,3, {FD, GO, 160, NA6}, {B6, R61, 0, NA6}}, //74
	{0,3, {FD, GO, 161, NA6}, {B6, R62, 0, NA6}}, //75
	{0,3, {FD, GO, 162, NA6}, {B6, R63, 0, NA6}}, //76
	{0,3, {FD, GO, 163, NA6}, {B6, R64, 0, NA6}}, //77
	{0,3, {FD, GO, 164, NA6}, {B6, R65, 0, NA6}}, //78
	{0,3, {FD, GO, 165, NA6}, {B6, R66, 0, NA6}}, //79

	{1,3, {FD, GO, 166, NA6}, {B6, R61, 0, NA6}}, //80
	{1,3, {FD, GO, 167, NA6}, {B6, R62, 0, NA6}}, //81
	{1,3, {FD, GO, 168, NA6}, {B6, R63, 0, NA6}}, //82
	{1,3, {FD, GO, 169, NA6}, {B6, R64, 0, NA6}}, //83
	{1,3, {FD, GO, 170, NA6}, {B6, R65, 0, NA6}}, //84
	{1,3, {FD, GO, 171, NA6}, {B6, R66, 0, NA6}}, //85

	//86-91, 2 available actions, P0 responding to P1's all-in from P0's BN
	{0,2, {FD, AI, NA7}, {B1, 0, NA7}}, //86
	{0,2, {FD, AI, NA7}, {B2, 0, NA7}}, //87
	{0,2, {FD, AI, NA7}, {B3, 0, NA7}}, //88
	{0,2, {FD, AI, NA7}, {B4, 0, NA7}}, //89
	{0,2, {FD, AI, NA7}, {B5, 0, NA7}}, //90
	{0,2, {FD, AI, NA7}, {B6, 0, NA7}}, //91

	//92-97, 2 available actions, P1 responding to P0's all-in from P1's BN
	{1,2, {FD, AI, NA7}, {B1, 0, NA7}}, //92
	{1,2, {FD, AI, NA7}, {B2, 0, NA7}}, //93
	{1,2, {FD, AI, NA7}, {B3, 0, NA7}}, //94
	{1,2, {FD, AI, NA7}, {B4, 0, NA7}}, //95
	{1,2, {FD, AI, NA7}, {B5, 0, NA7}}, //96
	{1,2, {FD, AI, NA7}, {B6, 0, NA7}}, //97

	//98-99, 2 available actions, responding to P0 all-in off the bat, or P0 check - P1 allin
	{1,2, {FD, AI, NA7}, {0, 0, NA7}}, //98
	{0,2, {FD, AI, NA7}, {0, 0, NA7}}, //99

	//100-105 & 106-111, 2 available actions, responding to all-in off of R1N
	{1,2, {FD, AI, NA7}, {R11, 0, NA7}}, //100
	{1,2, {FD, AI, NA7}, {R12, 0, NA7}}, //101
	{1,2, {FD, AI, NA7}, {R13, 0, NA7}}, //102
	{1,2, {FD, AI, NA7}, {R14, 0, NA7}}, //103
	{1,2, {FD, AI, NA7}, {R15, 0, NA7}}, //104
	{1,2, {FD, AI, NA7}, {R16, 0, NA7}}, //105

	{0,2, {FD, AI, NA7}, {R11, 0, NA7}}, //106
	{0,2, {FD, AI, NA7}, {R12, 0, NA7}}, //107
	{0,2, {FD, AI, NA7}, {R13, 0, NA7}}, //108
	{0,2, {FD, AI, NA7}, {R14, 0, NA7}}, //109
	{0,2, {FD, AI, NA7}, {R15, 0, NA7}}, //110
	{0,2, {FD, AI, NA7}, {R16, 0, NA7}}, //111

	//112-117 & 118-123, 2 available actions, responding to all-in off of R2N
	{1,2, {FD, AI, NA7}, {R21, 0, NA7}}, //112
	{1,2, {FD, AI, NA7}, {R22, 0, NA7}}, //113
	{1,2, {FD, AI, NA7}, {R23, 0, NA7}}, //114
	{1,2, {FD, AI, NA7}, {R24, 0, NA7}}, //115
	{1,2, {FD, AI, NA7}, {R25, 0, NA7}}, //116
	{1,2, {FD, AI, NA7}, {R26, 0, NA7}}, //117

	{0,2, {FD, AI, NA7}, {R21, 0, NA7}}, //118
	{0,2, {FD, AI, NA7}, {R22, 0, NA7}}, //119
	{0,2, {FD, AI, NA7}, {R23, 0, NA7}}, //120
	{0,2, {FD, AI, NA7}, {R24, 0, NA7}}, //121
	{0,2, {FD, AI, NA7}, {R25, 0, NA7}}, //122
	{0,2, {FD, AI, NA7}, {R26, 0, NA7}}, //123

	//124-129 & 130-135, 2 available actions, responding to all-in off of R3N
	{1,2, {FD, AI, NA7}, {R31, 0, NA7}}, //124
	{1,2, {FD, AI, NA7}, {R32, 0, NA7}}, //125
	{1,2, {FD, AI, NA7}, {R33, 0, NA7}}, //126
	{1,2, {FD, AI, NA7}, {R34, 0, NA7}}, //127
	{1,2, {FD, AI, NA7}, {R35, 0, NA7}}, //128
	{1,2, {FD, AI, NA7}, {R36, 0, NA7}}, //129

	{0,2, {FD, AI, NA7}, {R31, 0, NA7}}, //130
	{0,2, {FD, AI, NA7}, {R32, 0, NA7}}, //131
	{0,2, {FD, AI, NA7}, {R33, 0, NA7}}, //132
	{0,2, {FD, AI, NA7}, {R34, 0, NA7}}, //133
	{0,2, {FD, AI, NA7}, {R35, 0, NA7}}, //134
	{0,2, {FD, AI, NA7}, {R36, 0, NA7}}, //135

	//136-141 & 142-147, 2 available actions, responding to all-in off of R4N
	{1,2, {FD, AI, NA7}, {R41, 0, NA7}}, //136
	{1,2, {FD, AI, NA7}, {R42, 0, NA7}}, //137
	{1,2, {FD, AI, NA7}, {R43, 0, NA7}}, //138
	{1,2, {FD, AI, NA7}, {R44, 0, NA7}}, //139
	{1,2, {FD, AI, NA7}, {R45, 0, NA7}}, //140
	{1,2, {FD, AI, NA7}, {R46, 0, NA7}}, //141

	{0,2, {FD, AI, NA7}, {R41, 0, NA7}}, //142
	{0,2, {FD, AI, NA7}, {R42, 0, NA7}}, //143
	{0,2, {FD, AI, NA7}, {R43, 0, NA7}}, //144
	{0,2, {FD, AI, NA7}, {R44, 0, NA7}}, //145
	{0,2, {FD, AI, NA7}, {R45, 0, NA7}}, //146
	{0,2, {FD, AI, NA7}, {R46, 0, NA7}}, //147

	//148-153 & 154-159, 2 available actions, responding to all-in off of R5N
	{1,2, {FD, AI, NA7}, {R51, 0, NA7}}, //148
	{1,2, {FD, AI, NA7}, {R52, 0, NA7}}, //149
	{1,2, {FD, AI, NA7}, {R53, 0, NA7}}, //150
	{1,2, {FD, AI, NA7}, {R54, 0, NA7}}, //151
	{1,2, {FD, AI, NA7}, {R55, 0, NA7}}, //152
	{1,2, {FD, AI, NA7}, {R56, 0, NA7}}, //153

	{0,2, {FD, AI, NA7}, {R51, 0, NA7}}, //154
	{0,2, {FD, AI, NA7}, {R52, 0, NA7}}, //155
	{0,2, {FD, AI, NA7}, {R53, 0, NA7}}, //156
	{0,2, {FD, AI, NA7}, {R54, 0, NA7}}, //157
	{0,2, {FD, AI, NA7}, {R55, 0, NA7}}, //158
	{0,2, {FD, AI, NA7}, {R56, 0, NA7}}, //159

	//160-165 & 166-171, 2 available actions, responding to all-in off of R6N
	{1,2, {FD, AI, NA7}, {R61, 0, NA7}}, //160
	{1,2, {FD, AI, NA7}, {R62, 0, NA7}}, //161
	{1,2, {FD, AI, NA7}, {R63, 0, NA7}}, //162
	{1,2, {FD, AI, NA7}, {R64, 0, NA7}}, //163
	{1,2, {FD, AI, NA7}, {R65, 0, NA7}}, //164
	{1,2, {FD, AI, NA7}, {R66, 0, NA7}}, //165

	{0,2, {FD, AI, NA7}, {R61, 0, NA7}}, //166
	{0,2, {FD, AI, NA7}, {R62, 0, NA7}}, //167
	{0,2, {FD, AI, NA7}, {R63, 0, NA7}}, //168
	{0,2, {FD, AI, NA7}, {R64, 0, NA7}}, //169
	{0,2, {FD, AI, NA7}, {R65, 0, NA7}}, //170
	{0,2, {FD, AI, NA7}, {R66, 0, NA7}}  //171
}; 
	
/**
//PREFLOP
const betnode pfn[N_NODES] = 
{// P#,#A, {children},                  {result},                    {potcontrib},                {need}                       
};
*/
