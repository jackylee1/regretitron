
#if SIGNEDNESS == SIGNED && MASTERTYPE == FLOAT
#define USE_SIGN_BIT
#define ClassName FloatCustomSigned
#elif SIGNEDNESS == UNSIGNED && MASTERTYPE == FLOAT
#define ClassName FloatCustomUnsigned
#elif SIGNEDNESS == SIGNED && MASTERTYPE == DOUBLE
#define USE_SIGN_BIT
#define ClassName DoubleCustomSigned
#elif SIGNEDNESS == UNSIGNED && MASTERTYPE == DOUBLE
#define ClassName DoubleCustomUnsigned
#else
#error __FILE__ " is only to be #included by a header that defines SIGNEDNESS to SIGNED or UNSIGNED and MASTERTYPE to FLOAT or DOUBLE. Try again."
#endif

#if MASTERTYPE == FLOAT
#define SUPER_MANTBITS SUPER_MANTBITS_FLOAT
#define MY_IEEE754 ieee754_float
#define MASTERTYPE_BIAS IEEE754_FLOAT_BIAS
#define MASTER_T float
#elif MASTERTYPE == DOUBLE
#define SUPER_MANTBITS SUPER_MANTBITS_DOUBLE
#define MY_IEEE754 my_ieee754_double
#define MASTERTYPE_BIAS IEEE754_DOUBLE_BIAS
#define MASTER_T double
#else
#error
#endif

template < typename INTTYPE, int EXPBITS, int MAXEXPNEEDED, bool OVERFLOWISERROR >
class ClassName
{
public:
	ClassName<INTTYPE,EXPBITS,MAXEXPNEEDED,OVERFLOWISERROR>( ) { Set( 0 ); }
	ClassName<INTTYPE,EXPBITS,MAXEXPNEEDED,OVERFLOWISERROR>( const MASTER_T init ) { Set( init ); }
	inline ClassName<INTTYPE,EXPBITS,MAXEXPNEEDED,OVERFLOWISERROR> & operator= ( const MASTER_T source ) { Set( source ); return *this; }
	inline ClassName<INTTYPE,EXPBITS,MAXEXPNEEDED,OVERFLOWISERROR> & operator+= ( const MASTER_T other ) { Set( Get() + other ); return *this; }
	inline operator MASTER_T () const { return Get(); }

private:
	void Set( const MASTER_T source );
	MASTER_T Get( ) const;

#ifdef USE_SIGN_BIT
	static const int SIGNBITS = 1;
#else
	static const int SIGNBITS = 0;
#endif
	static const int MANTBITS = 8*sizeof(INTTYPE) - SIGNBITS - EXPBITS;
	static const int TRUNCATEDBITS = SUPER_MANTBITS - MANTBITS;
	static const INTTYPE EXPMAX = (INTTYPE)0xFFFFFFFFU >> ( 8*sizeof(INTTYPE) - EXPBITS ); //all 1's
	static const int EXPBIAS = (int)EXPMAX - MAXEXPNEEDED;
	static const INTTYPE MANTMAX = (INTTYPE)0xFFFFFFFFU >> ( 8*sizeof(INTTYPE) - MANTBITS );
	static const uint64 TRUNCATEDMAX = ( 1ULL << TRUNCATEDBITS ); // truncated in in range [0..TRUNCATEDMAX)
	static const MASTER_T DENORMALIZED_MULTIPLIER;
	static MASTER_T calc_denorm_multiplier( );

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

	BOOST_STATIC_ASSERT( 1 <= EXPBITS && EXPBITS <= 8 * sizeof( MASTER_T ) - SUPER_MANTBITS - 1 ); //8 * sizeof( MASTER_T ) - SUPER_MANTBITS - 1 expbits in a MASTER_T
	BOOST_STATIC_ASSERT( ( ( MASTERTYPE_BIAS << 1 ) | 1 )/*expmax*/ - MASTERTYPE_BIAS >= (int)EXPMAX - EXPBIAS ); //make sure mastertype has at least the range
	BOOST_STATIC_ASSERT( 0 - (int)MASTERTYPE_BIAS <= 0 - EXPBIAS ); //make sure mastertype has at least the range
	BOOST_STATIC_ASSERT( SIGNBITS + MANTBITS + EXPBITS == 8*sizeof( INTTYPE ) );
} __attribute__((packed));

