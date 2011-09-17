#include "stdafx.h"
#include "memorymgr.h"
#include "../PokerLibrary/cardmachine.h" //for cardsi_max
#include "../PokerLibrary/treenolimit.h"
#include "../utility.h"
#include <iostream> //for allocate function
#include <iomanip>
#include <fstream> //for save function
#include <algorithm> //for max_element
#include <boost/static_assert.hpp>
#include <cstring> //for memcpy
#include <pthread.h>
#include <boost/math/common_factor_rt.hpp>
#include <cassert>
using namespace std;

//global variables
MemoryManager* memory = NULL;

//internal, used by the functions Solver calls

//note: two versions of computestratt
//note: the non-template one overloads the templated one. the non-template one takes precedent
//      in C++ so that If I use a regret type the same as Working_type, the template won't be called
//      the template is there to make my FloatCustom's faster by not calling Get more than needed.

//this is a version of the function when we don't have to cast the regret values to 
//Working_type. In this case, I don't read them in first and just use them.
void computestratt(Working_type * stratt, Working_type const * const regret, int numa)
{
	//find total regret

	Working_type totalregret=0;

	for(int i=0; i<numa; i++)
		if (regret[i]>0) totalregret += regret[i];

	//set strategy proportional to positive regret, or 1/numa if no positive regret

	if (totalregret > 0)
		for(int i=0; i<numa; i++)
			stratt[i] = (regret[i]>0) ? regret[i] / totalregret : 0;
	else
		for(int i=0; i<numa; i++)
			stratt[i] = (Working_type)1/(Working_type)numa;
}

//this is a version of the function that gets chosen by the compiler if T is
// not Working_type. T may be my FloatCustom's, and I only want to read them once.
template <typename T> 
void computestratt(Working_type * stratt, T const * const regret_slow, int numa)
{
	Working_type regret[ MAX_ACTIONS_SOLVER ];
	for( int i = 0; i < numa; i++ )
		regret[ i ] = static_cast< Working_type >( regret_slow[ i ] ); //cast is slow

	computestratt( stratt, regret, numa ); //should call function above
}

template <typename T>
void computeprobs(unsigned char * buffer, unsigned char &checksum, T const * const stratn, int numa)
{
	checksum = 0;

	T max_value = *max_element(stratn, stratn+numa);

	if(*min_element(stratn, stratn+numa) < 0)
		REPORT("stratn has negative values...");

	if(max_value==0) //algorithm never reached this node.
		for(int a=0; a<numa; a++)
			checksum += buffer[a]=1; //assigns equal probability weighting to each
	else
	{
		for(int a=0; a<numa; a++)
		{
			//we have values in the range [0.0, max_value]
			//map them to [0.0, 256.0]
			//then cast to int. almost always all values will get rounded down,
			//But max_value will never round down. we change 256 to 255.

			int temp = int(256.0 * (Working_type)stratn[a] / max_value);
			if(temp == 256) temp = 255;
			if(temp < 0 || temp > 255)
				REPORT("failure to divide");
			checksum += buffer[a] = (unsigned char)temp;
		}
	}
}

template <typename T>
void computestratn(T * stratn, Working_type prob, Working_type * stratt, int numa)
{
	for(int a=0; a<numa; a++)
		stratn[a] += ( prob * stratt[a] );
}

template <int P, typename T>
void computeregret(T * regret, Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility, int numa )
{
	for(int a=0; a<numa; a++)
		regret[a] += ( prob * (utility[a].get<P>() - avgutility) );
}

//functions that solver calls to do its work
void MemoryManager::readstratt( Working_type * stratt, int gr, int numa, int actioni, int cardsi )
{
	if (gr==3)
	{
		RiverRegret_type * regret;
		riverdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
	}
	else if (gr == 2)
	{
		TurnRegret_type * regret;
		turndata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
	}
	else if (gr == 1)
	{
		FlopRegret_type * regret;
		flopdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
	}
	else if (gr == 0)
	{
		PFlopRegret_type * regret;
		pflopdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
	}
}


