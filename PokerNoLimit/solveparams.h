#ifndef __solveparams_h__
#define __solveparams_h__

#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h" //for treesettings_t
#include "../PokerLibrary/cardmachine.h" //for cardsettings_t
#include "../PokerLibrary/binstorage.h" //to determine bin filesize
#include "../utility.h" // for TOSTRING()
#include <qd/qd_real.h>

const int64 THOUSAND = 1000;
const int64 MILLION = 1000000;
const int64 BILLION = THOUSAND*MILLION;

//main settings
const int64 TESTING_AMT = 10*THOUSAND; //do this many iterations as a test for speed
const int64 STARTING_AMT = 100*MILLION;//22300*MILLION/4; //do this many iterations, then...
const double MULTIPLIER = 1; //multiply by this amount, do that many, repeat ....
const int64 SAVEAFTER = 0; // save xml after this amount
const bool SAVESTRAT = true; //save strategy file when saving xml
const int64 STOPAFTER = 1*STARTING_AMT; //stop after (at least) this amount of iterations

//solver settings
#define FPSTORE_T double
#define FPWORKING_T long double
#define STORE_DENOM 0
#define NUM_THREADS 1
#define USE_HISTORY 1 //affects threading method
const double AGGRESSION_FACTOR = 0; //0 = "calm old man", 7 = "crazed, cocaine-driven maniac with an ax"
const int  N_LOOKAHEAD = 4; //affects threading performance
const bool SEED_RAND = true;
const int  SEED_WITH = 3;
const bool THREADLOOPTRACE = false; //prints out debugging
const bool WALKERDEBUG = false; //debug print

//tree settings
#define SB 1
#define BB 2
#define SS CMD_LINE_VAR

const string SAVENAME = "limit-5bin-newsys-ss" TOSTRING(SS);

//add your rake graduations in here if solving for a cash game
inline int rake(int winningutility) { return winningutility; }

//bin settings
#if USE_HISTORY // this for my new binning method with history
#define TEMP 5
#define PFBIN TEMP
#define FBIN TEMP
#define TBIN TEMP
#define RBIN TEMP
const int PFLOP_CARDSI_MAX = PFBIN; // used by threading method. 
const cardsettings_t CARDSETTINGS =
{

	{ PFBIN, FBIN, TBIN, RBIN },
	{
		string("bins/preflop" TOSTRING(PFBIN)),
		string("bins/flop" TOSTRING(PFBIN) "-" TOSTRING(FBIN)),
		string("bins/turn" TOSTRING(PFBIN) "-" TOSTRING(FBIN) "-" TOSTRING(TBIN)),
		string("bins/river" TOSTRING(PFBIN) "-" TOSTRING(FBIN) "-" TOSTRING(TBIN) "-" TOSTRING(RBIN)),
	},
	{
		PackedBinFile::numwordsneeded(PFBIN, INDEX2_MAX)*8,
		PackedBinFile::numwordsneeded(FBIN, INDEX23_MAX)*8,
		PackedBinFile::numwordsneeded(TBIN, INDEX231_MAX)*8,
		PackedBinFile::numwordsneeded(RBIN, INDEX2311_MAX)*8
	},
	true, //use history
	false //use flopalyzer
};

#undef TEMP
#else // this one is my old binning method, with no history
#define FBIN 256
#define TBIN 90
#define RBIN 90
const cardsettings_t CARDSETTINGS =
{

	{ INDEX2_MAX, FBIN, TBIN, RBIN },
	{
		string(""),
		string("bins/flop" TOSTRING(FBIN)),
		string("bins/turn" TOSTRING(TBIN)),
		string("bins/river" TOSTRING(RBIN)),
	},
	{
		0,
		PackedBinFile::numwordsneeded(FBIN, INDEX23_MAX)*8,
		PackedBinFile::numwordsneeded(TBIN, INDEX24_MAX)*8,
		PackedBinFile::numwordsneeded(RBIN, INDEX25_MAX)*8
	},
	false, //use history
	false //use flopalyzer
};
#endif 


