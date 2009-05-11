#ifndef __constants_h__
#define __constants_h__

#include "../HandIndexingV1/constants.h"

const int PREFLOP = 0;
const int FLOP = 1;
const int TURN = 2;
const int RIVER = 3;

/***********************************************************************************/
//constants for the binning of hands
#include "../SharedCode/binstruct8.h"
const int BIN_PREFLOP_MAX = INDEX2_MAX; //should be 169
const int BIN_FLOP_MAX = BIN_MAX; //comes from from binstructN.h
const int BIN_TURN_MAX = BIN_MAX;
const int BIN_RIVER_MAX = BIN_MAX;


/***********************************************************************************/
//constants for the flopalyzer

const int FLOPALYZER_MAX=6;
const int TURNALYZER_MAX=17;
const int RIVALYZER_MAX=37;

/***********************************************************************************/
//constants for the pot indexing, and those that define game parameters
const int POTI_FLOP_MAX = 10;
const int POTI_TURN_MAX = 10;
const int POTI_RIVER_MAX = 10;

const unsigned char SB=1, BB=2;

const unsigned char B1=2,  R11=4,  R12=8,  R13=14, R14=22, R15=32, R16=44;
const unsigned char B2=4,  R21=8,  R22=14, R23=22, R24=32, R25=44, R26=58;
const unsigned char B3=8,  R31=14, R32=22, R33=32, R34=44, R35=58, R36=74;
const unsigned char B4=14, R41=22, R42=32, R43=44, R44=58, R45=74, R46=92;
const unsigned char B5=22, R51=32, R52=44, R53=58, R54=74, R55=92,  R56=100;
const unsigned char B6=32, R61=44, R62=58, R63=74, R64=92, R65=100, R66=100;

//stacksize of the smallest stack, in small blinds, 
//as HU poker is only as good as its smaller stack.
const int STACKSIZE = 50*BB; //26 should match that paper, with 13 big blinds each.

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
const unsigned char GO_BASE=GO1;
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
#define N_CHUNKS 6

#if N_CHUNKS > 1
const int SCENIPERCHUNK = (SCENI_MAX / N_CHUNKS) + 1;
#endif

//Now, we can access a certain sceni number from above. The only thing missing is accessing
//the particular parts that depend on beti. These numbers help allow that.
//These allow determining the max actions we have stored for in memory from betting inde.
const int BETI_MAX = N_NODES;

const int BETI9_CUTOFF = 14;
const int BETI3_CUTOFF = 86;
const int BETI2_CUTOFF = BETI_MAX;

const int BETI9_MAX = BETI9_CUTOFF;
const int BETI3_MAX = BETI3_CUTOFF-BETI9_CUTOFF;
const int BETI2_MAX = BETI2_CUTOFF-BETI3_CUTOFF;

//These are offsets in bytes. They may not always be word aligned.
// a certain offset = the last offset  +  size of last offset
const int STRATN9_OFFSET = 0;
const int STRATD9_OFFSET = STRATN9_OFFSET + BETI9_MAX*8*sizeof(float);
const int REGRET9_OFFSET = STRATD9_OFFSET + BETI9_MAX*8*sizeof(float);

const int STRATN3_OFFSET = REGRET9_OFFSET + BETI9_MAX*9*sizeof(float);
const int STRATD3_OFFSET = STRATN3_OFFSET + BETI3_MAX*2*sizeof(float);
const int REGRET3_OFFSET = STRATD3_OFFSET + BETI3_MAX*2*sizeof(float);

const int STRATN2_OFFSET = REGRET3_OFFSET + BETI3_MAX*3*sizeof(float);
#if 0 //to match old results, set to 1 to put this back the way it was before i fixed the bug.
const int STRATD2_OFFSET = STRATN2_OFFSET;
const int REGRET2_OFFSET = STRATN2_OFFSET;
#else
const int STRATD2_OFFSET = STRATN2_OFFSET + BETI2_MAX*1*sizeof(float);
const int REGRET2_OFFSET = STRATD2_OFFSET + BETI2_MAX*1*sizeof(float);
#endif
const int SCENARIODATA_BYTES = REGRET2_OFFSET + BETI2_MAX*2*sizeof(float);

//separate indexing for stratt, as it's really a cache.
const int STRATT9_OFFSET = 0;
const int STRATT3_OFFSET = STRATT9_OFFSET + BETI9_MAX*8*sizeof(float);
const int STRATT2_OFFSET = STRATT3_OFFSET + BETI3_MAX*2*sizeof(float);
const int STRATT_WALKERI_BYTES = STRATT2_OFFSET + BETI2_MAX*1*sizeof(float);

/******************/
//constants for walker indexing in pokernolimit
const int WALKERI_PREFLOP_MAX = 1;
const int WALKERI_FLOP_MAX  = WALKERI_PREFLOP_MAX + BETHIST_MAX*POTI_FLOP_MAX;
const int WALKERI_TURN_MAX  = WALKERI_FLOP_MAX    + BETHIST_MAX*BETHIST_MAX*POTI_TURN_MAX;
const int WALKERI_RIVER_MAX = WALKERI_TURN_MAX    + BETHIST_MAX*BETHIST_MAX*BETHIST_MAX*POTI_RIVER_MAX;
const int WALKERI_MAX = WALKERI_RIVER_MAX;

#endif