#ifndef __solver_h__
#define __solver_h__

#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/cardmachine.h"
#include "memorymgr.h"
#include "../PokerLibrary/constants.h"
#ifdef DO_THREADS
#include <pthread.h>
#endif
#include <boost/tuple/tuple.hpp>
#include <string>
using std::string;
#include <list>
using std::list;
#include <bitset>
using std::bitset;

//each is a thread. static means shared among threads
class Solver
{
public:
	Solver() {} //does nothing - all data is initialized before each loop in threadloop()
	static void initsolver(); //called once before doing anything ever
	static void destructsolver();
	static boost::tuple<
		double, //time taken
		double, //seconds compacting this cycle
		double, //cumulative seconds compacting
		int64, //times compacted this cycle
		int64, //bytes used
		int64 //total bytes available
	> solve(int64 iter);
	static double gettotaltime() { return getdoubletime() - inittime; }
	static inline int64 gettotal() { return total; } //returns total iterations done so far
	static void save(const string &filename, bool writedata); //saves result so far, including optionally whole strategy file
	
private:
	//we do not support copying.
	Solver(const Solver& rhs);
	Solver& operator=(const Solver& rhs);

	static void* callthreadloop(void * mysolver); //static linkage function to allow use of C-style pthreads
	static void control_c(int sig); //static linkage to allow use of C-style signal handler

	struct iteration_data_t
	{
		int cardsi[4][2]; // 4 gamerounds, 2 players.
		int twoprob0wins;
	};

	//only these 3 functions can access per thread data, as they are non-static
	inline bool getreadydata(list<iteration_data_t>::iterator & newdata); //used by threadloop
	bool isalldataclear();
	inline void ThreadDebug(string debugstring);
	inline static void docompact();
	void threadloop(); //master loop function, called first by each thread
	tuple<Working_type,Working_type> walker(const int gr, const int pot, const Vertex node, const Working_type prob0, const Working_type prob1);

	//this is the per thread data
	int cardsi[4][2]; // 4 gamerounds, 2 players.
	int twoprob0wins;

	static int64 iterations, total; //number of iterations remaining and total done so far
	static double inittime; //to find total elapsed for logging purposes
	static double secondscompacting;
	static int64 numbercompactings;
	
	static CardMachine * cardmachine; //created as pointers so I do not statically initialize them (fiasco...)
	static BettingTree * tree;
	static Vertex treeroot;
	static MemoryManager * memory;
#ifdef DO_THREADS
	static pthread_mutex_t threaddatalock;
	static pthread_cond_t signaler;
	static list<iteration_data_t> dataqueue;
	static bitset<PFLOP_CARDSI_MAX> dataguardpflopp0;
	static bitset<PFLOP_CARDSI_MAX> dataguardpflopp1;
	static bitset<RIVER_CARDSI_MAX> dataguardriver; //needed for memory contention in HugeBuffer, works for imperfect recall too
#if IMPERFECT_RECALL /*then we need to account for all data in use, not just first round*/
	static bitset<FLOP_CARDSI_MAX> dataguardflopp0;
	static bitset<FLOP_CARDSI_MAX> dataguardflopp1;
	static bitset<TURN_CARDSI_MAX> dataguardturnp0;
	static bitset<TURN_CARDSI_MAX> dataguardturnp1;
#endif
#endif
};

#endif