//tree settings
#define PUSHFOLD 0
#define LIMIT 1
const treesettings_t TREESETTINGS = 
{
#if !LIMIT
#if PUSHFOLD

	SB, BB, //smallblind, bigblind
	{99,99,99,99,99,99},   //B1 - B6
	{{99,99,99,99,99,99},  //R11 - R16
	 {99,99,99,99,99,99},  //R21 - R26
	 {99,99,99,99,99,99},  //R31 - R36
	 {99,99,99,99,99,99},  //R41 - R46
	 {99,99,99,99,99,99},  //R51 - R56
	 {99,99,99,99,99,99}}, //R61 - R66
	SS, true, false //stacksize, pushfold, limit

#elif SS<=8 //all possible options are represented

	SB, BB,
	{2, 4, 6, 8, 10,12}, //Bn
	{{4, 6, 8, 10,12,14}, //R1n
	 {8, 12,16,20,24,28}, //R2n
	 {12,18,24,30,36,42}, //R3n
	 {16,24,32,40,48,56}, //R4n
	 {20,30,40,50,60,70}, //R5n
	 {24,36,48,60,72,84}}, //R6n
	SS, false, false

#elif SS==13
	
#if 1 //most options possible
	SB, BB,
	{2, 4, 8, 12,16,20},
	{{4, 6, 10,14,18,22},
	 {8, 16,20,24,28,32},
	 {16,24,32,40,48,99},
	 {24,36,48,99,99,99},
	 {32,48,99,99,99,99},
	 {40,99,99,99,99,99}},
	SS, false, false
#else //smaller tree (same as used for prototype?)
	SB, BB,
	{2, 6, 12,20,99,99}, //B
	{6, 12,20,99,99,99}, //R1
	{12,18,99,99,99,99}, //R2
	{24,99,99,99,99,99}, //R3
	{99,99,99,99,99,99}, //R4
	{99,99,99,99,99,99}, //R5
	{99,99,99,99,99,99}, //R6
	SS, false, false
#endif

#elif SS>=25 && SS<35

	SB, BB,
	{2, 6, 12,20,34,48}, //B
	{6, 12,20,30,42,56}, //R1
	{12,20,32,44,58,99}, //R2
	{20,34,46,60,99,99}, //R3
	{34,48,60,99,99,99}, //R4
	{48,60,99,99,99,99}, //R5
	{60,99,99,99,99,99}, //R6
	SS, false, false

#elif SS>=35

	SB, BB,
	4, 12,24,36,50,66,
	12,24,36,50,66,99,
	24,36,50,66,99,99,
	36,50,66,99,99,99,
	50,66,99,99,99,99,
	66,99,99,99,99,99,
	99,99,99,99,99,99,
	SS, false, false

#endif
#else //limit!

	SB, BB,
	{0,0,0,0,0,0},
	{{0,0,0,0,0,0},
	 {0,0,0,0,0,0},
	 {0,0,0,0,0,0},
	 {0,0,0,0,0,0},
	 {0,0,0,0,0,0},
	 {0,0,0,0,0,0}},
	SS, false, true

#endif
};

//do not pollute and do not allow use
#undef SS
#undef SB
#undef BB
#undef PUSHFOLD
#undef LIMIT

//turn off threads for windows
#if __GNUC__ && NUM_THREADS > 1
#define DO_THREADS
#else
#undef NUM_THREADS
#define NUM_THREADS 1
#endif

//typedef the data types and store as strings for logging
typedef FPWORKING_T fpworking_type;
typedef FPSTORE_T fpstore_type;
const char * const FPWORKING_TYPENAME = TOSTRING(FPWORKING_T);
const char * const FPSTORE_TYPENAME = TOSTRING(FPSTORE_T);
#undef FPWORKING_T
#undef FPSTORE_T
#undef PFBIN
#undef FBIN
#undef TBIN
#undef RBIN

#endif
