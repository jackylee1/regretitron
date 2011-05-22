#include "mergeparser.h"
#include <iostream>
#include <string>
#include <boost/regex.hpp>
#ifndef TIXML_USE_TICPP
#define TIXML_USE_TICPP
#endif
#include "../TinyXML++/ticpp.h"

using namespace std;

using namespace boost;

namespace
{
	void stripcomma( string & str )
	{
		for( string::iterator i = str.begin( ); i != str.end( ); )
			if( *i == ',' )
				i = str.erase( i );
			else
				i++;
	}
}

#define do_regex( s, m, r ) if( ! regex_match( s, m, r ) ) \
	throw Exception( "Failed to match string '" + s + "' with regex '" + tostr( r ) + "'." );

#define myassert( s, exp ) if( ! ( exp ) ) \
	throw Exception( "Failed to '" #exp "' for string '" + s + "'." );

void MergeParser::Parse( const boost::filesystem::path & filename )
{
	using namespace ticpp;
	const string olddirectory = getdirectory();
	try
	{
		//load the xml
		Document doc( filename.string( ) );
		doc.LoadFile();

		//parse the xml
		//TODO

	}
	catch(ticpp::Exception &ex) //error parsing XML
	{
		REPORT(ex.what(),KNOWN);
	}

}

