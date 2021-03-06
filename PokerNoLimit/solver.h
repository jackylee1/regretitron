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
#include <boost/scoped_array.hpp>
#include <string>
using std::string;
#include <list>
using std::list;
#include <vector>
using std::vector;

//each is a thread. static means shared among threads
class Solver
{
public:
	Solver() {} //does nothing - all data is initialized before each loop in threadloop()
	static void initsolver( const treesettings_t &, const cardsettings_t &, 
			MTRand::uint32, unsigned, unsigned, double, double, string, int ); //called once before doing anything ever
	static void destructsolver();
	static double solve(int64 iter); // returns time taken
	static double gettotaltime() { return getdoubletime() - inittime; }
	static inline int64 gettotal() { return total; } //returns total iterations done so far
	static void save(const string &filename, bool writedata); //saves result so far, including optionally whole strategy file
	
private:
	//we do not support copying.
	Solver(const Solver& rhs);
	Solver& operator=(const Solver& rhs);

	static void* callthreadloop(void * mysolver); //static linkage function to allow use of C-style pthreads

	struct iteration_data_t
	{
		int cardsi[4][2]; // 4 gamerounds, 2 players.
		int twoprob0wins;
	};

	//only these 3 functions can access per thread data, as they are non-static
	inline bool getreadydata(list<iteration_data_t>::iterator & newdata); //used by threadloop
	bool isalldataclear();
	inline void ThreadDebug(string debugstring);
	void threadloop(); //master loop function, called first by each thread
	static tuple<Working_type, Working_type> utiltuple( int p0utility, bool awardthebonus, bool haveseenflop );
	tuple<Working_type,Working_type> walker(const int gr, const int pot, const Vertex node, const Working_type prob0, const Working_type prob1);

	//this is the per thread data
	int cardsi[4][2]; // 4 gamerounds, 2 players.
	int twoprob0wins;

	static int64 iterations, total; //number of iterations remaining and total done so far
	static double inittime; //to find total elapsed for logging purposes
	
	static CardMachine * cardmachine; //created as pointers so I do not statically initialize them (fiasco...)
	static BettingTree * tree;
	static Vertex treeroot;
	static MemoryManager * memory;
	static boost::scoped_array< Solver > solvers;
	static unsigned num_threads; //used only when DO_THREADS but defined nonetheless
	static unsigned n_lookahead; //used only when DO_THREADS but defined nonetheless
	static Working_type aggression_static_mult;
	static Working_type aggression_selective_mult;
	static string m_rakename;
	static int m_rakecap;
#ifdef DO_THREADS
	static pthread_mutex_t threaddatalock;
	static pthread_cond_t signaler;
	static list<iteration_data_t> dataqueue;
	static vector< bool > dataguardpflopp0;
	static vector< bool > dataguardpflopp1;
	static vector< bool > dataguardriver; //needed for memory contention in HugeBuffer, works for imperfect recall too
#if IMPERFECT_RECALL /*then we need to account for all data in use, not just first round*/
	static vector< bool > dataguardflopp0;
	static vector< bool > dataguardflopp1;
	static vector< bool > dataguardturnp0;
	static vector< bool > dataguardturnp1;
#endif
#endif
};

#endif

