#ifndef __solveparams_h__
#define __solveparams_h__

#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h" //for treesettings_t
#include "../PokerLibrary/cardmachine.h" //for cardsettings_t
#include "../PokerLibrary/binstorage.h" //to determine bin filesize
#include "../utility.h" // for TOSTRING()

//main settings
const char SAVENAME[] = "testbotapi4";
#define TESTXML 0
#define STOPAFTER 1*MILLION*MILLION
#define SAVEAFTER 1*MILLION

//solver settings
#define FPWORKING_T long double
#define FPSTORE_T double
#define STORE_DENOM 1
#define NUM_THREADS 2
const bool SEED_RAND = true;
const int  SEED_WITH = 42;

//bin settings
#define PFBIN 7
#define FBIN 128
#define TBIN 90
#define RBIN 32

const cardsettings_t CARDSETTINGS =
{
#if 0 // this for my new binning method with history

	{ PFBIN, FBIN, TBIN, RBIN },
	{
		string("bins/preflop" TOSTRING(PFBIN) "HistBins.dat"),
		string("bins/flop" TOSTRING(PFBIN) "-" TOSTRING(FBIN) "HistBins.dat"),
		string("bins/turn" TOSTRING(PFBIN) "-" TOSTRING(FBIN) "-" TOSTRING(TBIN) "HistBins.dat"),
		string("bins/river" TOSTRING(PFBIN) "-" TOSTRING(FBIN) "-" TOSTRING(TBIN) "-" TOSTRING(RBIN) "HistBins.dat"),
	},
	{
		PackedBinFile::numwordsneeded(PFBIN, INDEX2_MAX)*8,
		PackedBinFile::numwordsneeded(FBIN, INDEX23_MAX)*8,
		PackedBinFile::numwordsneeded(TBIN, INDEX231_MAX)*8,
		PackedBinFile::numwordsneeded(RBIN, INDEX2311_MAX)*8
	},
	true, //use history
	false //use flopalyzer

#else // this one is my old binning method, with no history

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
	true //use flopalyzer

#endif 
};


//tree settings
#define SB 1
#define BB 2
#define SS 13 //units of BB
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
	SS*BB, true, false //stacksize, pushfold, limit

#elif SS<=8 //all possible options are represented

	SB, BB,
	{2, 4, 6, 8, 10,12}, //Bn
	{{4, 6, 8, 10,12,14}, //R1n
	 {8, 12,16,20,24,28}, //R2n
	 {12,18,24,30,36,42}, //R3n
	 {16,24,32,40,48,56}, //R4n
	 {20,30,40,50,60,70}, //R5n
	 {24,36,48,60,72,84}}, //R6n
	SS*BB, false, false

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
	SS*BB, false, false
#else //smaller tree (same as used for prototype?)
	SB, BB,
	{2, 6, 12,20,99,99}, //B
	{6, 12,20,99,99,99}, //R1
	{12,18,99,99,99,99}, //R2
	{24,99,99,99,99,99}, //R3
	{99,99,99,99,99,99}, //R4
	{99,99,99,99,99,99}, //R5
	{99,99,99,99,99,99}, //R6
	SS*BB, false, false
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
	SS*BB, false, false

#elif SS>=35

	SB, BB,
	4, 12,24,36,50,66,
	12,24,36,50,66,99,
	24,36,50,66,99,99,
	36,50,66,99,99,99,
	50,66,99,99,99,99,
	66,99,99,99,99,99,
	99,99,99,99,99,99,
	SS*BB, false, false

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
	200, false, true

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
#undef FBIN
#undef TBIN
#undef RBIN

#endif