//static non-integral members

template < typename INTTYPE, int EXPBITS, int MAXEXPNEEDED, bool OVERFLOWISERROR > //rediculous templates
uint64 ClassName< INTTYPE, EXPBITS, MAXEXPNEEDED, OVERFLOWISERROR >::ran = ClassName< INTTYPE, EXPBITS, MAXEXPNEEDED, OVERFLOWISERROR >::TRUNCATEDMAX / 4;
template < typename INTTYPE, int EXPBITS, int MAXEXPNEEDED, bool OVERFLOWISERROR > // divide by 2^( -EXPBIAS + 1 ), multiply by 2^MANTBITS
const MASTER_T ClassName< INTTYPE, EXPBITS, MAXEXPNEEDED, OVERFLOWISERROR >::DENORMALIZED_MULTIPLIER = calc_denorm_multiplier( );
template < typename INTTYPE, int EXPBITS, int MAXEXPNEEDED, bool OVERFLOWISERROR >
MASTER_T ClassName< INTTYPE, EXPBITS, MAXEXPNEEDED, OVERFLOWISERROR >::calc_denorm_multiplier( )
{
	MY_IEEE754 myf;
	myf.ieee.negative = 0;
	myf.ieee.mantissa = 0;
	myf.ieee.exponent = MASTERTYPE_BIAS + MANTBITS + EXPBIAS - 1;
#if MASTERTYPE == DOUBLE
    if( myf.f != boost::math::pow< MANTBITS + EXPBIAS - 1, double >( 2.0 ) )
#elif MASTERTYPE == FLOAT
    if( myf.f != boost::math::pow< MANTBITS + EXPBIAS - 1, float >( 2.0F ) )
#endif
		REPORT( "Failure to initialize denormalized multiplier" );
	return myf.f;
}

// conversion functions
//  see summary table: http://steve.hollasch.net/cgindex/coding/ieeefloat.html

