#ifndef __constants_h__
#define __constants_h__

//below constants depend on these constants.
#include "../HandIndexingV1/constants.h"

const int PREFLOP = 0;
const int FLOP = 1;
const int TURN = 2;
const int RIVER = 3;

/***********************************************************************************/
//constants for the binning of hands
const int BIN_PREFLOP_MAX = INDEX2_MAX; //should be 169, not really bins.

const int BIN_FLOP_MAX = 256;
const int BIN_TURN_MAX = 90;
const int BIN_RIVER_MAX = 32;
const char * const FLOPFILENAME = "bins/flop256BINS.dat";
const char * const TURNFILENAME = "bins/turn90BINS.dat";
const char * const RIVERFILENAME = "bins/river32BINS.dat";

/***********************************************************************************/
//constants for the flopalyzer

const int FLOPALYZER_MAX=6;
const int TURNALYZER_MAX=17;
const int RIVALYZER_MAX=37;

/*****************************************************************************/
//constants for cardsi returned by gamestate
const int CARDSI_PFLOP_MAX = BIN_PREFLOP_MAX;
const int CARDSI_FLOP_MAX = BIN_FLOP_MAX*FLOPALYZER_MAX;
const int CARDSI_TURN_MAX = BIN_TURN_MAX*TURNALYZER_MAX;
const int CARDSI_RIVER_MAX = BIN_RIVER_MAX*RIVALYZER_MAX;

//constants for the actiondefs
const int MAX_ACTIONS = 9;

//stacksize of the smallest stack, in small blinds, 
//as HU poker is only as good as its smaller stack.
#define SS 13
#define PUSHFOLD 0
const unsigned char SB=1, BB=2;
const int STACKSIZE = SS*BB;

#if PUSHFOLD
const unsigned char B1=99, R11=99, R12=99, R13=99, R14=99, R15=99, R16=99;
const unsigned char B2=99, R21=99, R22=99, R23=99, R24=99, R25=99, R26=99;
const unsigned char B3=99, R31=99, R32=99, R33=99, R34=99, R35=99, R36=99;
const unsigned char B4=99, R41=99, R42=99, R43=99, R44=99, R45=99, R46=99;
const unsigned char B5=99, R51=99, R52=99, R53=99, R54=99, R55=99, R56=99;
const unsigned char B6=99, R61=99, R62=99, R63=99, R64=99, R65=99, R66=99;
#elif SS<=8 //all possible options are represented
const unsigned char B1=2,  R11=4,  R12=6,  R13=8,  R14=10, R15=12, R16=99;
const unsigned char B2=4,  R21=8,  R22=12, R23=99, R24=99, R25=99, R26=99;
const unsigned char B3=6,  R31=12, R32=99, R33=99, R34=99, R35=99, R36=99;
const unsigned char B4=8,  R41=99, R42=99, R43=99, R44=99, R45=99, R46=99;
const unsigned char B5=10, R51=99, R52=99, R53=99, R54=99, R55=99, R56=99;
const unsigned char B6=12, R61=99, R62=99, R63=99, R64=99, R65=99, R66=99;
#elif SS==13
/*
const unsigned char B1=2,  R11=4,  R12=6,  R13=10, R14=14, R15=18, R16=22;
const unsigned char B2=6,  R21=8,  R22=16, R23=20, R24=24, R25=99, R26=99;
const unsigned char B3=8,  R31=16, R32=24, R33=99, R34=99, R35=99, R36=99;
const unsigned char B4=12, R41=24, R42=99, R43=99, R44=99, R45=99, R46=99;
const unsigned char B5=16, R51=99, R52=99, R53=99, R54=99, R55=99, R56=99;
const unsigned char B6=20, R61=99, R62=99, R63=99, R64=99, R65=99, R66=99;
*/
const unsigned char B1=2,  R11=6,  R12=12, R13=20, R14=99, R15=99, R16=99;
const unsigned char B2=6,  R21=12, R22=18, R23=99, R24=99, R25=99, R26=99;
const unsigned char B3=12, R31=24, R32=99, R33=99, R34=99, R35=99, R36=99;
const unsigned char B4=20, R41=99, R42=99, R43=99, R44=99, R45=99, R46=99;
const unsigned char B5=99, R51=99, R52=99, R53=99, R54=99, R55=99, R56=99;
const unsigned char B6=99, R61=99, R62=99, R63=99, R64=99, R65=99, R66=99;
#elif SS>=35
const unsigned char B1=4,  R11=12, R12=24, R13=36, R14=50, R15=66, R16=99;
const unsigned char B2=12, R21=24, R22=36, R23=50, R24=66, R25=99, R26=99;
const unsigned char B3=24, R31=36, R32=50, R33=66, R34=99, R35=99, R36=99;
const unsigned char B4=36, R41=50, R42=66, R43=99, R44=99, R45=99, R46=99;
const unsigned char B5=50, R51=66, R52=99, R53=99, R54=99, R55=99,  R56=100;
const unsigned char B6=66, R61=99, R62=99, R63=99, R64=99, R65=100, R66=100;
#endif

/***********************************************************************************/
//constants for the betting tree itself
//THE SIZE OF THE BETTING TREE
const int N_NODES = 172;

//constants used in the betting tree
const unsigned char NA=0xFF;
const unsigned char AI=0xFE;
const unsigned char FD=0xFD;
const unsigned char GO=0xFC;

#endif