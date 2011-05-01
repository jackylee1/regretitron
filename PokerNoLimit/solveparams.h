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

/* MAXEXPNEEDED:
   the third template parameter in a FloatCustom is the MAXEXPNEEDED. This number defines the maximum 
   exponent representable by the floating point number representation. It is a way of specifying
   the EXPBIAS used in the representatian. In principle, it could be anything, in practice, it is only 
   bounded by the master type (float or double usually) which is used for all calculations.
   
   if MAXEXPNEEDED is zero, then the maximum number representable by a FloatCustum would be just less
   than 2.0 (base-10), or more exactly 1.1111... (base-2) where the number of ones after the decimal 
   point is the same as the number of mantissa bits, naturally. The EXPBITS (in this case) would represent
   the exponent <= 0 by which a 2 raised to that exponent would multiply the mantissa. 

   This formula shows the effects of the MAXEXPNEEDED. For example, if MAXEXPNEEDED = 0, then the maximum
   representable number is just under 2.

   maximal representable number is just under: 
   			2^( 1 + MAXEXPNEEDED )

   EXPBITS then defines the amount of exponent space that is used for negative exponents. For example, 
   if EXPBITS is 3, and MAXEXPNEEDED is 4, then raw exponent value 1 to 7 would represent actual
   exponent values -2 to 4. A raw exponent value of 0 represents a denormalized number.

   for full stacksizes and a bigblind value of 2, a MAXEXPNEEDED of 16 is sufficient for regrets on 
   the river to keep them from overflowing. (5, 6, 8, 10, 10 bins)

   for full stacksizes and a bigblind value of 10, a MAXEXPNEEDED of 16 is insufficient for regrets on 
   the river to keep them from overflowing. (5 bins)

   for full stacksizes and a bigblind value of 10, a MAXEXPNEEDED of 18 is sufficient for regrets on
   the river to keep them from overflowing. (5 bins)
   */


//this one is good for river stratn
typedef FloatCustomUnsigned< uint16, 6, 32, true > FloatCustomUnsigned_uint16_6_32_true;
//these are good for river regret
typedef FloatCustomSigned< uint8, 5, 18, true > FloatCustomSigned_uint8_5_18_true;
typedef FloatCustomSigned< uint16, 5, 18, true > FloatCustomSigned_uint16_5_18_true;
//stratn's are write only
#define SOLVER_TYPES (9, ( \
( Working_type, float ), \
( PFlopStratn_type, double ), \
( FlopStratn_type, double ), \
( TurnStratn_type, double ), \
( RiverStratn_type, FloatCustomUnsigned_uint16_6_32_true ), \
( PFlopRegret_type, float ), \
( FlopRegret_type, float ), \
( TurnRegret_type, float ), \
( RiverRegret_type, FloatCustomSigned_uint8_5_18_true )))

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
