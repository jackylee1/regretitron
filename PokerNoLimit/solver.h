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
	static fpworking_type getpfloppushprob(int pushfold_index);
	
private:
	//we do not support copying.
	Solver(const Solver& rhs);
	Solver& operator=(const Solver& rhs);

	static void doiter(int64 iter); //actually starts threads, does iterations
	static void* callthreadloop(void * mysolver); //static linkage function to allow use of C-style pthreads

	//only these 3 functions can access per thread data, as they are non-static
	void threadloop(); //master loop function, called first by each thread
	fpworking_type walker(int gr, int pot, int beti, fpworking_type prob0, fpworking_type prob1); //the algorithm
	void dummywalker(int gr, int pot, int beti); //faster version of the above when no data needs updating

	static string getstatus();
	static void bounder(int gr, int pot, int beti);

	struct iteration_data_t
	{
		int cardsi[4][2]; // 4 gamerounds, 2 players.
		int twoprob0wins;
	};
	//this is the per thread data
	int cardsi[4][2]; // 4 gamerounds, 2 players.
	int twoprob0wins;
	int actioncounters[4][MAX_NODETYPES];

	static int64 iterations, total; //number of iterations remaining and total done so far
	static double inittime; //to find total elapsed for logging purposes
	
	static CardMachine * cardmachine; //created as pointers so I do not statically initialize them (fiasco...)
	static const BettingTree * tree;
	static MemoryManager * memory;
#ifdef DO_THREADS
	static pthread_mutex_t threaddatalock;
#if USE_HISTORY
	static list<iteration_data_t> dataqueue;
	static bool datainuse[PFLOP_CARDSI_MAX*2];
	static pthread_cond_t signaler;
#else
	static pthread_mutex_t * cardsilocks[4]; //one lock per player per gameround per cardsi
#endif
#endif
	//any info I want to get out of (recursive function) bounder, has to be put here
	//other alternative is global, or adding more parameters to bounder (both suck)
	static vector< vector<int> > staticactioncounters; //need static version for bounder
	static vector< fpworking_type > accumulated_regret;
};

#endif

