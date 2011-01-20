#ifndef __floating_point_h__
#define __floating_point_h__
#include <math.h>
#include <ieee754.h>
#include <boost/static_assert.hpp>
#include <boost/math/special_functions/pow.hpp> //http://www.boost.org/doc/libs/1_41_0/libs/math/doc/sf_and_dist/html/math_toolkit/special/powers/ct_pow.html
#ifndef DONT_INCLUDE_UTILITY
#include "../utility.h"
#endif

//adapted to use uint64 instead of 2 uint32's
union my_ieee754_double
{
	double f; //f to match the float one to use the included file polymorphically

	/* This is the IEEE 754 double-precision format.  */
	struct
	{
#if __BYTE_ORDER == __BIG_ENDIAN
#error bigendian
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# if    __FLOAT_WORD_ORDER == __BIG_ENDIAN
#error littleend bigfloat
# else
		uint64 mantissa:52;
		uint64 exponent:11;
		uint64 negative:1;
# endif
#endif
	} ieee;
};

const int SUPER_MANTBITS_DOUBLE = 52; //number of bits in the mantissa of the "SUPER" type, here double.
const int SUPER_MANTBITS_FLOAT = 23; //number of bits in the mantissa of the "SUPER" type, here float.

// I use the same header to generate the two classes. No better way to remove that bit from the representation.
// We're going for the bits!

#define SIGNED 12
#define UNSIGNED 34
#define FLOAT 56
#define DOUBLE 78
#define MASTERTYPE FLOAT
#define SIGNEDNESS SIGNED
#include "floatingpoint_internal.h"
#undef SIGNEDNESS
#define SIGNEDNESS UNSIGNED
#include "floatingpoint_internal.h"
#undef SIGNEDNESS
#undef MASTERTYPE
#define MASTERTYPE DOUBLE
#define SIGNEDNESS SIGNED
#include "floatingpoint_internal.h"
#undef SIGNEDNESS
#define SIGNEDNESS UNSIGNED
#include "floatingpoint_internal.h"
#undef SIGNEDNESS
#undef MASTERTYPE
#undef SIGNED
#undef UNSIGNED
#undef FLOAT
#undef DOUBLE

#endif
