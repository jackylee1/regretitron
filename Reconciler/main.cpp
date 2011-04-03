#include "../utility.h"
#include "fulltiltparser.h"
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
			Reconciler reconciler( argv[ 2 ], actionchooserseed, forceactions );
			FullTiltParser parser( reconciler );

			// open input log file

			ifstream input( argv[ 1 ], ios::in | ios::binary | ios::ate );
			if( ! input.good( ) )
				throw Exception( "Input file " + string( argv[ 1 ] ) + " could not be opened." );
			const size_t sizebytes = input.tellg( );
			input.seekg( 0, ios::beg );
			uint8 firstbytes[ 2 ];
			input.read( (char*)firstbytes, 2 );
			if( ( firstbytes[ 0 ] == 0xff && firstbytes[ 1 ] == 0xfe )
			 || ( firstbytes[ 0 ] == 0xfe && firstbytes[ 1 ] == 0xff ) )
			{
				if( sizebytes % 2 != 0 )
					throw Exception( "found BOM but not even bytes!" );

				const size_t nchars = ( sizebytes - 2 ) / 2;

				string filestr;
				filestr.reserve( nchars );

				for( size_t i = 0; i < nchars; i++ )
				{
					char byte1, byte2;
					input.read( & byte1, 1 );
					input.read( & byte2, 1 );

					if( firstbytes[ 0 ] == 0xfe && firstbytes[ 1 ] == 0xff )
						swap( byte1, byte2 );

					if( byte2 != 0 )
						throw Exception( "non-zero second byte detected!" );

					if( byte1 == '\r' )
						continue;

					filestr += byte1;
				}

				istringstream filestream( filestr );
				parser.Parse( filestream );
			}
			else
			{
				input.close( );
				input.open( argv[ 1 ] );
				parser.Parse( input );
			}

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

