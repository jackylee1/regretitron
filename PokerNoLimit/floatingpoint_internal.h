
#if SIGNEDNESS == SIGNED
#define USE_SIGN_BIT
#define ClassName FloatCustomSigned
#elif SIGNEDNESS == UNSIGNED
#define ClassName FloatCustomUnsigned
#else
#error __FILE__ " is only to be #included by a header that defines SIGNEDNESS to SIGNED Or UNSIGNED. Try again."
#endif

template < typename INTTYPE, int EXPBITS, bool OVERFLOWISERROR >
class ClassName
{
public:
	ClassName<INTTYPE,EXPBITS,OVERFLOWISERROR>( ) { Set( 0 ); }
	ClassName<INTTYPE,EXPBITS,OVERFLOWISERROR>( const double init ) { Set( init ); }
	inline ClassName<INTTYPE,EXPBITS,OVERFLOWISERROR> & operator= ( const double source ) { Set( source ); return *this; }
	inline ClassName<INTTYPE,EXPBITS,OVERFLOWISERROR> & operator+= ( const double other ) { Set( Get() + other ); return *this; }
	inline operator double () const { return Get(); }

private:
	void Set( const double source );
	double Get( ) const;

#ifdef USE_SIGN_BIT
	static const int SIGNBITS = 1;
#else
	static const int SIGNBITS = 0;
#endif
	static const int MANTBITS = 8*sizeof(INTTYPE) - SIGNBITS - EXPBITS;
	static const int TRUNCATEDBITS = SUPER_MANTBITS - MANTBITS;
	static const INTTYPE EXPBIAS = (INTTYPE)0xFFFFFFFFU >> ( 8*sizeof(INTTYPE) - EXPBITS + 1 );
	static const INTTYPE EXPMAX = (EXPBIAS << 1) | 1;
	static const INTTYPE MANTMAX = (INTTYPE)0xFFFFFFFFU >> ( 8*sizeof(INTTYPE) - MANTBITS );
	static const uint64 TRUNCATEDMAX = ( 1ULL << TRUNCATEDBITS ); // truncated in in range [0..TRUNCATEDMAX)

#ifdef USE_SIGN_BIT
	INTTYPE negative : SIGNBITS;
#endif
	INTTYPE exponent : EXPBITS;
	INTTYPE mantissa : MANTBITS;

	//someone please read a good book on randomness and get back to me
	//This is a linear congruential generator with full period. 
	//I may benefit from some of the usually-crappy properties of this crappy generator. More research would have to be done to be sure.
	//I choose it here because it shouldn't be a problem if it gets corrupted during multithreading
	static uint64 ran;
	static uint64 GetRand() { return ran = ( 9 * ran + 1 ) % TRUNCATEDMAX; } // this will be called multithreadedly
	/*** For testing:
	   static uint64 GetRand() { return ( (uint64)rand() << 31 | rand() ) % TRUNCATEDMAX; }
	   BOOST_STATIC_ASSERT( RAND_MAX == 0x7FFFFFFF );
    ***/

	BOOST_STATIC_ASSERT( 1 <= EXPBITS && EXPBITS <= 11 ); //11 expbits in a double
	BOOST_STATIC_ASSERT( IEEE754_DOUBLE_BIAS >= EXPBIAS );
	BOOST_STATIC_ASSERT( IEEE754_DOUBLE_BIAS >> ( 11-EXPBITS ) == EXPBIAS );
	BOOST_STATIC_ASSERT( SIGNBITS + MANTBITS + EXPBITS == 8*sizeof( INTTYPE ) );
} __attribute__((packed));

template < typename INTTYPE, int EXPBITS, bool OVERFLOWISERROR > //rediculous templates
uint64 ClassName< INTTYPE, EXPBITS, OVERFLOWISERROR >::ran = ClassName< INTTYPE, EXPBITS, OVERFLOWISERROR >::TRUNCATEDMAX / 4;

// see summary table: http://steve.hollasch.net/cgindex/coding/ieeefloat.html

