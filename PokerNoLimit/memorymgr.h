#ifndef _memorymgr_h_
#define _memorymgr_h_

#include "../PokerLibrary/constants.h"
#include "solveparams.h" //needed for fp_types
#include <string>
using std::string;
#include <vector>
using std::vector;
#include "../utility.h"
#include "../PokerLibrary/treenolimit.h"
#include <boost/multi_array.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/tuple/tuple.hpp>
using boost::tuple;


/****************************************************

  Note on precision and such.

  Stratn's:

  Stratn's are required to never overflow. Required to never have ANY rounding errors.
  This is possible, because they are never *read* back into the algorithm. It makes sense
  that the algorithm would require them to accurately compute the average of the stratt's
  across iterations. for that reason, Stratn's should ALWAYS be doubles for preflop flop
  and turn. No reason to use more than doubles, and very marginal utility in selectively
  using less than doubles, so the best policy is always doubles preflop, flop, and turn.
  On the river, I see no loss in playoff scores by using 2 byte floats for stratn.

  Regret's:

  Regret's are approximate. This is important to understanding the algorithm. No datatype
  is large enough to keep the regrets from becoming approximate after a few iterations
  because the are *read* back into the algorithm, and the errors grow exponentially.
  For that reason, I use floats *everywhere* for the regrets, *including* the working
  type. This took me a while to realize, I used to think using a precise working type
  and less precise store type was a way to cheat the system. This is not so. On the river,
  I see no loss in playoff scores by using 2 byte floats, and marginal (1 mbb/hand) loss
  by using 1 byte floats for regret. If there is any trend with increasing bin sizes, it
  is that the 1 mbb/hand penalty becomes *smaller* as I go from 5 -> 6 -> 8 -> 10 -> 12
  bins per round.

  ******************/


  /******* OLD COMMENT:

  Stratn's (continued from above):

  On the river, the simplest policy is to use floats. River nodes get hit less often, and 
  for that reason, it is possible to use floats. Two extra actions can be taken in this
  case. (1) A 16-bit counter can be used to upgrade the stratn's to doubles when they have
  been written to 65k times. (2) stratn's can be initially allocated as a half data-type
  and then upgraded to a float on an 8-bit counter. In a test with 100M iterations of
  a 5-5-5-5 bin bot:
   A) 42.0% never written
   B) 43.9% written less than 256 times
   C) 11.5% written 256 - 65k times
   D) 2.6% written >65k times (up to a max of 163k)
  Case A would never be allocated. Case B *could* be allocated as a half. Case C should
  probably be a float. and Case D *could* be a double, but it may not matter. Also note
  for case D that the max of 163k is the determining factor of how many total iterations
  to do, and should be somewhat constant for larger bots.

  Regret's:

   A) 0.03% never read
   B) 41.6% read less than 256 times
   C) 55.7% read 256 - 65k times
   D) 2.7% read >65k times (up to a max of 163k)

   A') 0.005% never written
   B') 99.995% written at least once

   ****************/
  

#ifndef SEPARATE_STRATN_REGRET
#error "define SEPARATE_STRATN_REGRET to either 1 or 0"
#endif


//forward declaration
class CardMachine;

//used on river
//only holds stratns OR regrets, not both
template <typename T>
class DataContainerSingle
{
public:
	DataContainerSingle(int nactions, int64 ndatanodes) 
		: datastore(ndatanodes == 0 ? NULL : new T[nactions*ndatanodes]), nacts(nactions), ndata(ndatanodes)
	{ 
		if(nactions < 2) REPORT("invalid nactions");
		if(ndatanodes < 0) REPORT("invalid ndatanodes");

		for(int64 i=0; i<nactions*ndatanodes; i++)
			datastore[i] = 0;
	}

	~DataContainerSingle() 
	{ 
		if(datastore != NULL) 
			delete[] datastore; 
	}

	inline void getdata(int64 index, T* &stratn, int checknacts)
	{
		if(checknacts != nacts) REPORT("invalid acts");
		if(index < 0 || index >= ndata) REPORT("invalid index");
		stratn = &datastore[ index * nacts ];
	}

	int getnodesize() const { return 2*nacts*sizeof(T); } //in bytes

	int getnacts() const { return nacts; }

	int64 size() const { return ndata; }

private:
	T * datastore;
	int nacts;
	int64 ndata;

	//no copying
	DataContainerSingle(const DataContainerSingle& rhs);
	DataContainerSingle& operator=(const DataContainerSingle& rhs);
};

//used on pflop, flop, and turn
#if SEPARATE_STRATN_REGRET
template < typename S, typename R >
class DataContainer
{
public:
	DataContainer( int nactions, int64 ndatanodes )
		: datastratn( nactions, ndatanodes ) 
		, dataregret( nactions, ndatanodes ) 
	{ }
	inline void getstratn(int64 index, S* &stratn, int checknacts) { return datastratn.getdata( index, stratn, checknacts ); }
	inline void getregret(int64 index, R* &stratn, int checknacts) { return dataregret.getdata( index, stratn, checknacts ); }
private:
	DataContainerSingle< S > datastratn;
	DataContainerSingle< R > dataregret;
};
#else
template <typename T>
class DataContainer
{
public:
	DataContainer(int nactions, int64 ndatanodes) 
		: datastore(ndatanodes == 0 ? NULL : new T[nactions*2*ndatanodes]), nacts(nactions), ndata(ndatanodes)
	{ 
		if(nactions < 2) REPORT("invalid nactions");
		if(ndatanodes < 0) REPORT("invalid ndatanodes");

		for(int64 i=0; i<nactions*2*ndatanodes; i++)
			datastore[i] = 0;
	}

