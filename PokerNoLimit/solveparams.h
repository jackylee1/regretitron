#ifndef __solveparams_h__
#define __solveparams_h__

#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h" //for treesettings_t
#include "../PokerLibrary/cardmachine.h" //for cardsettings_t
#include "../PokerLibrary/binstorage.h" //to determine bin filesize
#include "../utility.h" // for TOSTRING()
#include <boost/preprocessor.hpp>
#include "floatingpoint.h"

//solver settings
/* Note FloatCustom ranges: can usually use 5 or 6
   expbits = 4, 512
   expbits = 5, 131072
   expbits = 6, 8.6e9
   expbits = 7, 3.7e19
   *******/

typedef FloatCustomUnsigned< uint16, 6, true > FloatCustomUnsigned_uint16_6_true;
typedef FloatCustomSigned< uint8, 5, true > FloatCustomSigned_uint8_5_true;
typedef FloatCustomSigned< uint16, 5, true > FloatCustomSigned_uint16_5_true;
#define SOLVER_TYPES (9, ( \
( Working_type, float ), \
( PFlopStratn_type, double ), \
( FlopStratn_type, double ), \
( TurnStratn_type, double ), \
( RiverStratn_type, float ), \
( PFlopRegret_type, float ), \
( FlopRegret_type, float ), \
( TurnRegret_type, float ), \
( RiverRegret_type, float )))

#define SEPARATE_STRATN_REGRET 1
#define DYNAMIC_ALLOC_STRATN 0 /*must be false for __float128 to work due to alignment issues*/
#define DYNAMIC_ALLOC_REGRET 0 /*must be false for __float128 to work due to alignment issues*/
#define DYNAMIC_ALLOC_COUNTS 0

#define DO_THREADS /*undefine this to disable threading use*/
const double AGGRESSION_FACTOR = 0; //0 = "calm old man", 7 = "crazed, cocaine-driven maniac with an ax"
const bool THREADLOOPTRACE = false; //prints out debugging
const bool WALKERDEBUG = false; //debug print

//add your rake graduations in here if solving for a cash game
const string RAKE_TYPE = "none"; //added to XML file
inline int rake(int winningutility) { return winningutility; }

#define IMPERFECT_RECALL 0 /* 0 = perfect recall */

#define LIMIT 1

//controls how large the arrays are that are allocated in the stack in walker.
//they need to accomodate the largest number of actions that we might see
#if LIMIT
const int MAX_ACTIONS_SOLVER = 3;
#else
const int MAX_ACTIONS_SOLVER = MAX_ACTIONS; //defined in PokerLibrary/constants.h
#endif

//do not pollute and do not allow use
#undef LIMIT

//tuples are ( name, type ), otherwise this is COMPLETE MAGIC
#define TYPEDEF( tuple ) typedef BOOST_PP_TUPLE_ELEM(2,1,tuple) BOOST_PP_TUPLE_ELEM(2,0,tuple);
#define ARRAYELEM_TN( tuple ) { BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,0,tuple)), BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,1,tuple)) }
#define ARRAYELEM_TS( tuple ) sizeof( BOOST_PP_TUPLE_ELEM(2,0,tuple) )
#define MACROTD(r, state) TYPEDEF(BOOST_PP_ARRAY_ELEM( BOOST_PP_TUPLE_ELEM(2,0,state), BOOST_PP_TUPLE_ELEM(2,1,state)))
#define MACROARR_TN(r, state) ARRAYELEM_TN(BOOST_PP_ARRAY_ELEM( BOOST_PP_TUPLE_ELEM(2,0,state), BOOST_PP_TUPLE_ELEM(2,1,state))) \
	BOOST_PP_IIF( BOOST_PP_EQUAL( BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2,0,state)), BOOST_PP_ARRAY_SIZE( BOOST_PP_TUPLE_ELEM(2,1,state))), \
	BOOST_PP_EMPTY, BOOST_PP_COMMA )()
#define MACROARR_TS(r, state) ARRAYELEM_TS(BOOST_PP_ARRAY_ELEM( BOOST_PP_TUPLE_ELEM(2,0,state), BOOST_PP_TUPLE_ELEM(2,1,state))) \
	BOOST_PP_IIF( BOOST_PP_EQUAL( BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2,0,state)), BOOST_PP_ARRAY_SIZE( BOOST_PP_TUPLE_ELEM(2,1,state))), \
	BOOST_PP_EMPTY, BOOST_PP_COMMA )()
#define PRED(r, state) BOOST_PP_NOT_EQUAL(BOOST_PP_TUPLE_ELEM(2,0,state), BOOST_PP_ARRAY_SIZE(BOOST_PP_TUPLE_ELEM(2,1,state)))
#define OP(r, state) (BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2,0,state)), BOOST_PP_TUPLE_ELEM(2,1,state))
BOOST_PP_FOR( (0, SOLVER_TYPES), PRED, OP, MACROTD) //defines all my typedefs
const char* const TYPENAMES[][2] = { BOOST_PP_FOR( (0, SOLVER_TYPES), PRED, OP, MACROARR_TN) }; //defines key, value strings
const int TYPESIZES[] = { BOOST_PP_FOR( ( 0, SOLVER_TYPES ), PRED, OP, MACROARR_TS ) }; //an array of sizes
#undef TYPEDEF
#undef ARRAYELEM
#undef MACROTD
#undef MACROARR
#undef PRED
#undef OP

#endif
