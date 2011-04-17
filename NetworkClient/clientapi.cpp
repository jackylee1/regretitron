#include "clientapi.h"
#include <boost/scoped_array.hpp>
#include <set>
using namespace std;

const string loadpathname = GetOurPath( ) + "regretinator.settings.txt";
const string sessionpathname = GetOurPath( ) + "regretinator.sessionid";


void ClientAPI::Init( )
{
	string server;
	string port;
	string playerid;
	string sessionid;

	// read settings file

	ifstream loadfile( loadpathname.c_str( ) );
	if( ! loadfile.good( ) )
		throw Exception( "settings file not found at '" + loadpathname + "'" );

	int numlines = 0;
	while( loadfile.good( ) )
	{
		string thisline;
		getline( loadfile, thisline );
		if( ! thisline.empty( ) && thisline[ 0 ] != '#' )
		{
			numlines++;

			if( numlines == 1 )
				server = thisline;
			else if( numlines == 2 )
				port = thisline;
			else if( numlines == 3 )
				playerid = thisline;
		}
	}

	if( numlines != 3 )
		throw Exception( loadpathname + " must contain exactly 3 line but contains " + tostring( numlines ) + " lines." );

	// read the sessionid file

	ifstream sessionfile( sessionpathname.c_str( ) );
	if( ! sessionfile.good( ) )
		sessionid = "0";
	else
		getline( sessionfile, sessionid );

	//create network connection

	tcp::resolver resolver( m_ioservice );
	tcp::resolver::query query( tcp::v4( ), server, port );
	tcp::resolver::iterator iterator = resolver.resolve( query );

	m_socket.connect( *iterator );

	//set the common header;

	m_commonheader.playerid = fromstr< uint64 >( playerid );
	m_commonheader.sessionid = fromstr< uint64 >( sessionid );
	m_commonheader.protocolversion = SERVER_PROTOCOL_VERSION;

	//test the session state with the server

	m_commonheader.messagetype = MESSAGE_TYPE_GETSESSIONSTATE;
	WriteStruct( m_socket, m_commonheader );

	//read the response from the server

	ReadSessionState( false );
}

void ClientAPI::CreateNewSession( const MessageCreateNewSession & request )
{
	try
	{
		if( m_commonheader.sessionid != 0 )
			throw Exception( "Trying to create new session but a session is already active" );
		
		m_commonheader.messagetype = MESSAGE_TYPE_CREATENEWSESSION;
		WriteStruct( m_socket, m_commonheader, request );

		ReadSessionState( true );
	}
	catch( std::exception & e )
	{
		MessageBox( NULL, ( string( "Error: \"" ) + e.what( ) + "\"" ).c_str( ), "Error", MB_ICONEXCLAMATION );
	}
}

void ClientAPI::setnewgame(Player playernum, CardMask myhand, double sblind, double bblind, double stacksize)
{
	try
	{
		m_commonheader.messagetype = MESSAGE_TYPE_SETNEWGAME;
		MessageSetNewGame request;
		request.bblind = bblind;
		request.myhand = myhand;
		request.playernum = playernum;
		request.sblind = sblind;
		request.stacksize = stacksize;
		WriteStruct( m_socket, m_commonheader, request );
	}
	catch( std::exception & e )
	{
		MessageBox( NULL, ( string( "Error: \"" ) + e.what( ) + "\"" ).c_str( ), "Error", MB_ICONEXCLAMATION );
	}
}

void ClientAPI::setnextround(int gr, CardMask newboard, double newpot)
{
	try
	{
		m_commonheader.messagetype = MESSAGE_TYPE_SETNEXTROUND;
		MessageSetNextRound request;
		request.gr = gr;
		request.newboard = newboard;
		request.newpot = newpot;
		WriteStruct( m_socket, m_commonheader, request );
	}
	catch( std::exception & e )
	{
		MessageBox( NULL, ( string( "Error: \"" ) + e.what( ) + "\"" ).c_str( ), "Error", MB_ICONEXCLAMATION );
	}
}

void ClientAPI::doaction(Player player, Action a, double amount)
{
	try
	{
		m_commonheader.messagetype = MESSAGE_TYPE_DOACTION;
		MessageDoAction request;
		request.player = player;
		request.action = a;
		request.amount = amount;
		WriteStruct( m_socket, m_commonheader, request );
	}
	catch( std::exception & e )
	{
		MessageBox( NULL, ( string( "Error: \"" ) + e.what( ) + "\"" ).c_str( ), "Error", MB_ICONEXCLAMATION );
	}
}

