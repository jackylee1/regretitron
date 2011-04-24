#ifndef __solveparams_h__
#define __solveparams_h__

#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h" //for treesettings_t
#include "../PokerLibrary/cardmachine.h" //for cardsettings_t
#include "../PokerLibrary/binstorage.h" //to determine bin filesize
#include "../utility.h" // for TOSTRING()
#include <boost/preprocessor.hpp>

//solver settings
#define MYTYPE double
#define SOLVER_TYPES (6, ( \
( Working_type, MYTYPE ), \
( PFlopStore_type, MYTYPE ), \
( FlopStore_type, MYTYPE ), \
( TurnStore_type, MYTYPE ), \
( RiverStratn_type, MYTYPE ), \
( RiverRegret_type, MYTYPE )))
const bool MEMORY_OVER_SPEED = false; //must be true for __float128 to work due to alignment issues
#define NUM_THREADS 1
#define N_LOOK 3 //affects threading performance
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

#endif
