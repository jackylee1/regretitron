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

//forward declaration
class CardMachine;

//used on pflop, flop, and turn
template <typename T>
class DataContainer
{
public:
	DataContainer(int nactions, int ndatanodes) 
		: datastore(ndatanodes == 0 ? NULL : new T[nactions*2*ndatanodes]), nacts(nactions), ndata(ndatanodes)
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

	inline void getstratn(int index, T* &stratn, int checknacts)
	{
		if(checknacts != nacts) REPORT("invalid acts");
		if(index < 0 || index >= ndata) REPORT("invalid index");
		stratn = &datastore[ index * 2*nacts ];
	}

	inline void getregret(int index, T* &regret, int checknacts)
	{
		if(checknacts != nacts) REPORT("invalid acts");
		if(index < 0 || index >= ndata) REPORT("invalid index");
		regret = &datastore[ index * 2*nacts + nacts ];
	}

	int getnodesize() const { return 2*nacts*sizeof(T); } //in bytes

	int getnacts() const { return nacts; }

	int size() const { return ndata; }

private:
	T * datastore;
	int nacts;
	int ndata;

	//no copying
	DataContainer(const DataContainer& rhs);
	DataContainer& operator=(const DataContainer& rhs);
};

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
	inline void writeregret( int gr, int numa, int actioni, int cardsi, Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility );
	void writeregret0( int gr, int numa, int actioni, int cardsi, Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility );
	void writeregret1( int gr, int numa, int actioni, int cardsi, Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility );

	void savecounts(const string & filename);
	int64 save(const string &filename); //returns length of file written
	
	int riveractionimax();
	int getnumafromriveractioni(int ractioni);
	int getactionifromriveractioni(int ractioni);
	int getactmax(int r, int nacts) { return get_property(tree, maxorderindex_tag())[r][nacts]; } //for convenience

	inline void SetMasterCompactFlag() { mastercompactflag = true; }
	inline void ClearMasterCompactFlag() { mastercompactflag = false; }
	inline bool GetMasterCompactFlag() { return mastercompactflag && MEMORY_OVER_SPEED; }
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
	boost::multi_array<DataContainer<PFlopStore_type>*,1> pflopdata;
	boost::multi_array<DataContainer<FlopStore_type>*,1> flopdata;
	boost::multi_array<DataContainer<TurnStore_type>*,1> turndata;
	boost::multi_array<DataContainer<RiverRegret_type>*,1> riverdata_oldmethod;
	CardsiContainer* riverdata; //new method, slower, but less memory
	volatile bool mastercompactflag;
};

#endif
