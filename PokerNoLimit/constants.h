#ifndef __constants_h__
#define __constants_h__

#include "../HandIndexingV1/constants.h"

const int PREFLOP = 0;
const int FLOP = 1;
const int TURN = 2;
const int RIVER = 3;

/***********************************************************************************/
//constants for the binning of hands

/*****************************************/
/*should be copied from HandSSCalculator.*/
/*****************************************/
const int BIN_PREFLOP_MAX = 169;
const int BIN_FLOP_MAX = 8;
const int BIN_TURN_MAX = 8;
const int BIN_RIVER_MAX = 8;

const LPCTSTR FLOPFILENAME = TEXT("files/flop8BINS.dat");
const LPCTSTR TURNFILENAME = TEXT("files/turn8BINS.dat");
const LPCTSTR RIVERFILENAME = TEXT("files/river8BINS.dat");

const int BINSPERSTRUCT = 10;

//ensure INDEX2N_MAX is initialized by the time we get here, and that it knows what a binstruct is.
const unsigned int FLOPFILESIZE = (INDEX23_MAX/BINSPERSTRUCT+1)*4;
const unsigned int TURNFILESIZE = (INDEX24_MAX/BINSPERSTRUCT+1)*4;
const unsigned int RIVERFILESIZE = (INDEX25_MAX/BINSPERSTRUCT+1)*4;

/***********************************************************************************/
//constants for the flopalyzer

const int FLOPALYZER_MAX=6;
const int TURNALYZER_MAX=17;
const int RIVALYZER_MAX=37;

/***********************************************************************************/
//constants for the pot indexing, and those that define game parameters
const int POTI_FLOP_MAX = 6;
const int POTI_TURN_MAX = 6;
const int POTI_RIVER_MAX = 6;

//stacksize of the smallest stack, in small blinds, 
//as HU poker is only as good as its smaller stack.
const int STACKSIZE = 26; //26 should match that paper, with 13 big blinds each.

const unsigned char SB=1, BB=2;

const unsigned char B1=4,  R11=8,  R12=12, R13=14, R14=18, R15=22, R16=26;
const unsigned char B2=6,  R21=10, R22=14, R23=18, R24=22, R25=26, R26=30;
const unsigned char B3=8,  R31=12, R32=16, R33=20, R34=24, R35=28, R36=32;
const unsigned char B4=12, R41=16, R42=20, R43=24, R44=28, R45=32, R46=36;
const unsigned char B5=16, R51=20, R52=24, R53=28, R54=32, R55=36, R56=40;
const unsigned char B6=24, R61=28, R62=32, R63=36, R64=40, R65=44, R66=48;

/***********************************************************************************/
//constants for the betting tree itself
//THE SIZE OF THE BETTING TREE
const int N_NODES = 172;

//constants used in the betting tree
const unsigned char NA=0xFF;
const unsigned char AI=0xFE;
const unsigned char FD=0xFD;
const unsigned char GO5=0xFC; //check-bet-raise-call
const unsigned char GO4=0xFB; //bet-raise-call
const unsigned char GO3=0xFA; //check-bet-call
const unsigned char GO2=0xF9; //bet-call
const unsigned char GO1=0xF8; //check-check
const int BETHIST_MAX = 5; // number of GO's we have

/***********************************************************************************/
//constants that help keep track of the indexing of the game state.
const int SCENI_PREFLOP_MAX = BIN_PREFLOP_MAX;
const int SCENI_FLOP_MAX = BIN_FLOP_MAX * FLOPALYZER_MAX * POTI_FLOP_MAX * BETHIST_MAX;
const int SCENI_TURN_MAX = BIN_TURN_MAX * TURNALYZER_MAX * POTI_TURN_MAX * BETHIST_MAX*BETHIST_MAX;
const int SCENI_RIVER_MAX = BIN_RIVER_MAX * RIVALYZER_MAX * POTI_RIVER_MAX * BETHIST_MAX*BETHIST_MAX*BETHIST_MAX;
const int SCENI_MAX = SCENI_PREFLOP_MAX + SCENI_FLOP_MAX + SCENI_TURN_MAX + SCENI_RIVER_MAX;


/***********************************************************************************/
//constants used for managing the memory, and the beti segmentation indexing
//First, we segment the memory used into chunks, each covering an integral number of
//scenario indices. Chunks overcome memory fragmentation I hope.
const int N_CHUNKS = 1; //should be chosen to divide SCENI_MAX, otherwise, put a +1 below
const int SCENIPERCHUNK = (SCENI_MAX / N_CHUNKS);

//Now, we can access a certain sceni number from above. The only thing missing is accessing
//the particular parts that depend on beti. These numbers help allow that.
//These allow determining the max actions we have stored for in memory from betting inde.
const int BETI9_CUTOFF = 14;
const int BETI3_CUTOFF = 86;
const int BETI2_CUTOFF = 172;

const int BETI9_MAX = BETI9_CUTOFF;
const int BETI3_MAX = BETI3_CUTOFF-BETI9_CUTOFF;
const int BETI2_MAX = BETI2_CUTOFF-BETI3_CUTOFF;
const int BETI_MAX = BETI2_CUTOFF;

//These are offsets in bytes. They may not always be word aligned.
// a certain offset = the last offset  +  size of last offset
const int STRATT9_OFFSET = 0;
const int STRATN9_OFFSET = STRATT9_OFFSET + BETI9_MAX*8*sizeof(float);
const int STRATD9_OFFSET = STRATN9_OFFSET + BETI9_MAX*8*sizeof(float);
const int REGRET9_OFFSET = STRATD9_OFFSET + BETI9_MAX*8*sizeof(float);

const int STRATT3_OFFSET = REGRET9_OFFSET + BETI9_MAX*9*sizeof(float);
const int STRATN3_OFFSET = STRATT3_OFFSET + BETI3_MAX*2*sizeof(float);
const int STRATD3_OFFSET = STRATN3_OFFSET + BETI3_MAX*2*sizeof(float);
const int REGRET3_OFFSET = STRATD3_OFFSET + BETI3_MAX*2*sizeof(float);

const int STRATT2_OFFSET = REGRET3_OFFSET + BETI3_MAX*3*sizeof(float);
const int STRATN2_OFFSET = STRATT2_OFFSET + BETI2_MAX*1*sizeof(float);
const int STRATD2_OFFSET = STRATT2_OFFSET + BETI2_MAX*1*sizeof(float);
const int REGRET2_OFFSET = STRATT2_OFFSET + BETI2_MAX*1*sizeof(float);
const int SCENARIODATA_BYTES = REGRET2_OFFSET + BETI2_MAX*2*sizeof(float);

#endif