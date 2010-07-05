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

//forward declaration
class CardMachine;

//algorithm data
template <typename FStore>
class DataContainer
{
public:
	DataContainer(int nactions, int ndatanodes) 
		: datastore(ndatanodes == 0 ? NULL : new FStore[nactions*2*ndatanodes]), nacts(nactions), ndata(ndatanodes)	  
	{ 
		if(nactions < 2) REPORT("invalid nactions");
		if(ndatanodes < 0) REPORT("invalid ndatanodes");

		for(int i=0; i<nactions*2*ndatanodes; i++)
			datastore[i] = 0;
	}

	~DataContainer() 
	{ 
		if(datastore != NULL) 
			delete[] datastore; 
	}

	inline void getdata(int index, FStore* &stratn, FStore* &regret, int checknacts)
	{
		if(checknacts != nacts) REPORT("invalid acts");
		if(index < 0 || index >= ndata) REPORT("invalid index");
		stratn = &datastore[ index * 2*nacts ];
		regret = &datastore[ index * 2*nacts + nacts ];
	}

	int getnodesize() const { return 2*nacts*sizeof(FStore); } //in bytes

	int getnacts() const { return nacts; }

	int size() const { return ndata; }

private:
	FStore * datastore;
	int nacts;
	int ndata;

	//no copying
	DataContainer(const DataContainer& rhs);
	DataContainer& operator=(const DataContainer& rhs);
};

//returns the number of nodes of a certain type per full limit mini-tree
//specific to deep-stacked limit holdem. in short stacked these numbers are not constant
inline int nodespertreebyact(int nact)
{
	return nact*2; //miraculously, on the river, the number of nodes pertree of a certain number of actions is equal to twice the number of actions
}

//used for full stack limit on the river only and handles smart allocation
//stores one mini tree
//basically a big fat wrapper around a pointer
template <typename FStore>
class CompactAllocatingContainer
{
public:
	CompactAllocatingContainer() : datastore(NULL) { }
	~CompactAllocatingContainer() { if(datastore != NULL) delete[] datastore; }

	void getdata(int index, int nacts, FStore* &stratn, FStore* &regret)
	{
		if(datastore == NULL) //time to allocate and initialize
			allocandinit();

		if(index < 0 || index >= nodespertreebyact(nacts))
			REPORT("bad index into compact allocating container");
		if(nacts < 2 || nacts > 3)
			REPORT("bad getdata nacts");

		const int offset = index*2*nacts + (nacts-2)*16;
		stratn = &datastore[ offset ];
		regret = &datastore[ offset + nacts ];
		counts[ (nacts-2)*4 + index ] ++;
	}

	void allocandinit();

	bool isallocated() const { return datastore != NULL; }

	int getcount(int i) { if(i<0 || i>=10) REPORT("bad count"); return counts[i]; }

private:
	FStore * datastore;
	int counts[10];

	//no copying
	CompactAllocatingContainer(const CompactAllocatingContainer& rhs);
	CompactAllocatingContainer& operator=(const CompactAllocatingContainer& rhs);
};

template <typename FStore>
void CompactAllocatingContainer<FStore>::allocandinit()
{
	const unsigned size = (4 * 2*2) + (6 * 2*3); //4 with 2 acts, 6 with 3 acts
	datastore = new FStore[ size ];
	for(unsigned i = 0; i<size; i++)
		datastore[i] = 0;
	for(unsigned i = 0; i<10; i++)
		counts[i] = 0;
}

class MemoryManager
{
public:
	MemoryManager(const BettingTree &tree, const CardMachine &cardmach); //allocates memory
	~MemoryManager(); //deallocates memory

	template <typename FStore>
	inline void dataindexing(int gr, int nacts, int actioni, int cardsi, FStore* &stratn, FStore* &regret);
#if SAME_STORE_TYPES
	inline void dataindexingriv(int gr, int nacts, int actioni, int cardsi, FRivStore_type* &stratn, FRivStore_type* &regret);
#endif

	int64 save(const string &filename); //returns length of file written
	void readcounts(const string &filename);
	
private:
	//we do not support copying.
	MemoryManager(const MemoryManager& rhs);
	MemoryManager& operator=(const MemoryManager& rhs);

	//might as well keep these things around
	const CardMachine &cardmach;
	const BettingTree &tree;
	int getactmax(int r, int nacts) { return get_property(tree, maxorderindex_tag())[r][nacts]; } //for convenience

	//checks if allocated
	bool checkcompactdata(int actioni, int cardsi, int nacts); //doesn't have to be templated as it doesn't touch the pointers
	template <typename FStore>
	inline void getcompactdata(int actioni, int cardsi, int nacts, FStore* &stratn, FStore* &regret);

	//for my smart allocation on the river
	const bool usegrouping;
	const int groupmax;

	//each is a pointer to a large array allocated on the heap
	//one such array per gameround per type of node.
	//used as 2-dimensional arrays
	multi_array<DataContainer<FStore_type>*,2> data;
	multi_array<DataContainer<FRivStore_type>*,1> datariver; //only used if !usegrouping
	//pointer to an array of managed pointers, each of which I call a CompactAllocatingContainer
	CompactAllocatingContainer<FRivStore_type>* datarivercompact; //only used if usegrouping
};

//used to access compact data on the river. logic is used twice now, combine it is one nasty function
template <typename FStore>
inline void MemoryManager::getcompactdata(int actioni, int cardsi, int nacts, FStore* &stratn, FStore* &regret)
{
	datarivercompact[ 
		combine(actioni/nodespertreebyact(nacts), cardsi, cardmach.getcardsimax(3))  //index of the group (a minitree)
		].getdata(actioni%nodespertreebyact(nacts), nacts, stratn, regret); //handles indexing within the group
}

//this function is used during the preflop, flop, and turn only... (see below comment)
template <>
inline void MemoryManager::dataindexing<FStore_type>(int gr, int nacts, int actioni, int cardsi, FStore_type* &stratn, FStore_type* &regret)
{
	if(gr < 0 || gr > 2) REPORT("invalid use of game type");
	data[gr][nacts]->getdata(combine(cardsi, actioni, getactmax(gr,nacts)), stratn, regret, nacts);
}

//if the store types are the same -- same type used to store data on the river as on the flop/turn/preflop -- then the decision to use 
//the river version of dataindexing must be made at run time. If the store types are different, then two different versions of walker 
//are instantiated from template, and the decision *must* be made at compile time to match the store type of walker. The compile time
//decision is handled via template specialization of dataindexing, the runtime decision is handled by a different name for the function:
//dataindexingriv.
#if SAME_STORE_TYPES
inline void MemoryManager::dataindexingriv(int gr, int nacts, int actioni, int cardsi, FRivStore_type* &stratn, FRivStore_type* &regret)
#else
template <>
inline void MemoryManager::dataindexing<FRivStore_type>(int gr, int nacts, int actioni, int cardsi, FRivStore_type* &stratn, FRivStore_type* &regret)
#endif
{
	if(gr != 3) REPORT("invalid use of river type");
#if USEGROUPING
	getcompactdata<FRivStore_type>(actioni, cardsi, nacts, stratn, regret);
#else
	datariver[nacts]->getdata(combine(cardsi, actioni, getactmax(gr,nacts)), stratn, regret, nacts);
#endif
}

#endif

