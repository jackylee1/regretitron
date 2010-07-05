#ifndef __solver_h__
#define __solver_h__

#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/cardmachine.h"
#include "memorymgr.h"
#include "../PokerLibrary/constants.h"
#ifdef DO_THREADS
#include <pthread.h>
#endif
#include <string>
using std::string;
#include <list>
using std::list;

//each is a thread. static means shared among threads
class Solver
{
public:
	Solver() {} //does nothing - all data is initialized before each loop in threadloop()
	static void initsolver(); //called once before doing anything ever
	static void destructsolver();
	static double solve(int64 iter); //returns elapsed time taken for this run
	static inline int64 gettotal() { return total; } //returns total iterations done so far
	static void save(const string &filename, bool writedata); //saves result so far, including optionally whole strategy file
	
private:
	//we do not support copying.
	Solver(const Solver& rhs);
	Solver& operator=(const Solver& rhs);

	static void doiter(int64 iter); //actually starts threads, does iterations
	static void* callthreadloop(void * mysolver); //static linkage function to allow use of C-style pthreads

	//only these 3 functions can access per thread data, as they are non-static
	void threadloop(); //master loop function, called first by each thread
	template<typename FStore>
	pair<FWorking_type,FWorking_type> walker(int gr, int pot, Vertex node, FWorking_type prob0, FWorking_type prob1);

	struct iteration_data_t
	{
		int cardsi[4][2]; // 4 gamerounds, 2 players.
		int twoprob0wins;
	};
	//this is the per thread data
	int cardsi[4][2]; // 4 gamerounds, 2 players.
	int twoprob0wins;

	static int64 iterations, total; //number of iterations remaining and total done so far
	static double inittime; //to find total elapsed for logging purposes
	
	static CardMachine * cardmachine; //created as pointers so I do not statically initialize them (fiasco...)
	static BettingTree * tree;
	static Vertex treeroot;
	static MemoryManager * memory;
#ifdef DO_THREADS
	static pthread_mutex_t threaddatalock;
	static pthread_cond_t signaler;
	static list<iteration_data_t> dataqueue;
	static bool datainuse0[PFLOP_CARDSI_MAX*2];
#if !USE_HISTORY
	static bool datainuse1[FLOP_CARDSI_MAX*2];
	static bool datainuse2[TURN_CARDSI_MAX*2];
	static bool datainuse3[RIVER_CARDSI_MAX*2];
#endif
#endif
};

#endif

