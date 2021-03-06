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
#include <boost/program_options.hpp>
using namespace std;
using boost::tuple;

const int64 THOUSAND = 1000;
const int64 MILLION = THOUSAND * THOUSAND;
const int64 BILLION = THOUSAND * MILLION;

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
	   << iterstring(Solver::gettotal() + iter) << " ... " << flush;
	if(prevrate > 0) cout << "expect to finish at " << timestring(time(NULL)+iter/prevrate) << " ... " << flush;

	double timetaken = Solver::solve(iter); // returns seconds taken
	prevrate = iter / timetaken;

	cout << "Took " << timeival(timetaken) << " (" << fixed << setprecision(1) << prevrate << " per second)" << endl;
}


int main(int argc, char* argv[])
{
	string savename;
	vector< uint64 > iterlist;
	vector< uint64 >::iterator lastiter, nextiter;

	{
		//the logic to generate this is now a global function in treenolimit.h
		treesettings_t treesettings;
		//the logic to generate this is now a static function of CardMachine
		cardsettings_t cardsettings;
		MTRand::uint32 randseed;
		unsigned nthreads;
		unsigned nlook;
		double aggstatic = 0, aggselective = 0;
		string rakename = "none";
		int rakecap = 0;

		namespace po = boost::program_options;

		po::options_description myoptions( "Solver settings" );

		myoptions.add_options( )
			( "help", "produce this help message" )

			( "file", po::value< string >( & savename )->required( ), 
			  "base of save filename" )

			( "saveat", po::value< vector< uint64 > >( & iterlist )->multitoken( )->required( ),
			  "save the strategy after this many iterations "
			  "(can be specified more than once)" )
#ifdef DO_THREADS
			( "nthread", po::value< unsigned >( & nthreads )->required( )->default_value( 1 ),
			  "number of threads to use when solving" )
			( "nlook", po::value< unsigned >( & nlook )->required( )->default_value( 1 ),
			  "amount to consider ahead for out of order solving when running "
			  "with more than one thread (may reduce contention)" )
#else
			( "nthread", po::value< unsigned >( & nthreads )->required( )->default_value( 1 ),
			  "this build not compiled with theading support, ignored" )
			( "nlook", po::value< unsigned >( & nlook )->required( )->default_value( 1 ),
			  "used with threading, this build not compiled with theading support, ignored" )
#endif

			( "limit", po::value< bool >( )->default_value( true )->required( ),
			  "limit, or no-limit, that is the question." )
			( "sblind", po::value< int >( )->required( ),
			  "small blind size (integer value)" )
			( "bblind", po::value< int >( )->required( ),
			  "big blind size (integer value)" )
			( "stacksize", po::value< int >( )->required( ),
			  "stacksize (integer value)" )

			( "pfbin", po::value< string >( )->required( ),
			  "number of preflop bins" )
			( "fbin", po::value< string >( )->required( ),
			  "number of flop bins" )
			( "tbin", po::value< string >( )->required( ),
			  "number of turn bins" )
			( "rbin", po::value< string >( )->required( ),
			  "number of river bins" )
			( "useflopalyzer", po::value< bool >( )->default_value( false )->required( ),
			  "use the cardmachine's flopalyzer or not" )
			( "usehistory", po::value< bool >( ),
			  "use the private information bins historically or not (defaults to !useboardbins)" )
			( "useboardbins", po::value< bool >( )->default_value( false )->required( ),
			  "use extra files for the board bins or not" )
			( "fboard", po::value< int >( ),
			  "number of flop board bins to use (required when useboardbins=true)" )
			( "tboard", po::value< int >( ),
			  "number of turn board bins to use (required when useboardbins=true)" )
			( "rboard", po::value< int >( ),
			  "number of river board bins to use (required when useboardbins=true)" )
			( "seed", po::value< MTRand::uint32 >( ),
			  "seed to provide to card generator (optional, uses time and clock if not set)" )

#ifdef DO_AGGRESSION
			( "aggstatic", po::value< double >( & aggstatic )->required( )->default_value( 0 ),
			  "static aggresion factor (awarded after every hand to the winner)" )
			( "aggselective", po::value< double >( & aggselective )->required( )->default_value( 0 ),
			  "selective aggresion factor (awarded after every hand to the winner when he does win by calling)" )
#endif
			( "rakename", po::value< string >( & rakename )->required( )->default_value( "none" ),
			  "a label for the rake scheme used that is saved into the XML file" )
			( "rakecap", po::value< int >( & rakecap )->required( )->default_value( 0 ),
			  "this amount is taken from every post-flop pot (usually, in limit), and is "
			  "specified in the same units as sblind, bblind, and stacksize above" )
			;

		bool askingforhelp = false;

		try
		{
			po::variables_map varmap;
			po::store( po::parse_command_line( argc, argv, myoptions ), varmap );
			askingforhelp = ( varmap.count( "help" ) || argc == 1 );
			//the notify function will throw if any of my required params above are not
			//specified. so I need to keep track of this help flap to properly do
			//"--help" and use the built-in 'required' functionality
			po::notify( varmap );

			cout << boolalpha;

			// check if strategy folder exists
			if( ! boost::filesystem::is_directory( "strategy" ) )
				throw Exception( "Directory 'strategy' not found in current working directory. solver must be run from the directory to save in." );

			// check for slash in savename
			if( savename.find_first_of( "/\\" ) != string::npos )
				throw Exception( "Directory delimiter found in savename, solver must be run from the directory to save in." );

			// general solver settings

			cout << "using settings:" << endl;

			cout << setw( 30 ) << right << "Save file basename:  " << savename << endl;

			sort( iterlist.begin( ), iterlist.end( ) );
			cout << setw( 30 ) << right << "Saving after iterations:  ";
			copy( iterlist.begin( ), iterlist.end( ), ostream_iterator< uint64 >( cout, " " ) );
			cout << endl;
			iterlist.insert( iterlist.begin( ), 0 );

			if( iterlist.size( ) < 2 )
				throw Exception( "invalid iterlist size" );
			for( lastiter = iterlist.begin( ), nextiter = ++iterlist.begin( ); 
					nextiter != iterlist.end( ); lastiter++, nextiter++ )
				if( *lastiter >= *nextiter )
					throw Exception( "invalid iterlist elements" );
			if( varmap.count( "seed" ) > 0 )
			{
				randseed = varmap[ "seed" ].as< MTRand::uint32 >( );
				cout << setw( 30 ) << right << "Using seed:  " << randseed << endl;
			}
			else
				randseed = MTRand::gettimeclockseed( );

#ifdef DO_THREADS
			cout << setw( 30 ) << right << "Threads:  " 
				<< nthreads << " (nlook = " << nlook << ")" << endl;
#else
			cout << setw( 30 ) << right << "Threads:  " 
				<< "This build not compiled with threading support, using single thread." << endl;
			nthreads = 1;
			nlook = 1;
#endif

			//tree settings

			treesettings = makelimittreesettings( 
					varmap[ "sblind" ].as< int >( ),
					varmap[ "bblind" ].as< int >( ),
					varmap[ "stacksize" ].as< int >( ) );
			cout << setw( 30 ) << right << "Blinds:  " << treesettings.sblind << " / " << treesettings.bblind << endl;
			cout << setw( 30 ) << right << "Stacksize:  " << treesettings.stacksize << " (" << fixed << setprecision( 2 ) << (double)treesettings.stacksize / treesettings.bblind << " big blinds)" << endl;

			////////////////// temporary implementation /////////////////////
			if( varmap[ "limit" ].as< bool >( ) == false )
			{
				treesettings.limit = false;
				cout << setw( 30 ) << right << "Limit poker:  " << "No! Playing no-limit!" << endl;
			}
			else
			{
				treesettings.limit = true;
				cout << setw( 30 ) << right << "Limit poker:  " << "yes, limit." << endl;
			}
			/////////////////////////////////////////////////////////////////

			//card settings

			cardsettings = CardMachine::makecardsettings( 
					varmap[ "pfbin" ].as< string >( ),
					varmap[ "fbin" ].as< string >( ),
					varmap[ "tbin" ].as< string >( ),
					varmap[ "rbin" ].as< string >( ),
					varmap[ "useboardbins" ].as< bool >( ) 
								? varmap[ "fboard" ].as< int >( ) 
								: 0,
					varmap[ "useboardbins" ].as< bool >( ) 
								? varmap[ "tboard" ].as< int >( ) 
								: 0,
					varmap[ "useboardbins" ].as< bool >( ) 
								? varmap[ "rboard" ].as< int >( ) 
								: 0,
					varmap.count( "usehistory" )
								?  varmap[ "usehistory" ].as< bool >( ) 
								: ! varmap[ "useboardbins" ].as< bool >( ),
					varmap[ "useflopalyzer" ].as< bool >( ),
					varmap[ "useboardbins" ].as< bool >( ) );
			cout << setw( 30 ) << right << "Preflop bins:  " << cardsettings.bin_max[ 0 ] << " (" << cardsettings.filename[ 0 ] << ")" << endl;
			cout << setw( 30 ) << right << "Flop bins:  " << cardsettings.bin_max[ 1 ] << " (" << cardsettings.filename[ 1 ] << ")" << endl;
			cout << setw( 30 ) << right << "Turn bins:  " << cardsettings.bin_max[ 2 ] << " (" << cardsettings.filename[ 2 ] << ")" << endl;
			cout << setw( 30 ) << right << "River bins:  " << cardsettings.bin_max[ 3 ] << " (" << cardsettings.filename[ 3 ] << ")" << endl;
			cout << setw( 30 ) << right << "Using history:  " << cardsettings.usehistory << endl;
			cout << setw( 30 ) << right << "Using flopalyzer:  " << cardsettings.useflopalyzer << endl;
			cout << setw( 30 ) << right << "Using board bins:  " << cardsettings.useboardbins << endl;
			if( cardsettings.useboardbins )
			{
				cout << setw( 30 ) << right << "Flop board bins:  " << cardsettings.board_bin_max[ 1 ] << " (" << cardsettings.boardbinsfilename[ 1 ] << ")" << endl;
				cout << setw( 30 ) << right << "Turn board bins:  " << cardsettings.board_bin_max[ 2 ] << " (" << cardsettings.boardbinsfilename[ 2 ] << ")" << endl;
				cout << setw( 30 ) << right << "River board bins:  " << cardsettings.board_bin_max[ 3 ] << " (" << cardsettings.boardbinsfilename[ 3 ] << ")" << endl;
			}

			//aggression

#ifdef DO_AGGRESSION
			cout << setw( 30 ) << right << "Static aggression:  " << aggstatic << endl;
			cout << setw( 30 ) << right << "Selective aggression:  " << aggselective << endl;
#endif

			//rake

			cout << setw( 30 ) << right << "Rake label:  " << rakename << endl;
			cout << setw( 30 ) << right << "Rake cap:  " << rakecap << endl;
			if( ( rakename == "none" ) != ( rakecap == 0 ) )
				throw Exception( "rake name doesn't match rake amount" );

			//initialize the solver, open bin files and grab all memory

			cout << endl;

			Solver::initsolver( treesettings, cardsettings, randseed, nthreads, nlook, aggstatic, aggselective, rakename, rakecap );
		}
		catch( std::exception & e )
		{
			if( askingforhelp )
			{
				cout << myoptions;
				return 0;
			}
			else
			{
				cerr << "FAILURE: " << e.what( ) << endl;
				return -1;
			}
		}
	}
	cout << endl << "starting work..." << endl << endl;

	for( lastiter = iterlist.begin( ), nextiter = ++iterlist.begin( ); 
			nextiter != iterlist.end( ); lastiter++, nextiter++ )
	{
		simulate( *nextiter - *lastiter );
		Solver::save( savename + "-" + iterstring( Solver::gettotal( ) ), true);
	}

	//clean up and close down

	Solver::destructsolver();
#ifdef _WIN32
	PAUSE();
#endif
	return 0;
}