void MemoryManager::readstratn( unsigned char * buffer, unsigned char & checksum, int gr, int numa, int actioni, int cardsi )
{
	if (gr == 3)
	{
		RiverStratn_type * stratn;
		riverdata[numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computeprobs( buffer, checksum, stratn, numa );
	}
	else if (gr == 2)
	{
		TurnStratn_type * stratn;
		turndata[numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa );
		computeprobs( buffer, checksum, stratn, numa );
	}
	else if (gr == 1)
	{
		FlopStratn_type * stratn;
		flopdata[numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa );
		computeprobs( buffer, checksum, stratn, numa );
	}
	else if (gr == 0)
	{
		PFlopStratn_type * stratn;
		pflopdata[numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa );
		computeprobs( buffer, checksum, stratn, numa );
	}
}

void MemoryManager::writestratn( int gr, int numa, int actioni, int cardsi, Working_type prob, Working_type * stratt )
{
	if (gr == 3)
	{
		RiverStratn_type * stratn;
		riverdata[numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
	}
	else if (gr == 2)
	{
		TurnStratn_type * stratn;
		turndata[numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
	}
	else if (gr == 1)
	{
		FlopStratn_type * stratn;
		flopdata[numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
	}
	else if (gr == 0)
	{
		PFlopStratn_type * stratn;
		pflopdata[numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
	}
}

template < int P >
void MemoryManager::writeregret( int gr, int numa, int actioni, int cardsi, 
		Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility )
{
	if (gr == 3)
	{
		RiverRegret_type * regret;
		riverdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
	}
	else if (gr == 2)
	{
		TurnRegret_type * regret;
		turndata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
	}
	else if (gr == 1)
	{
		FlopRegret_type * regret;
		flopdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
	}
	else if (gr == 0)
	{
		PFlopRegret_type * regret;
		pflopdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
	}
}
template void MemoryManager::writeregret< 0 >( int gr, int numa, int actioni, int cardsi, 
		Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility );
template void MemoryManager::writeregret< 1 >( int gr, int numa, int actioni, int cardsi, 
		Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility );

// ---------------------------------------------------------------------------

//used for outputting info in MemoryManager ctor only
double getsize(int r)
{
	switch(r)
	{
		case 3: return sizeof( RiverStratn_type ) + sizeof( RiverRegret_type );
		case 2: return sizeof( TurnStratn_type ) + sizeof( TurnRegret_type );
		case 1: return sizeof( FlopStratn_type ) + sizeof( FlopRegret_type );
		case 0: return sizeof( PFlopStratn_type ) + sizeof( PFlopRegret_type );
		default: REPORT("bad r"); return 0;
	}
}
	
//cout how much memory will use, then allocate it
MemoryManager::MemoryManager(const BettingTree &bettingtree, const CardMachine &cardmachine)
	: cardmach( cardmachine )
	, tree( bettingtree )
	  // extent_range creates the range [start, end), thus the "+1" is needed
	, pflopdata( boost::extents[boost::multi_array_types::extent_range(2,MAX_ACTIONS+1)] )
	, flopdata( boost::extents[boost::multi_array_types::extent_range(2,MAX_ACTIONS+1)] )
	, turndata( boost::extents[boost::multi_array_types::extent_range(2,MAX_ACTIONS+1)] )
	, riverdata( boost::extents[boost::multi_array_types::extent_range(2,MAX_ACTIONS+1)] )
{
	for(int gr=0; gr<4; gr++) 
		for(int a=2; a<=MAX_ACTIONS; a++)
			if( (int64)getactmax( gr, a ) * (int64)cardmach.getcardsimax( gr ) > 0x000000007fffffffLL )
				REPORT("your indices may overflow. fix that.");

	//print out data types that are used and their sizes

	for(unsigned i=0; i<sizeof(TYPENAMES)/sizeof(TYPENAMES[0]); i++)
		cout << "floating point type " << TYPENAMES[i][0] << " is " << TYPENAMES[i][1] << " (" << space( TYPESIZES[i] ) << ")." << endl;

	//print a table of where what memory is used where

	cout << endl << "STATICALLY ALLOCATED:" << endl;

	const int c = 18;
	cout << endl
		<< setw(c) << "GAMEROUND" 
		<< setw(c) << "N ACTS"
		<< setw(c) << "N NODES" 
		<< setw(c) << "N BINS" 
		<< setw(c) << "SPACE"
		<< endl;

	//print out space requirements by game round for preflop flop and turn, track total space used

	int64 totalbytes = 0; //only statically allocated

	for(int gr=0; gr<4; gr++) 
	{
		int64 totalactsthisround = 0;
		int64 totalbytesthisround = 0;

		for(int a=2; a<=MAX_ACTIONS; a++)
		{
			if( getactmax( gr, a ) == 0 )
				continue;

			int64 thisbytes = getactmax( gr, a ) * a * getsize( gr ) * cardmach.getcardsimax( gr );
			totalactsthisround += getactmax( gr, a );
			totalbytesthisround += thisbytes;

			cout 
				<< setw( c ) << gameroundstring( gr )
				<< setw( c ) << a
				<< setw( c ) << getactmax( gr, a )
				<< setw( c ) << cardmach.getcardsimax( gr )
				<< setw( c ) << space( thisbytes )
				<< endl;
		}
		cout
			<< setw( c ) << gameroundstring( gr )
			<< setw( c ) << "TOTAL"
			<< setw( c ) << totalactsthisround
			<< setw( c ) << cardmach.getcardsimax( gr )
			<< setw( c ) << space( totalbytesthisround ) << '*'
			<< endl;

		totalbytes += totalbytesthisround;
	}

	//print space used by bin files

	cout << endl
		<< setw( c ) << "GAMEROUND"
		<< setw( c ) << "BIN MAX"
		<< setw( c ) << "BIN FILE SPACE"
		<< endl;

	for(int gr=0; gr<4; gr++)
	{
		cout 
			<< setw( c ) << gameroundstring( gr )
			<< setw( c ) << cardmach.getparams().bin_max[ gr ]
			<< setw( c ) << space( cardmach.getparams().filesize[ gr ] ) 
			<< endl;
		totalbytes += cardmach.getparams().filesize[ gr ];
	}

	//print out total bytes used

	cout << endl << "TOTAL BYTES STATICALLY ACCOUNTED FOR: " << space(totalbytes) << endl;

	//print out space possibly used by dynamic allocator

	//allocate for river

#if SEPARATE_STRATN_REGRET
	for(int n=2; n<=MAX_ACTIONS; n++) riverdata[n] = getactmax(3,n) > 0 ? new DataContainer< RiverStratn_type, RiverRegret_type >(n, getactmax(3,n)*cardmach.getcardsimax(3)) : NULL;
#else
	for(int n=2; n<=MAX_ACTIONS; n++) riverdata[n] = getactmax(3,n) > 0 ? new DataContainer< RiverStratn_type >(n, getactmax(3,n)*cardmach.getcardsimax(3)) : NULL;
#endif

	//allocate for preflop, flop, and turn.

#if SEPARATE_STRATN_REGRET
	for(int n=2; n<=MAX_ACTIONS; n++) pflopdata[n] = getactmax(0,n) > 0 ? new DataContainer< PFlopStratn_type, PFlopRegret_type >(n, getactmax(0,n)*cardmach.getcardsimax(0)) : NULL;
	for(int n=2; n<=MAX_ACTIONS; n++) flopdata[n] = getactmax(1,n) > 0 ? new DataContainer< FlopStratn_type, FlopRegret_type >(n, getactmax(1,n)*cardmach.getcardsimax(1)) : NULL;
	for(int n=2; n<=MAX_ACTIONS; n++) turndata[n] = getactmax(2,n) > 0 ? new DataContainer< TurnStratn_type, TurnRegret_type >(n, getactmax(2,n)*cardmach.getcardsimax(2)) : NULL;
#else
	for(int n=2; n<=MAX_ACTIONS; n++) pflopdata[n] = getactmax(0,n) > 0 ? new DataContainer<PFlopStratn_type>(n, getactmax(0,n)*cardmach.getcardsimax(0)) : NULL;
	for(int n=2; n<=MAX_ACTIONS; n++) flopdata[n] = getactmax(1,n) > 0 ? new DataContainer<FlopStratn_type>(n, getactmax(1,n)*cardmach.getcardsimax(1)) : NULL;
	for(int n=2; n<=MAX_ACTIONS; n++) turndata[n] = getactmax(2,n) > 0 ? new DataContainer<TurnStratn_type>(n, getactmax(2,n)*cardmach.getcardsimax(2)) : NULL;
#endif
}

MemoryManager::~MemoryManager()
{
	for(int n=2; n<=MAX_ACTIONS; n++) delete pflopdata[n];
	for(int n=2; n<=MAX_ACTIONS; n++) delete flopdata[n];
	for(int n=2; n<=MAX_ACTIONS; n++) delete turndata[n];
	for(int n=2; n<=MAX_ACTIONS; n++) delete riverdata[n];
}

int64 MemoryManager::save(const string &filename)
{
	cout << "Saving strategy data file..." << endl;

	ofstream f(("strategy/" + filename + ".strat").c_str(), ofstream::binary);

	//this starts at zero, the beginning of the file
	int64 tableoffset=0;

	//this starts at the size of the table: one 8-byte offset integer per data-array
	int64 dataoffset= 4 * MAX_NODETYPES * 8;

	for(int r=0; r<4; r++)
	{
		for(int n=MAX_ACTIONS; n>=2; n--)
		{
			// seek to beginning and write the offset
			f.seekp(tableoffset); 
			f.write((char*)&dataoffset, 8); 
			tableoffset += 8; 

			// seek to end and write the data
			f.seekp(dataoffset); 
			for(int i=0; i<getactmax(r,n)*cardmach.getcardsimax(r); i++) 
			{ 
				const int actioni = i % getactmax(r,n);
				const int cardsi = i / getactmax(r,n);
				unsigned char buffer[MAX_ACTIONS_SOLVER];
				unsigned char checksum = 0;
				readstratn( buffer, checksum, r, n, actioni, cardsi );
				f.write((char*)buffer, n); 
				f.write((char*)&checksum, 1); 
			} 
			dataoffset = f.tellp(); 
		}
	}

	f.close();

	return dataoffset;
}

