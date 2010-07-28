#ifndef __solveparams_h__
#define __solveparams_h__

#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h" //for treesettings_t
#include "../PokerLibrary/cardmachine.h" //for cardsettings_t
#include "../PokerLibrary/binstorage.h" //to determine bin filesize
#include "../utility.h" // for TOSTRING()
#include <boost/preprocessor.hpp>
//CMD_LINE_VAR = 
// 2 - __float128
// 3 - __float80
// 4 - double
// 5 - float
// 6 - Half
// 7 - Byte
// 8 - Bit

#if CMD_LINE_VAR == 2
#define MYTYPE __float128
#elif CMD_LINE_VAR == 3
#define MYTYPE __float80
#elif CMD_LINE_VAR == 4
#define MYTYPE double
#elif CMD_LINE_VAR == 5
#define MYTYPE float
#elif CMD_LINE_VAR == 6
#define MYTYPE Half
class Half
{
#error no class
};
#elif CMD_LINE_VAR == 7
#define MYTYPE Bytes
class Bytes
{
#error no class
};
#elif CMD_LINE_VAR == 8
#define MYTYPE Bits
class Bits
{
#error no class
};
#else
#error "define CMD_LINE_VAR for type usage"
#endif

//settings for the settings - metasettings
const int PFBIN = 5;
const int FBIN = 5;
const int TBIN = 5;
const int RBIN = 5;
const bool USE_FLOPALYZER = false;
const int64 mysp_millions_iter = 100;
//end metasettings

const int64 THOUSAND = 1000;
const int64 MILLION = 1000000;
const int64 BILLION = THOUSAND*MILLION;

//main settings
const int64 TESTING_AMT = 1; //do this many iterations as a test for speed
const int64 STARTING_AMT = 10; //do this many iterations, then...
const int64 PLATEAU_AMT = 10; //do starting_amt this many times, then multiply by multiplier, then do that amt this many times, then....
const double MULTIPLIER = 10; //multiply by this amount, do that many, repeat ....
const int64 SAVEAFTER = 0; // save xml after this amount
const bool SAVESTRAT = true; //save strategy file when saving xml
const int64 STOPAFTER = mysp_millions_iter*MILLION; //stop after (at least) this amount of iterations

//solver settings
#define SOLVER_TYPES (6, ( \
( Working_type, MYTYPE ), \
( PFlopStore_type, MYTYPE ), \
( FlopStore_type, MYTYPE ), \
( TurnStore_type, MYTYPE ), \
( RiverStratn_type, MYTYPE ), \
( RiverRegret_type, MYTYPE )))
const string SAVENAME = tostring(PFBIN)+"bin-precision-f80randseed3";
const bool MEMORY_OVER_SPEED = false; //must be true for __float128 to work due to alignment issues
#define NUM_THREADS 1
#define N_LOOK 3 //affects threading performance
const double AGGRESSION_FACTOR = 0; //0 = "calm old man", 7 = "crazed, cocaine-driven maniac with an ax"
const bool SEED_RAND = false;
const int  SEED_WITH = 3;
const bool THREADLOOPTRACE = false; //prints out debugging
const bool WALKERDEBUG = false; //debug print

//tree settings
#define SB 1
#define BB 2
#define SS 48

//add your rake graduations in here if solving for a cash game
const string RAKE_TYPE = "none"; //added to XML file
inline int rake(int winningutility) { return winningutility; }

