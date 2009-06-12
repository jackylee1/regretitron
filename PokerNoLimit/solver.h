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

	static void* callthreadloop(void * mysolver); //static linkage function to allow use of C-style pthreads

	//only these 3 functions can access per thread data, as they are non-static
	void threadloop(); //master loop function, called first by each thread
	fpworking_type walker(int gr, int pot, int beti, fpworking_type prob0, fpworking_type prob1); //the algorithm
	void dummywalker(int gr, int pot, int beti); //faster version of the above when no data needs updating

	//this is the per thread data
	int cardsi[4][2]; // 4 gamerounds, 2 players.
	int twoprob0wins;
	int actioncounters[4][MAX_NODETYPES];

	static int64 iterations, total; //number of iterations remaining and total done so far
	static double inittime; //to find total elapsed for logging purposes
	
	static CardMachine * cardmachine; //created as pointers so I do not statically initialize them (fiasco...)
	static BettingTree * tree;
	static MemoryManager * memory;
#ifdef DO_THREADS
	static pthread_mutex_t * cardsilocks[4]; //one lock per player per gameround per cardsi
	static pthread_mutex_t threaddatalock;
#endif
};

#endif

