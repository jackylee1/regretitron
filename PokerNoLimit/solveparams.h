#ifndef __solveparams_h__
#define __solveparams_h__

#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h" //for treesettings_t
#include "../PokerLibrary/cardmachine.h" //for cardsettings_t
#include "../PokerLibrary/binstorage.h" //to determine bin filesize

#define STRINGIFY(symb) #symb
#define TOSTRING(symb) STRINGIFY(symb)

//main settings
const char SAVENAME[] = "newesttest3";

//solver settings
#define FPWORKING_T float
#define FPSTORE_T float
#define STORE_DENOM 1
#define NUM_THREADS 4
const bool SEED_RAND = true;
const int  SEED_WITH = 42;

#define TESTXML 0
#define STOPAFTER 3*MILLION

//bin settings
#define FBIN 256
#define TBIN 90
#define RBIN 32

const cardsettings_t CARDSETTINGS =
{
	{ INDEX2_MAX, FBIN, TBIN, RBIN },
	{
		"",
		"bins/flop" TOSTRING(FBIN) "BINS.dat",
		"bins/turn" TOSTRING(TBIN) "BINS.dat",
		"bins/river" TOSTRING(RBIN) "BINS.dat",
	},
	{
		0,
		BinRoutines::filesize(FBIN, INDEX23_MAX),
		BinRoutines::filesize(TBIN, INDEX24_MAX),
		BinRoutines::filesize(RBIN, INDEX25_MAX)
	}
};


//tree settings
#define SB 1
#define BB 2
#define SS 13 //units of BB
#define PUSHFOLD 0
const treesettings_t TREESETTINGS = 
{
#if PUSHFOLD

	SB, BB, //smallblind, bigblind
	{99,99,99,99,99,99}, //B1 - B6
	{99,99,99,99,99,99}, //R11 - R16
	{99,99,99,99,99,99}, //R21 - R26
	{99,99,99,99,99,99}, //R31 - R36
	{99,99,99,99,99,99}, //R41 - R46
	{99,99,99,99,99,99}, //R51 - R56
	{99,99,99,99,99,99}, //R61 - R66
	SS*BB, true //stacksize, pushfold

#elif SS<=8 //all possible options are represented

	SB, BB,
	{2, 4, 6, 8, 10,12}, //Bn
	{{4, 6, 8, 10,12,14}, //R1n
	 {8, 12,16,20,24,28}, //R2n
	 {12,18,24,30,36,42}, //R3n
	 {16,24,32,40,48,56}, //R4n
	 {20,30,40,50,60,70}, //R5n
	 {24,36,48,60,72,84}}, //R6n
	SS*BB, false

#elif SS==13
	
#if 1
	SB, BB,
	{2, 4, 8, 12,16,20},
	{{4, 6, 10,14,18,22},
	 {8, 16,20,24,28,32},
	 {16,24,32,40,48,99},
	 {24,36,48,99,99,99},
	 {32,48,99,99,99,99},
	 {40,99,99,99,99,99}},
	SS*BB, false
#else
	SB, BB,
	{2, 6, 12,20,99,99}, //B
	{6, 12,20,99,99,99}, //R1
	{12,18,99,99,99,99}, //R2
	{24,99,99,99,99,99}, //R3
	{99,99,99,99,99,99}, //R4
	{99,99,99,99,99,99}, //R5
	{99,99,99,99,99,99}, //R6
	SS*BB, false
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
	SS*BB, false

#elif SS>=35

	SB, BB,
	4, 12,24,36,50,66,
	12,24,36,50,66,99,
	24,36,50,66,99,99,
	36,50,66,99,99,99,
	50,66,99,99,99,99,
	66,99,99,99,99,99,
	99,99,99,99,99,99,
	SS*BB, false

#endif
};

//do not pollute and do not allow use
#undef SS
#undef SB
#undef BB
#undef PUSHFOLD

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
#undef STRINGIFY
#undef TOSTRING
#undef FBIN
#undef TBIN
#undef RBIN

#endif
