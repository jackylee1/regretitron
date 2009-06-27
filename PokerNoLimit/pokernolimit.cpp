#include "stdafx.h"
#include "solver.h"
#include "../PokerLibrary/treenolimit.h"
#include "memorymgr.h"
#include "../utility.h"
#include "../PokerLibrary/constants.h"
#include <string>
#include <sstream>
#include <iomanip>
using namespace std;

string iterstring(int64 iter)
{
	ostringstream o;
	if(iter%MILLION==0 && iter>0)
		o << 'M' << iter/MILLION;
	else if(iter>MILLION)
		o << 'M' << fixed << setprecision(1)<< (double)iter/MILLION;
	else if(iter>=10*THOUSAND)
		o << 'k' << iter/THOUSAND;
	else
		o << iter;
	return o.str();
}

string timestring(time_t mytime)
{
	char mytimestr[32];
#ifdef _WIN32 //avoid deprecation warning
	tm mytm;
	localtime_s(&mytm, &mytime);
	strftime(mytimestr, 32, "%A, %I:%M %p", &mytm); //Thursday 5:54 PM
#else
	strftime(mytimestr, 32, "%A, %I:%M %p", localtime(&mytime)); //Thursday 5:54 PM
#endif
	return string(mytimestr);
}

void simulate(int64 iter)
{
	static double prevrate=-1;
	cout << endl << "starting on " << timestring(time(NULL)) << ":" << endl;
	cout << "doing " << iterstring(iter) << " iterations..." << endl;
	if(prevrate > 0)
		cout << "Expect to finish on " << timestring(time(NULL)+iter/prevrate) << endl;

	double timetaken = Solver::solve(iter);

	prevrate = iter / timetaken;
	cout << "...took " << timetaken << " seconds. (" << prevrate << " per second)." << endl;
}

int main(int argc, char* argv[])
{
	Solver::initsolver();
	cout << "starting work..." << endl;

	//do TESTING_AMT and then STARTING_AMT iterations

	int64 current_iter_amount=STARTING_AMT;
	simulate(TESTING_AMT);
	simulate(current_iter_amount-TESTING_AMT);

	while(1)
	{
		//save the solution

		if(Solver::gettotal() >= SAVEAFTER)
			Solver::save(SAVENAME+"-"+iterstring(Solver::gettotal()), true); //true: save strat file

		//stop if we've reached our iter goal

		if(Solver::gettotal() >= STOPAFTER) 
			break;

		//update the current iteration amount and do more iterations

		current_iter_amount *= MULTIPLIER;
		simulate(current_iter_amount);
	}

	//clean up and close down

	Solver::destructsolver();
#ifdef _WIN32
	PAUSE();
#endif
	return 0;
}