template < typename INTTYPE, int EXPBITS, int MAXEXPNEEDED, bool OVERFLOWISERROR >
void ClassName< INTTYPE, EXPBITS, MAXEXPNEEDED, OVERFLOWISERROR >::Set( const MASTER_T source )
{
	MY_IEEE754 myf;
	myf.f = source;
	INTTYPE m; // will temporarily hold the mantissa

#ifdef USE_SIGN_BIT
	negative = myf.ieee.negative;
#else
	if( source < 0 ) //rather not check negative due to -0
		REPORT( "Setting a FloatCustomUnsigned to " + tostring( source ) );
#endif

	if( isnan(source) || isinf(source) )
		{ REPORT( "Detected a NAN or an INF (" + tostring(source) + ") while setting a FloatCustom" ); exit(-1); }
	else if( (int)myf.ieee.exponent - (int)MASTERTYPE_BIAS > (int)EXPMAX - EXPBIAS )
	{   
		//overflow: infinity

		if( OVERFLOWISERROR 
#ifdef USE_SIGN_BIT
				&& ! negative //see other comment
#endif
				) //report an error
			{ REPORT( "Your FloatCustom has overflowed, could not deal with " + tostring( source ) ); exit(-1); }
		else //just do the best we can
		{
			exponent = EXPMAX;
			m = MANTMAX;
		}
	}
	else if( (int)myf.ieee.exponent - (int)MASTERTYPE_BIAS <= (int)0 - EXPBIAS )
	{   
		//my denormalized: use the mantissa to store the number on a scale from 0 to 2^( -EXPBIAS + 1 )

		exponent = 0;
		myf.ieee.negative = 0; //since I will be using the logical value of this thing forget about the sign bit
		myf.f *= DENORMALIZED_MULTIPLIER; // divide by 2^( -EXPBIAS + 1 ), multiply by 2^MANTBITS
		m = (INTTYPE) ( myf.f );

		//myf.f - m is a number from 0 to 1
		//cout << myf.f << ' ' << (int64)m << ' ' << myf.f - (int64)m << ' ';
		if( 0.5 < ( myf.f - m ) * TRUNCATEDMAX - GetRand( ) )
			m++;

		//cout << "encoding denormalized... " << source << " --> " << (int64)m << endl;
	}
	else
	{   
		//regular flow, loss of precision

		exponent = (INTTYPE)( (int)myf.ieee.exponent - (int)MASTERTYPE_BIAS + EXPBIAS );
		m = myf.ieee.mantissa >> ( SUPER_MANTBITS - MANTBITS );
		uint64 truncatedpart = myf.ieee.mantissa & ( 0xFFFFFFFFFFFFFFFFULL >> ( 64 - TRUNCATEDBITS ) );
		/*******

		  let's say 2 bits are being truncated.
		  TRUNCATEDMAX would be 4
		  GetRand( ) would return numbers in the range 0..3
		  truncatedpart would have the possible values 0..3

		  |  0  |  1  |  2  |  3  |  0  |  1  |  2  |  3  | <-- truncatedpart and | are divisions between MASTER_T representations on the real number line
		  *************************************************** <-- real number line
             ^                       ^                      <-- position on the real number line of our FloatCustom representation

		****/

		if( GetRand( ) < truncatedpart ) 
			m++;
		//cout << (int64)truncatedpart << " of " << (int64)TRUNCATEDMAX << " (" << (double)truncatedpart/TRUNCATEDMAX << "%) mantissa=" << (int64)m << endl;
	}

	// I have stored the correct mantissa in m, but it may need to be adjusted for the case where it was rounded up

	if( m > ( 1U << MANTBITS ) )
		REPORT( "Error in MASTER_T conversion " + tostring(m) + " > " + tostring( 1U << MANTBITS ) + " for source=" + tostring( source ) );
	else if( m == ( 1U << MANTBITS ) ) //rare case where we round the mantissa up and have to increase the exponent
	{
		if( exponent == EXPMAX )
		{
			if( OVERFLOWISERROR
#ifdef USE_SIGN_BIT
					// I do not want to throw an error on small subtractions from negative numbers. Signed numbers only are used for
					// regrets, and the pattern for an action that is always bad is for it to just become more and more negative. 
					// If I cap the negativeness, this does nothing because only positive regrets are used. The only danger is if 
					// it later becomes positive due to a turn in the cards. I see this as unlikely, for decent number of EXPBITS
			    && ! negative 
#endif
			  )
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

template < typename INTTYPE, int EXPBITS, int MAXEXPNEEDED, bool OVERFLOWISERROR >
MASTER_T ClassName< INTTYPE, EXPBITS, MAXEXPNEEDED, OVERFLOWISERROR >::Get( ) const
{
	MY_IEEE754 myf;

	if( exponent == 0 )
	{
		// my number is denormalized, convert from denormalized
		myf.f = mantissa;
		myf.f /= DENORMALIZED_MULTIPLIER;
		//cout << "decoding denormalized... " << (int64)mantissa << " --> " << myf.f << endl;
	}
	else
	{
		// my number is regular
		myf.ieee.exponent = (int)exponent - EXPBIAS + (int)MASTERTYPE_BIAS;
		myf.ieee.mantissa = ( (uint64)mantissa ) << TRUNCATEDBITS;
	}

#ifdef USE_SIGN_BIT
	myf.ieee.negative = negative;
#else
	myf.ieee.negative = 0;
#endif
	return myf.f;
}

#undef ClassName
#undef SUPER_MANTBITS
#undef MY_IEEE754
#undef MASTERTYPE_BIAS
#undef MASTER_T
#ifdef USE_SIGN_BIT
#undef USE_SIGN_BIT
#endif