Action ClientAPI::getbotaction( double & amount, string & actionstr, int & level )
{
	m_commonheader.messagetype = MESSAGE_TYPE_GETBOTACTION;
	WriteStruct( m_socket, m_commonheader );

	ReadHeader( MESSAGE_TYPE_SENDBOTACTION );
	MessageSendBotAction response;
	ReadStruct( m_socket, response );

	if( response.iserror )
		throw Exception( response.message );
	
	amount = response.amount;
	actionstr = response.message;
	level = response.level;
	return (Action)response.action;
}

void ClientAPI::endofgame( )
{
	try
	{
		m_commonheader.messagetype = MESSAGE_TYPE_ENDOFGAME;
		WriteStruct( m_socket, m_commonheader );
	}
	catch( std::exception & e )
	{
		MessageBox( NULL, ( string( "Error: \"" ) + e.what( ) + "\"" ).c_str( ), "Error", MB_ICONEXCLAMATION );
	}
}

void ClientAPI::endofgame( CardMask opponentcards )
{
	try
	{
		m_commonheader.messagetype = MESSAGE_TYPE_ENDOFGAMECARDS;
		MessageEndOfGameCards request;
		request.opponentcards = opponentcards;
		WriteStruct( m_socket, m_commonheader, request );
	}
	catch( std::exception & e )
	{
		MessageBox( NULL, ( string( "Error: \"" ) + e.what( ) + "\"" ).c_str( ), "Error", MB_ICONEXCLAMATION );
	}
}

void ClientAPI::CloseSession( 
		const vector< string > & pathnames,
		MessageCloseSession & request )
{
	try
	{
		if( m_commonheader.sessionid == 0 )
			throw Exception( "Trying to close a session but no session is active" );

		if( pathnames.size( ) > CLOSESESSION_MAXFILES )
			throw Exception( "You tried to send " + tostr( pathnames.size( ) ) + " files but the max is " + tostr( CLOSESESSION_MAXFILES ) + " files." );

		// check the source files that will be sent over the wire, make sure they all have
		// appropriate pathnames and open them into ifstreams and make sure they all
		// have appropriate filelengths. keep the ifstreams around in an array for later
		// sending. Also, set the relevant file fields in the outgoing message struct.

		typedef boost::shared_ptr< ifstream > ifstream_ptr;
		boost::array< ifstream_ptr, CLOSESESSION_MAXFILES > inputfiles;
		int64 ntotalfilebytes = 0;
		std::set< string > uniqtiondetector;

		for( unsigned i = 0; i < CLOSESESSION_MAXFILES; i++ ) 
		{
			if( i < pathnames.size( ) )
			{
				//check filename 

				if( pathnames[ i ].empty( ) || pathnames[ i ].length( ) > CLOSESESSION_MAXFILENAMEBYTES - 1 )
					throw Exception( "Cannot send file with name of length " + tostr( pathnames[ i ].length( ) ) );

				//open the file and check size and validity

				inputfiles[ i ] = ifstream_ptr( new ifstream( pathnames[ i ].c_str( ), ios_base::binary | ios_base::ate ) );

				if( ! inputfiles[ i ]->good( ) || ! inputfiles[ i ]->is_open( ) )
					throw Exception( "file " + pathnames[ i ] + " doesn't exist." );

				const streampos filelength = inputfiles[ i ]->tellg( );
				if( filelength <= 0 || filelength >= 50000000 )
					throw Exception( "Cannot send " + pathnames[ i ] + " because its size is " + tostr( filelength ) );
				ntotalfilebytes += filelength;

				//set the fields in the message struct

				const string filename = pathnames[ i ].substr( pathnames[ i ].find_last_of( "/\\" ) + 1 );
				if( ! uniqtiondetector.insert( filename ).second )
					throw Exception( "Cannot send " + pathnames[ i ] + " because the filename is duplicated." );
				strncpy_s( request.filename[ i ], filename.c_str( ), CLOSESESSION_MAXFILENAMEBYTES );
				request.filelength[ i ] = filelength;
			}
			else
			{
				//this is an excess file field in the struct. make sure to set it to nothing.

				memset( request.filename[ i ], 0, CLOSESESSION_MAXFILENAMEBYTES );
				request.filelength[ i ] = 0;
			}
		}

		//read everything into a big memory array before sending to ensure it
		//can be sent and to not break the network connection with partial sends

		boost::scoped_array< char > filebuffer( new char[ ntotalfilebytes ] );
		char * currentfilebuffer = filebuffer.get( );

		//read the files into the filebuffer

		for( unsigned i = 0; i < inputfiles.size( ); i++ )
		{
			if( inputfiles[ i ] )
			{
				inputfiles[ i ]->seekg( 0, ios_base::beg );
				inputfiles[ i ]->read( currentfilebuffer, request.filelength[ i ] );
				//inputfiles[ i ]->peek( );
				if( inputfiles[ i ]->gcount( ) != request.filelength[ i ] )//|| ! inputfiles[ i ]->eof( ) )
					throw Exception( "error reading file for sending. filename: '" + pathnames[ i ] + "'." );
				currentfilebuffer += request.filelength[ i ];
			}
		}

		//send the header, the message body, and the file buffer over the network

		if( ntotalfilebytes > 1000000 )
			MessageBox( NULL, "Sending may take a while. Please relax.", "More than one megabyte.", MB_OK );
		m_commonheader.messagetype = MESSAGE_TYPE_CLOSESESSION;
		WriteStruct( m_socket, m_commonheader, request );
		boost::asio::write( m_socket, boost::asio::buffer( filebuffer.get( ), ntotalfilebytes ) );

		//read the response from the server

		ReadSessionState( true );
	}
	catch( std::exception & e )
	{
		MessageBox( NULL, ( string( "Error: cannot close session. \"" ) + e.what( ) + "\"" ).c_str( ), "Error", MB_ICONEXCLAMATION );
	}
}