//bin settings
#define IMPERFECT_RECALL 0 /* 0 = perfect recall */
#if !IMPERFECT_RECALL /*PERFECT RECALL*/
const int PFLOP_CARDSI_MAX = PFBIN; // used by threading method, must be compile time constant.
const int RIVER_CARDSI_MAX = PFBIN*FBIN*TBIN*RBIN; // used by threading method, must be compile time constant.
const cardsettings_t CARDSETTINGS =
{

	{ PFBIN, FBIN, TBIN, RBIN },
	{
		"bins/preflop"+tostring(PFBIN),
		"bins/flop"+tostring(PFBIN)+"-"+tostring(FBIN),
		"bins/turn"+tostring(PFBIN)+"-"+tostring(FBIN)+"-"+tostring(TBIN),
		"bins/river"+tostring(PFBIN)+"-"+tostring(FBIN)+"-"+tostring(TBIN)+"-"+tostring(RBIN),
	},
	{
		PackedBinFile::numwordsneeded(PFBIN, INDEX2_MAX)*8,
		PackedBinFile::numwordsneeded(FBIN, INDEX23_MAX)*8,
		PackedBinFile::numwordsneeded(TBIN, INDEX231_MAX)*8,
		PackedBinFile::numwordsneeded(RBIN, INDEX2311_MAX)*8
	},
	true, //use history
	USE_FLOPALYZER //use flopalyzer
};

#else /*IMPERFECT_RECALL*/
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

//usegrouping is computed once in memorymanager at run-time, we define this here so choices can be made at compile time
//memorymanager ctor checks the value of this constant for correctness at run-time.
#if LIMIT && SS > 23 * BB
#define USEGROUPING 1
#else
#define USEGROUPING 0
#endif

//controls how large the arrays are that are allocated in the stack in walker.
//they need to accomodate the largest number of actions that we might see
#if LIMIT
const int MAX_ACTIONS_SOLVER = 3;
#else
const int MAX_ACTIONS_SOLVER = MAX_ACTIONS; //defined in PokerLibrary/constants.h
#endif

//do not pollute and do not allow use
#undef SS
#undef SB
#undef BB
#undef PUSHFOLD
#undef LIMIT

//turn off threads for windows
#if __GNUC__ && NUM_THREADS > 1
#define DO_THREADS
const int N_LOOKAHEAD = N_LOOK;
#else
#undef NUM_THREADS
#define NUM_THREADS 1
const int N_LOOKAHEAD = 0;
#endif

//tuples are ( name, type ), otherwise this is COMPLETE MAGIC
#define TYPEDEF( tuple ) typedef BOOST_PP_TUPLE_ELEM(2,1,tuple) BOOST_PP_TUPLE_ELEM(2,0,tuple);
#define ARRAYELEM( tuple ) { BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,0,tuple)), BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,1,tuple)) }
#define MACROTD(r, state) TYPEDEF(BOOST_PP_ARRAY_ELEM( BOOST_PP_TUPLE_ELEM(2,0,state), BOOST_PP_TUPLE_ELEM(2,1,state)))
#define MACROARR(r, state) ARRAYELEM(BOOST_PP_ARRAY_ELEM( BOOST_PP_TUPLE_ELEM(2,0,state), BOOST_PP_TUPLE_ELEM(2,1,state))) \
	BOOST_PP_IIF( BOOST_PP_EQUAL( BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2,0,state)), BOOST_PP_ARRAY_SIZE( BOOST_PP_TUPLE_ELEM(2,1,state))), \
	BOOST_PP_EMPTY, BOOST_PP_COMMA )()
#define PRED(r, state) BOOST_PP_NOT_EQUAL(BOOST_PP_TUPLE_ELEM(2,0,state), BOOST_PP_ARRAY_SIZE(BOOST_PP_TUPLE_ELEM(2,1,state)))
#define OP(r, state) (BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2,0,state)), BOOST_PP_TUPLE_ELEM(2,1,state))
BOOST_PP_FOR( (0, SOLVER_TYPES), PRED, OP, MACROTD) //defines all my typedefs
const char* const TYPENAMES[][2] = { BOOST_PP_FOR( (0, SOLVER_TYPES), PRED, OP, MACROARR) }; //defines key, value strings
#undef TYPEDEF
#undef ARRAYELEM
#undef MACROTD
#undef MACROARR
#undef PRED
#undef OP

#undef PFBIN
#undef FBIN
#undef TBIN
#undef RBIN

#endif
