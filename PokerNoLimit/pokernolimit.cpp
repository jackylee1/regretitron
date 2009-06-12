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

const int64 MILLION = 1000000;
const int64 THOUSAND = 1000;

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
#if !TESTXML
	static double prevrate=-1;
	cout << endl << "starting on " << timestring(time(NULL)) << ":" << endl;
	cout << "doing " << iterstring(iter) << " iterations..." << endl;
	if(prevrate > 0)
		cout << "Expect to finish on " << timestring(time(NULL)+iter/prevrate) << endl;

	double timetaken = Solver::solve(iter);

	prevrate = iter / timetaken;
	cout << "...took " << timetaken << " seconds. (" << prevrate << " per second)." << endl;
#endif
}

int main(int argc, char* argv[])
{
	Solver::initsolver();
	cout << "starting work..." << endl;
	int64 i=25*THOUSAND;
	simulate(1000);
	simulate(i-1000);
	while(1)
	{
		simulate(i);
		Solver::save(
			string(SAVENAME)+"-"+iterstring(Solver::gettotal()), 
			(Solver::gettotal() >= SAVEAFTER));
		i*=2;
		if(i>STOPAFTER || TESTXML) break;
	}
	Solver::destructsolver();

#ifdef _WIN32 //in windows, the window will close right away
	PAUSE();
#endif
	return 0;
}

