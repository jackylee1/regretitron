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

/* DIGRESSION ON MY FLOATING POINT NUMBERS
   I have implemented tiny (1-byte) template-parameter-cofigurable floating point numbers that work
   in exactly the same way as IEEE floating point numbers -- mine just use fewer bits -- and mine
   also use randomness to choose which representable number to use. There may be a fruitful avenue 
   to improve them further (see future developments below), but for now, these work.

   The effects of all these parameters can be seen by simply printing out the 256 representable
   numbers by type-punning to a uint8.
   ***/

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

   EXPBITS then defines the amount of exponent space that is used for exponents less than MAXEXPNEEDED. 

   Since the bigblind value defines the scale of the solution, using larger numbers requires a larger
   value of MAXEXPNEEDED. Here are needed values of MAXEXPNEEDED for a variety of 4-bin solutions.

   At 4 bins, full stacksize, and bigblind value of 10, result of MAXEXPNEEDED values 
   used for 16-bit STRATN's:
     32 through 17 => solves completely (45M iterations)
     16 => overflows between 32M and 36M iterations
     15 => overflows between 16M and 20M iterations
     14 => overflows between 8M and 12M iterations
     13 => overflows between 4M and 8M iterations
     12 => overflows before 4M iterations
     11 => overflows before 4M iterations
     10 => overflows before 4M iterations

   At 4 bins, full stacksize, and bigblind value of 10, result of MAXEXPNEEDED values 
   used for 8-bit REGRET's:
     18 => solves completely (45M iterations)
     17 => solves completely
     16 => overflows between 16M and 20M iterations
     15 => overflows between 12M and 16M iterations

   ***/

/* EXPBITS

   As stated above EXPBITS defines the amount of exponent space used for values less than MAXEXPNEEDED.
   The effects of adding or removing a bit is dramatic, many expbits quickly allow for extremely tiny 
   representable numbers, few expbits make the smallest positive representable number around 10^3.
   The only correct way to decide what's best is by testing. 

   The following is all 5-bin games solved to 100M iterations with MAXEXPNEEDED at 18, and bblind at 10
   All tests use doubles on the preflop, flop, and turn for stratn, and floats on the preflop, flop, and turn
   for regrets, with float working type.

   results of playoffs vs ref-5-M100.xml (at 320M games .. significant to +- 0.251558 milli-big-blinds / hand)

   with river regret as an 8-bit CustomFloat with EXPBITS = N, and river stratn as a float
   N = 6: -2.319
   N = 5: -1.087
   N = 4: -0.346
   N = 3: -5.831
   N = 2: -31.38
   N = 1: -51.06

   with river stratn as an 8-bit CustomFloat with EXPBITS = N, and river regret as a float
   N = 7: overflowed 	(with MAXEXPNEEDED = 18)
   N = 6: overflowed 	(with MAXEXPNEEDED = 18)
   N = 7: overflowed	(with MAXEXPNEEDED = 19)
   N = 6: -0.078		(with MAXEXPNEEDED = 19)
   N = 5: +0.057
   N = 4: +0.065
   N = 3: -0.186
   N = 2: -1.851
   N = 1: -3.421

   From these data, it seems using 4-bits is best. So I did a further test

   playoff the stratn with 4 expbits vs the regret with 4 expbits (save solves used above
   ==> 4-bits wins +0.032 aganist the 5-bits after 3.731 billion games (+- 0.074 mbb/hand)

   with river stratn AND river regret as 8-bit CustomFloat with EXPBITS = 4...
   ==> wins -0.466 against ref-5-M100.xml after 2.607 billion games (+- 0.088 mbb/hand)

   As a control, I tried with river stratn AND river regret as 4-byte floats...
   ==> wins +0.039 against ref-5-M100.xml after 2.620 billion games (+- 0.088 mbb/hand)

   ***/

/* PERFORMANCE

   The main drawback of the FloatCustom is performance, when solving the above two strategies
   one using EXPBITS = 4 for both river regret and river stratn, and the other using all floats,
   I got these iterations per second on average for the *entire* 100 million iterations.

   both4: 7759  
   float: 11080

   The one-byte floats use 4 times less memory, and allow me to solve about a 4 times larger
   strategy, which at the same speed would take 4 times longer, but instead takes 5.71 times longer.

   ***/

/* FUTURE DEVELOPMENTS

   Seeing how single byte floats work well, it would be totally reasonable to use a lookup table in place 
   of all this floating point bull crap. That way I could choose the representable numbers at will, and ensure
   they are exactly exponentially distributed. Reading a float would be far faster than the current method,
   and with work, I could optimize the performance of writing a float. Writing a float involves finding the 
   two representable numbers in the table closest to the desired number. For stratn's it makes sense to optimize 
   for adding a small number to the existing one in the += operator. For regret's the best strategy is less clear,
   a tree search would be an obvious contender.

   ***/

#define CUSTOMFLOATNAME_( sign, temp1, temp2, temp3, temp4 ) FloatCustom##sign##_##temp1##_##temp2##_##temp3##_##temp4
#define CUSTOMFLOATNAME( sign, temp1, temp2, temp3, temp4 ) CUSTOMFLOATNAME_( sign, temp1, temp2, temp3, temp4 )

//unsigned are good for river stratn
typedef FloatCustomUnsigned< uint8, 4, 22, false > CUSTOMFLOATNAME( Unsigned, uint8, 4, 22, false );

//signed are good for river regret
typedef FloatCustomSigned< uint8, 4, 22, false > CUSTOMFLOATNAME( Signed, uint8, 4, 22, false );

//stratn's are write only
#define SOLVER_TYPES (9, ( \
( Working_type, float ), \
( PFlopStratn_type, double ), \
( FlopStratn_type, double ), \
( TurnStratn_type, double ), \
( RiverStratn_type, /*float*/ CUSTOMFLOATNAME( Unsigned, uint8, 4, 22, false )   ), \
( PFlopRegret_type, float ), \
( FlopRegret_type, float ), \
( TurnRegret_type, float ), \
( RiverRegret_type, /*float*/ CUSTOMFLOATNAME( Signed, uint8, 4, 22, false )   )))

#define SEPARATE_STRATN_REGRET 1

#define DO_THREADS /*undefine this to disable threading use*/
#define DO_AGGRESSION /*undefine this to disable aggression support (could be faster)*/
const bool THREADLOOPTRACE = false; //prints out debugging
const bool WALKERDEBUG = false; //debug print

#ifdef DO_THREADS
#define IMPERFECT_RECALL 1 /* 0 = perfect recall */
#endif

//controls how large the arrays are that are allocated in the stack in walker.
//they need to accomodate the largest number of actions that we might see

// const int MAX_ACTIONS_SOLVER = 3;
const int MAX_ACTIONS_SOLVER = MAX_ACTIONS; //defined in PokerLibrary/constants.h

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