	~DataContainer() 
	{ 
		if(datastore != NULL) 
			delete[] datastore; 
	}

	inline void getstratn(int64 index, T* &stratn, int checknacts)
	{
		if(checknacts != nacts) REPORT("invalid acts");
		if(index < 0 || index >= ndata) REPORT("invalid index");
		stratn = &datastore[ index * 2*nacts ];
	}

	inline void getregret(int64 index, T* &regret, int checknacts)
	{
		if(checknacts != nacts) REPORT("invalid acts");
		if(index < 0 || index >= ndata) REPORT("invalid index");
		regret = &datastore[ index * 2*nacts + nacts ];
	}

	int getnodesize() const { return 2*nacts*sizeof(T); } //in bytes

	int getnacts() const { return nacts; }

	int64 size() const { return ndata; }

private:
	T * datastore;
	int nacts;
	int64 ndata;

	//no copying
	DataContainer(const DataContainer& rhs);
	DataContainer& operator=(const DataContainer& rhs);
};
#endif

class HugeBuffer;
class CardsiContainer;

class MemoryManager
{
public:
	MemoryManager(const BettingTree &tree, const CardMachine &cardmach); //allocates memory
	~MemoryManager(); //deallocates memory

	void readstratt( Working_type * stratt, int gr, int numa, int actioni, int cardsi );
	void readstratn( unsigned char * buffer, unsigned char & checksum, int gr, int numa, int actioni, int cardsi );
	void writestratn( int gr, int numa, int actioni, int cardsi, Working_type prob, Working_type * stratt );
	template < int P >
	void writeregret( int gr, int numa, int actioni, int cardsi, Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility );

	void savecounts(const string & filename);
	int64 save(const string &filename); //returns length of file written
	
	int riveractionimax();
	int getnumafromriveractioni(int ractioni);
	int getactionifromriveractioni(int ractioni);
	int getactmax(int r, int nacts) { return get_property(tree, maxorderindex_tag())[r][nacts]; } //for convenience

#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET
	inline void SetMasterCompactFlag() { mastercompactflag = true; }
	inline void ClearMasterCompactFlag() { mastercompactflag = false; }
	inline bool GetMasterCompactFlag() { return mastercompactflag; }
#else
	inline void SetMasterCompactFlag() { }
	inline void ClearMasterCompactFlag() { }
	inline bool GetMasterCompactFlag() { return false; }
#endif
	int64 CompactMemory(); //returns space used in hugebuffer
	int64 GetHugeBufferSize(); //returns space available in hugebuffer
private:
	//we do not support copying.
	MemoryManager(const MemoryManager& rhs);
	MemoryManager& operator=(const MemoryManager& rhs);

	//might as well keep these things around
	const CardMachine &cardmach;
	const BettingTree &tree;
	int riveractioni(int numa, int actioni);

	//each is a pointer to a large array allocated on the heap
	//one such array per gameround per type of node.
#if SEPARATE_STRATN_REGRET
	boost::multi_array< DataContainer< PFlopStratn_type, PFlopRegret_type >*, 1 > pflopdata;
	boost::multi_array< DataContainer< FlopStratn_type, FlopRegret_type >*, 1 > flopdata;
	boost::multi_array< DataContainer< TurnStratn_type, TurnRegret_type >*, 1 > turndata;
#else
	boost::multi_array< DataContainer< PFlopStratn_type >*, 1 > pflopdata;
	boost::multi_array< DataContainer< FlopStratn_type >*, 1 > flopdata;
	boost::multi_array< DataContainer< TurnStratn_type >*, 1 > turndata;
#endif
#if !DYNAMIC_ALLOC_STRATN && !DYNAMIC_ALLOC_REGRET /*both use old method*/
#if SEPARATE_STRATN_REGRET
	boost::multi_array< DataContainer< RiverStratn_type, RiverRegret_type >*, 1 > riverdata_oldmethod;
#else
	boost::multi_array< DataContainer< RiverStratn_type >*, 1 > riverdata_oldmethod;
#endif
#elif !DYNAMIC_ALLOC_STRATN /*only stratn uses old method*/
	boost::multi_array< DataContainerSingle< RiverStratn_type >*, 1 > riverdata_stratn;
#elif !DYNAMIC_ALLOC_REGRET /*only regret uses old method*/
	boost::multi_array< DataContainerSingle< RiverRegret_type >*, 1 > riverdata_regret;
#endif
#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET /*at least one uses the new method*/
	CardsiContainer* riverdata; //new method, slower, but less memory
	volatile bool mastercompactflag;
#endif
};

#endif
