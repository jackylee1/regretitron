#include "reconcilerhelper.h"
#include "fulltiltparser.h"
#include "mergeparser.h"
#include <sstream>
#include <iomanip>
#include <boost/scoped_array.hpp>
#include <boost/regex.hpp>
using namespace std;

typedef boost::shared_ptr< istream > istream_ptr;

ReconcilerHelper::FormatEnum ReconcilerHelper::GetFileFormat( const boost::filesystem::path & filepath )
{
	boost::regex fulltiltstring( "FT.*Hold'em\\.txt" );

	// example: "Mink Room - Heads Up (34606767-1).xml"
	// example: "Boar Room - Turbo Heads Up (35009468-1).xml"
	// example: "Acropolis (Heads Up) (35783825).xml"
	boost::regex mergestring( ".* Heads Up\\)? \\(.*\\)\\.xml" ); 

	if( boost::regex_match( filepath.filename( ), fulltiltstring ) )
		return FORMAT_FULLTILT;
	else if( boost::regex_match( filepath.filename( ), mergestring ) )
		return FORMAT_MERGE;
	else
		return FORMAT_NONE;
}

void ReconcilerHelper::DoReconcile( const boost::filesystem::path & filepath, BotAPI & bot, const bool forceactions )
{
	// check the file format

	const FormatEnum format = GetFileFormat( filepath );
	if( format == FORMAT_NONE )
		throw Exception( "file \"" + filepath.string( ) + "\" is not a hand history" );

	// we will need the reconciler

	Reconciler reconciler( bot, forceactions, * this );

	// we have the format, open up the file based on which casino it is

	// formats that read it raw use this
	istream_ptr thefile;
	if( format == FORMAT_FULLTILT )
	{
		boost::filesystem::ifstream input( filepath, ios::in | ios::binary | ios::ate );
		if( ! input.good( ) )
			throw Exception( "Input file \"" + filepath.string( ) + "\" could not be opened." );

		// check for UTF16 encoding

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

			string filestr; //holds whole file
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

			thefile = istream_ptr( new istringstream( filestr ) );
		}
		else
		{
			input.close( );
			thefile = istream_ptr( new boost::filesystem::ifstream( filepath ) );
		}
	}

	// choose a parser based on the format

	if( format == FORMAT_FULLTILT )
	{
		FullTiltParser parser( reconciler );
		parser.Parse( * thefile );
	}
	else if( format == FORMAT_MERGE )
	{
		MergeParser parser( reconciler );
		parser.Parse( filepath );
	}
	else
		throw Exception( "no parser for format" );
}