template < typename INTTYPE, int EXPBITS, bool OVERFLOWISERROR >
void ClassName< INTTYPE, EXPBITS, OVERFLOWISERROR >::Set( const double source )
{
	my_ieee754_double myd;
	myd.d = source;
	INTTYPE m; // will temporarily hold the mantissa

#ifdef USE_SIGN_BIT
	negative = myd.ieee.negative;
#else
	if( source < 0 ) //rather not check negative due to -0
		REPORT( "Setting a FloatCustomUnsigned to " + tostring( source ) );
#endif

	if( isnan(source) || isinf(source) )
		{ REPORT( "Detected a NAN or an INF (" + tostring(source) + ") while setting a FloatCustom" ); exit(-1); }
	else if( (int)myd.ieee.exponent - (int)IEEE754_DOUBLE_BIAS > (int)EXPMAX - (int)EXPBIAS )
	{   
		//overflow: infinity

		if( OVERFLOWISERROR ) //report an error
			{ REPORT( "Your FloatCustom has overflowed, could not deal with " + tostring( source ) ); exit(-1); }
		else //just do the best we can
		{
			exponent = EXPMAX;
			m = MANTMAX;
		}
	}
	else if( (int)myd.ieee.exponent - (int)IEEE754_DOUBLE_BIAS <= (int)0 - (int)EXPBIAS )
	{   
		//my denormalized: use the mantissa to store the number on a scale from 0 to 2^( -EXPBIAS + 1 )

		exponent = 0;
		myd.ieee.negative = 0; //since I will be using the logical value of this thing forget about the sign bit
		myd.d *= boost::math::pow< MANTBITS + EXPBIAS - 1, double >( 2.0 ); // divide by 2^( -EXPBIAS + 1 ), multiply by 2^MANTBITS
		m = (INTTYPE) ( myd.d );

		//myd.d - m is a number from 0 to 1
		//cout << myd.d << ' ' << (int64)m << ' ' << myd.d - (int64)m << ' ';
		if( 0.5 < ( myd.d - m ) * TRUNCATEDMAX - GetRand( ) )
			m++;

		//cout << "encoding denormalized... " << source << " --> " << (int64)m << endl;
	}
	else
	{   
		//regular flow, loss of precision

		exponent = (INTTYPE)( (int)myd.ieee.exponent - (int)IEEE754_DOUBLE_BIAS + (int)EXPBIAS );
		m = myd.ieee.mantissa >> ( SUPER_MANTBITS - MANTBITS );
		uint64 truncatedpart = myd.ieee.mantissa & ( 0xFFFFFFFFFFFFFFFFULL >> ( 64 - TRUNCATEDBITS ) );
		/*******

		  let's say 2 bits are being truncated.
		  TRUNCATEDMAX would be 4
		  GetRand( ) would return numbers in the range 0..3
		  truncatedpart would have the possible values 0..3

		  |  0  |  1  |  2  |  3  |  0  |  1  |  2  |  3  | <-- truncatedpart and | are divisions between double representations on the real number line
		  *************************************************** <-- real number line
             ^                       ^                      <-- position on the real number line of our FloatCustom representation

		****/

		if( GetRand( ) < truncatedpart ) 
			m++;
		//cout << (int64)truncatedpart << " of " << (int64)TRUNCATEDMAX << " (" << (double)truncatedpart/TRUNCATEDMAX << "%) mantissa=" << (int64)m << endl;
	}

	// I have stored the correct mantissa in m, but it may need to be adjusted for the case where it was rounded up

	if( m > ( 1U << MANTBITS ) )
		REPORT( "Error in double conversion " + tostring(m) + " > " + tostring( 1U << MANTBITS ) + " for source=" + tostring( source ) );
	else if( m == ( 1U << MANTBITS ) ) //rare case where we round the mantissa up and have to increase the exponent
	{
		if( exponent == EXPMAX )
		{
			if( OVERFLOWISERROR )
				{ REPORT( "Your FloatCustom has overflowed on adding a small number, could not deal with " + tostring( source ) ); exit(-1); }
			else //exponent already at largest, just use mantissa all ones
				mantissa = MANTMAX;
		}
		else //increase exponent, set mantissa to 0. Works in denormalized case too.
		{
			exponent++;
			mantissa = 0;
		}
	}
	else
		mantissa = m;
}

template < typename INTTYPE, int EXPBITS, bool OVERFLOWISERROR >
double ClassName< INTTYPE, EXPBITS, OVERFLOWISERROR >::Get( ) const
{
	my_ieee754_double myd;

	if( exponent == 0 )
	{
		// my number is denormalized, convert from denormalized
		myd.d = mantissa;
		myd.d /= boost::math::pow< MANTBITS + EXPBIAS - 1, double >( 2.0 );
		//cout << "decoding denormalized... " << (int64)mantissa << " --> " << myd.d << endl;
	}
	else
	{
		// my number is regular
		myd.ieee.exponent = (int)exponent - (int)EXPBIAS + (int)IEEE754_DOUBLE_BIAS;
		myd.ieee.mantissa = ( (uint64)mantissa ) << TRUNCATEDBITS;
	}

#ifdef USE_SIGN_BIT
	myd.ieee.negative = negative;
#else
	myd.ieee.negative = 0;
#endif
	return myd.d;
}

#undef ClassName
#ifdef USE_SIGN_BIT
#undef USE_SIGN_BIT
#endif