void ClientAPI::CancelSession( )
{
	try
	{
		if( m_commonheader.sessionid == 0 )
			MessageBox( NULL, "Error: Trying to cancel a session but no session is active", "Error", MB_ICONEXCLAMATION );

		//send the header over the network

		m_commonheader.messagetype = MESSAGE_TYPE_CANCELSESSION;
		WriteStruct( m_socket, m_commonheader );

		//read the response from the server

		ReadSessionState( true );
	}
	catch( std::exception & e )
	{
		MessageBox( NULL, ( string( "Error: \"" ) + e.what( ) + "\"" ).c_str( ), "Error", MB_ICONEXCLAMATION );
	}
}

void ClientAPI::ReadHeader( MessageTypeEnum expectedtype )
{
	MessageHeader responseheader;
	ReadStruct( m_socket, responseheader );
	if( responseheader.protocolversion != SERVER_PROTOCOL_VERSION )
		throw Exception( "Server has inconsistant protocol version." );
	if( responseheader.messagetype != expectedtype )
		throw Exception( "Server sent message of type " + tostr( (int)responseheader.messagetype )
				+ " but client expected message of type " + tostr( (int)expectedtype ) );
	if( responseheader.playerid != m_commonheader.playerid )
		throw Exception( "Server sent unexpected playerid" );
	if( responseheader.sessionid != m_commonheader.sessionid )
		throw Exception( "Server sent unexpected sessionid" );
}

void ClientAPI::ReadSessionState( bool showpopup )
{
	// read the header and the message

	MessageHeader responseheader;
	ReadStruct( m_socket, responseheader );
	MessageSendSessionState response;
	ReadStruct( m_socket, response );
	
	if( responseheader.protocolversion != SERVER_PROTOCOL_VERSION )
		throw Exception( "Server has inconsistant protocol version." );
	if( responseheader.messagetype != MESSAGE_TYPE_SENDSESSIONSTATE )
		throw Exception( "Unexpected message type " + tostr( (int)responseheader.messagetype )
				+ " in ReadSessionState" );
	if( responseheader.playerid != m_commonheader.playerid )
		throw Exception( "Server sent unexpected playerid on SendSessionState" );

	if( ( responseheader.sessionid == 0 ) != ( response.sessiontype == SESSION_NONE ) )
		throw Exception( "Server sent inconsistent session response." );

	m_commonheader.sessionid = responseheader.sessionid;
	m_sessiontype = (SessionTypeEnum)response.sessiontype;

	// write the session file

	ofstream sessionfile( sessionpathname.c_str( ) );
	if( ! sessionfile.good( ) )
		throw Exception( string( "Error opening '" ) + sessionpathname.c_str( ) + "' for writing." );

	sessionfile << m_commonheader.sessionid;

	sessionfile.close( );

	// show the popup

	if( showpopup || m_commonheader.sessionid )
	{
		ostringstream o;
		if( m_commonheader.sessionid )
			o << "Session #" << m_commonheader.sessionid << " is active.\n\n";
		else
			o << "No session is active.\n\n";
		o << "Message from server: '" << response.message << "'";
		MessageBox( NULL, o.str( ).c_str( ), "Session State", MB_OK );
	}
}

