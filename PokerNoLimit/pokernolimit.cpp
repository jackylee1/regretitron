#include "stdafx.h"
#include "solver.h"
#include "../PokerLibrary/treenolimit.h"
#include "memorymgr.h"
#include "../utility.h"
#include "../PokerLibrary/constants.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <boost/tuple/tuple.hpp>
using namespace std;
using boost::tuple;

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

string timeival(double seconds)
{
	ostringstream o;
	o << setprecision(3);
	if(seconds < 0.5)
		o << seconds*1000 << "msec";
	else if(seconds < 90)
		o << seconds << "sec";
	else if(seconds < 7200)
		o << seconds/60 << "min";
	else
		o << seconds/3600 << "hr";
	return o.str();
}

void simulate(int64 iter)
{
	static double prevrate=-1;
	cout << endl << "At " << timestring(time(NULL)) << ": doing " << iterstring(iter) << " iterations to make total of "
	   << Solver::gettotal() + iter << " ... ";
	if(prevrate > 0) cout << "expect to finish at " << timestring(time(NULL)+iter/prevrate) << " ... ";

	double timetaken, compactthistime, compacttotal; //seconds
	int64 ncompactings, spaceused, spacetotal; //bytes, number
	boost::tie(timetaken, compactthistime, compacttotal, ncompactings, spaceused, spacetotal) = Solver::solve(iter);
	prevrate = iter / timetaken;

	cout << "Took " << timeival(timetaken) << " (" << setprecision(4) << prevrate << " per second)" << endl
		<< "Spent " << timeival(compactthistime) << " (" << 100.0*compactthistime/timetaken << "%) compacting " 
		<< ncompactings << " times, using " << space(spaceused) << " of total " << space(spacetotal) << " allocated ("
		<< 100.0*spaceused/spacetotal << "%), have spent total of " << timeival(compacttotal) << " ("
		<< 100.0*compacttotal/Solver::gettotaltime() << "%) compacting." << endl;
}

int main(int argc, char* argv[])
{
	fpu_fix_start(NULL);

	turnloggingon("cout");

	cout << "Save file: " << SAVENAME << endl;
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
			Solver::save(SAVENAME+"-"+iterstring(Solver::gettotal()), SAVESTRAT);

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

