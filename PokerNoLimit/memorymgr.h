#ifndef _memorymgr_h_
#define _memorymgr_h_

#include "../PokerLibrary/constants.h"
#include "solveparams.h" //needed for fp_types
#include <string>
using std::string;
#include <vector>
using std::vector;
#include "../utility.h"

//forward declaration
class BettingTree;
class CardMachine;

//algorithm data
template <int N> 
struct dataN_t
{
	dataN_t(); //initializes to zero
	fpstore_type regret[N];
	fpstore_type stratn[N];
#if STORE_DENOM
	fpstore_type stratd;
#endif
};

//result data, used for constructor.
template <int N> 
struct stratN_t
{
	stratN_t(const dataN_t<N> &data); //performs division
	unsigned char strat[N-1];
};


class MemoryManager
{
public:
	//tree is not saved, but used to find actionmax values.
	//cardmach is not saved, but used to get cardsi_max values.
	//allocates memory
	MemoryManager(const BettingTree &tree, const CardMachine &cardmach); 
	~MemoryManager(); //deallocates memory

	inline void dataindexing(int gr, int nacts, int actioni, int cardsi, fpstore_type* &stratn, fpstore_type* &regret
#if STORE_DENOM
		, fpstore_type* &stratd
#endif	
		);

	//cardmach is just needed for cardsi_max values
	int64 save(const string &filename, const CardMachine &cardmach); //returns length of file written
	
private:
	//we do not support copying.
	MemoryManager(const MemoryManager& rhs);
	MemoryManager& operator=(const MemoryManager& rhs);

	//each is a pointer to a large array allocated on the heap
	//one such array per gameround per type of node.
	//used as 2-dimensional arrays
	dataN_t<9> * data9[4];
	dataN_t<8> * data8[4];
	dataN_t<7> * data7[4];
	dataN_t<6> * data6[4];
	dataN_t<5> * data5[4];
	dataN_t<4> * data4[4];
	dataN_t<3> * data3[4];
	dataN_t<2> * data2[4];

	//needed for many things. computed from tree
	vector<vector<int> > actionmax;
};

#if STORE_DENOM
#define CASENINDEXING(nacts) case nacts: do{\
	stratn = data##nacts[gr][COMBINE(cardsi, actioni, actionmax[gr][nacts-2])].stratn; \
	regret = data##nacts[gr][COMBINE(cardsi, actioni, actionmax[gr][nacts-2])].regret; \
	stratd = &(data##nacts[gr][COMBINE(cardsi, actioni, actionmax[gr][nacts-2])].stratd); \
	return; \
}while(0)
#else
#define CASENINDEXING(nacts) case nacts: do{\
	stratn = data##nacts[gr][COMBINE(cardsi, actioni, actionmax[gr][nacts-2])].stratn; \
	regret = data##nacts[gr][COMBINE(cardsi, actioni, actionmax[gr][nacts-2])].regret; \
	return; \
}while(0)
#endif

inline void MemoryManager::dataindexing(int gr, int nacts, int actioni, int cardsi, fpstore_type* &stratn, fpstore_type* &regret
#if STORE_DENOM
, fpstore_type* &stratd
#endif
)
{
	switch(nacts)
	{
	default: REPORT("invalid number of actions");
	CASENINDEXING(9);
	CASENINDEXING(8);
	CASENINDEXING(7);
	CASENINDEXING(6);
	CASENINDEXING(5);
	CASENINDEXING(4);
	CASENINDEXING(3);
	CASENINDEXING(2);
	}
}

#endif

