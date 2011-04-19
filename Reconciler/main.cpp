#include "../utility.h"
#include "fulltiltparser.h"
#include "reconcilerhelper.h"
#include <sstream>
#include <iomanip>
#include <boost/scoped_array.hpp>
using namespace std;

string GetUsage( string exename )
{
	ostringstream o;
	o << "Usage: \n"
		<< "    " << exename << " INPUT RECONCILEAGENT\n"
		<< "where 'INPUT' is a Full Tilt hand history file\n"
		<< "and 'RECONCILEAGENT' is my xml input file.";
	return o.str( );
}

int main( int argc, char ** argv )
{

	try
	{
		if( argc == 3 )
		{
			// set up our reconciller with a seed

			cerr << "Enter seed for actionchooser (0 to force actions): ";
			MTRand::uint32 actionchooserseed;
			cin >> actionchooserseed;
			bool forceactions = false;
			if( actionchooserseed == 0 )
				forceactions = true;

			// this is the logger used by BotAPI and others
			FileLogger botapilogger( "reconciler.logger.log", false );

			// this is the logger used by BotAPI's reconciler log
			FileLogger botapireclog( "reconciler.reclog.log", false );

			BotAPI bot( argv[ 2 ], false, actionchooserseed, botapilogger, botapireclog );

			ReconcilerHelper rechelper;

			rechelper.DoReconcile( argv[ 1 ], bot, forceactions );

			return 0;
		}
		else
			throw Exception( GetUsage( argv[ 0 ] ) );

		return 0;
	}
	catch( std::exception & e )
	{
		cout << e.what( ) << endl;
		cerr << e.what( ) << endl;
		return -1;
	}
}

