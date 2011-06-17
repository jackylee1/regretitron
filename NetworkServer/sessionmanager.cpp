#include "sessionmanager.h"
#include <iomanip>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/scoped_array.hpp>
#include "sessionreconciler.h"
using namespace std;

namespace
{

	const boost::filesystem::path SESSIONDIR = "/home/scott/pokerbreedinggrounds/bin/botsessions";
	const boost::filesystem::path LOGGERFILE = "botapi.logger.log";
	const boost::filesystem::path RECLOGFILE = "botapi.reclog.log";

	string GetBotFolder( unsigned botid )
	{
		ostringstream o;
		o << "bot" << setfill( '0' ) << setw( 3 ) << botid;
		return o.str( );
	}

	string GetSessionFolder( unsigned sessionid )
	{
		ostringstream o;
		o << "session" << setfill( '0' ) << setw( 5 ) << sessionid;
		return o.str( );
	}

	void SetErrorResponse( MessageSendSessionState & response, const string & errmsg )
	{
		response.sessiontype = SESSION_NONE;
		memset( response.message, 0, sizeof( response.message ) );
		strncpy( response.message, errmsg.c_str( ), sizeof( response.message ) - 1 );
		response.message[ sizeof( response.message ) - 1 ] = 0;
	}


}

SessionManager::SessionStruct::SessionStruct( uint64 newbotid, uint64 newsessionid, const string & newxmlpath, uint64 newplayerid )
{
	const boost::filesystem::path botpath = SESSIONDIR / GetBotFolder( newbotid );
	const boost::filesystem::path sessionpath = botpath / GetSessionFolder( newsessionid );

	//may already exist but doesn't throw to try again
	boost::filesystem::create_directory( botpath );
	boost::filesystem::create_directory( sessionpath );

	//should be new files but doesn't hurt to append
	FileLogger * logptr = new FileLogger( ( sessionpath / LOGGERFILE ).string( ), true );
	FileLogger * recptr = new FileLogger( ( sessionpath / RECLOGFILE ).string( ), true );

	botapilogger = logptr;
	botapireclog = recptr;
	botseed = MTRand::gettimeclockseed( );
	bot = new BotAPI( newxmlpath, false, botseed, *logptr, *recptr );
	playerid = newplayerid;
	iserror = false;
}

SessionManager::SessionStruct::~SessionStruct( )
{
	delete bot; //must delete bot first (uses logs)
	delete botapilogger;
	delete botapireclog;
}


//return value references values in table bots
unsigned SessionManager::GetBotID( GameTypeEnum gametype, otl_connect & database )
{
	unsigned botid = -1;
	otl_stream getbotid( 3, "select botid from botmapping where gametype = :gmtp<int>", database );
	getbotid << (int)gametype << endr;
	getbotid >> botid >> endr;
	if( botid == (unsigned)-1 )
		throw Exception( "failed to get botid from botmapping" );
	return botid;
}

void SessionManager::Init( )
{
	otl_connect localdatabase( MYSQL_CONNECT_STRING );
	otl_stream getsessionid( 3, "select max( sessionid ) + 1 from sessions", localdatabase );
	unsigned nextsessionid = 0;
	getsessionid >> nextsessionid >> endr;
	if( nextsessionid == 0 )
		nextsessionid = 1;
	m_nextsessionid = nextsessionid;

	otl_stream sessionloader( 50, "select sessionid, playerid, s.botid, xmlpath from "
			"sessions s inner join bots b on s.botid = b.botid where status = 'ACTIVE'", localdatabase );

	while( ! sessionloader.eof( ) )
	{
		unsigned sessionid = 0, playerid = 0, botid = 0;
		string xmlpath;
		sessionloader >> sessionid >> playerid >> botid >> xmlpath >> endr;
		if( sessionid == 0 || playerid == 0 || botid == 0 )
			throw Exception( "0 stuff found" );

		pair< SessionMap::iterator, bool > insertionresult
			= m_map.insert( SessionMap::value_type( sessionid, SessionPtr( ) ) );

		if( ! insertionresult.second )
			throw Exception( "Error creating session (already there)" );

		insertionresult.first->second 
			= SessionPtr( new SessionStruct( botid, sessionid, xmlpath, playerid ) );
		insertionresult.first->second->database 
			= std::auto_ptr< otl_connect >( new otl_connect( MYSQL_CONNECT_STRING ) );
	}

	cout << "loaded " << sessionloader.get_rpc( ) << " sessions..." << endl;
}

uint64 SessionManager::CreateSession( uint64 playerid, const MessageCreateNewSession & request, MessageSendSessionState & response )
{
	cout << "Creating new session with playerid=" << playerid << " gametype=" << (int)request.gametype << endl;

	memset( response.message, 0, 1024 );

	try
	{
		boost::unique_lock< boost::shared_mutex > exclusivelylocked( m_maplock );

		std::auto_ptr< otl_connect > newdatabase( new otl_connect( MYSQL_CONNECT_STRING ) );

		otl_stream checkplayer( 3, "select enabled from players where playerid = :plid<unsigned> ", * newdatabase );
		checkplayer << (unsigned)playerid << endr;
		if( checkplayer.get_prefetched_row_count( ) != 1 )
			throw Exception( "Playerid not found." );
		int isenabled = 0;
		checkplayer >> isenabled >> endr;
		if( ! isenabled )
			throw Exception( "Playerid disabled." );

		const unsigned botid = GetBotID( (GameTypeEnum)request.gametype, * newdatabase );

		otl_stream makesession( 3, "insert into sessions ( sessionid, playerid, botid, status, gametype ) values ( :sessid<unsigned>, :plid<unsigned>, :btid<unsigned>, 'ACTIVE', :gtp<int> )", * newdatabase );
		makesession << (unsigned)m_nextsessionid << (unsigned)playerid << botid
			<< (int)request.gametype << endr;

		pair< SessionMap::iterator, bool > insertionresult
			= m_map.insert( SessionMap::value_type( m_nextsessionid, SessionPtr( ) ) );

		if( ! insertionresult.second )
			throw Exception( "Error creating session (already there)" );

		string xmlpath;
		otl_stream getpath( 3, "select xmlpath from bots where botid = :btid<unsigned>", * newdatabase );
		getpath << botid << endr;
		getpath >> xmlpath >> endr;

		insertionresult.first->second 
			= SessionPtr( new SessionStruct( botid, m_nextsessionid, xmlpath, playerid ) );
		insertionresult.first->second->database = newdatabase; //pass ownership over to the SessionStruct

		response.sessiontype = SESSION_OPEN;
		strcpy( response.message, "Session created." );
		return m_nextsessionid++;
	}
	catch( std::exception & e )
	{
		LogError( string( __FUNCTION__ ) + ": " + e.what ( ) );
		m_map.erase( m_nextsessionid );
		SetErrorResponse( response, e.what( ) );
		return 0;
	}
}

uint64 SessionManager::TestSession( uint64 playerid, uint64 sessionid, MessageSendSessionState & response )
{
	cout << "Testing session with playerid=" << playerid << " sessionid=" << sessionid << endl;

	memset( response.message, 0, 1024 );

	try
	{
		boost::unique_lock< boost::shared_mutex > exclusivelylocked( m_maplock );

		SessionMap::iterator findresult = m_map.find( sessionid );
		if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
		{
			SetErrorResponse( response, "No session was found." );
			return 0;
		}
		else
		{
			findresult->second->database = std::auto_ptr< otl_connect >( new otl_connect( MYSQL_CONNECT_STRING ) ); //make a new connection in case this session is a bit stale
			otl_stream updatetests( 1, "update sessions set numtests = numtests + 1 where sessionid = :sessid<unsigned>", * findresult->second->database );
			updatetests << (unsigned)sessionid << endr;

			response.sessiontype = SESSION_OPEN;
			strcpy( response.message, "Your session was restored." );
			return findresult->first;
		}
	}
	catch( std::exception & e )
	{
		LogError( string( __FUNCTION__ ) + ": " + e.what ( ) );
		SetErrorResponse( response, e.what( ) );
		return 0;
	}
}

void SessionManager::CloseSession( uint64 playerid, uint64 sessionid, tcp::socket & socket, const MessageCloseSession & request, MessageSendSessionState & response )
{
	cout << "closing session with playerid=" << playerid << " sessionid=" << sessionid << " notes=" << request.notes;

	memset( response.message, 0, 1024 );

	uint64 totalsize = 0;
	for( int i = 0; i < CLOSESESSION_MAXFILES; i++ )
	{
		cout << " file" << i << "='" << request.filename[ i ] << "' with size " << request.filelength[ i ];
		totalsize += request.filelength[ i ]; 
	}
	cout << endl;

	//read all files ahead of time so i make sure it gets done so i don't break connection
	boost::scoped_array< char > filebuffer( new char[ totalsize ] );
	boost::asio::read( socket, boost::asio::buffer( filebuffer.get( ), totalsize ) );

	try
	{
		boost::unique_lock< boost::shared_mutex > exclusivelylocked( m_maplock );

		SessionMap::iterator findresult = m_map.find( sessionid );
		if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
			throw Exception( "That session was not found." );

		//log the session

		otl_connect localdatabase( MYSQL_CONNECT_STRING ); //don't know if session connection will still work

		unsigned botid = 0;
		otl_stream getbotid( 3, "select botid from sessions where sessionid = :sessid<unsigned>", localdatabase );
		getbotid << (unsigned)sessionid << endr;
		getbotid >> botid >> endr;
		if( botid == 0 )
			throw Exception( "failed to get botid" );

		const boost::filesystem::path botpath = SESSIONDIR / GetBotFolder( botid );
		const boost::filesystem::path sessionpath = botpath / GetSessionFolder( sessionid );
		char * currentptr = filebuffer.get( );

		for( int i = 0; i < CLOSESESSION_MAXFILES; i++ )
		{
			if( request.filelength[ i ] )
			{
				const boost::filesystem::path filepath = sessionpath / request.filename[ i ];
				boost::filesystem::ofstream outfile( filepath, ios_base::out | ios_base::binary );

				outfile.write( currentptr, request.filelength[ i ] );

				if( ! outfile.good( ) )
					throw Exception( "failure to write '" + filepath.string( ) + "'" );

				currentptr += request.filelength[ i ];
			}
		}

		//now reconcile the session

		unsigned numhandhistories = 0;
		boost::filesystem::path handhistorypath;
		for( int i = 0; i < CLOSESESSION_MAXFILES; i++ )
		{
			if( request.filelength[ i ] )
			{
				boost::filesystem::path thisfile = sessionpath / request.filename[ i ];
				if( SessionReconciler::GetFileFormat( thisfile ) != SessionReconciler::FORMAT_NONE )
				{
					handhistorypath = thisfile;
					numhandhistories++;
				}
			}
		}

		if( numhandhistories == 1 )
		{
			otl_stream pathgetter( 50, "select xmlpath from sessions s inner join bots b "
					"on s.botid = b.botid where s.sessionid = :sid<unsigned>", localdatabase );
			string xmlpath;
			pathgetter << (unsigned)findresult->first << endr;
			pathgetter >> xmlpath >> endr;
			FileLogger reconlogger( sessionpath / "reconciler.logger.log", false );
			FileLogger reconreclog( sessionpath / "reconciler.reclog.log", false );
			BotAPI bot( xmlpath, false, findresult->second->botseed, reconlogger, reconreclog );
			SessionReconciler reconciler( findresult->first );
			reconciler.DoReconcile( handhistorypath, bot, false );
		}
		else
		{
			ostringstream o;
			o << "Only " << numhandhistories << " hand history files found among (";
			for( int i = 0; i < CLOSESESSION_MAXFILES; i++ )
				if( request.filelength[ i ] )
					o << "\"" << request.filename[ i ] << "\", ";
			o << ") files for session #" << findresult->first;
			LogError( o.str( ) );
		}

		//erase and remove the session from db

		m_map.erase( findresult );

		otl_stream killsession( 3, "update sessions set status = 'LOGGED', notes = :note<char[10000]> where sessionid = :sessid<unsigned>", localdatabase );
		killsession << request.notes << (unsigned)sessionid << endr;

		response.sessiontype = SESSION_NONE;
		strcpy( response.message, "Session was logged." );
	}
	catch( std::exception & e )
	{
		m_map.erase( sessionid );
		LogError( string( __FUNCTION__ ) + ": " + e.what ( ) );
		SetErrorResponse( response, e.what( ) );
	}
}

void SessionManager::CancelSession( uint64 playerid, uint64 sessionid, MessageSendSessionState & response )
{
	cout << "cancelling session with playerid=" << playerid << " sessionid=" << sessionid << endl;

	memset( response.message, 0, 1024 );

	try
	{
		boost::unique_lock< boost::shared_mutex > exclusivelylocked( m_maplock );

		SessionMap::iterator findresult = m_map.find( sessionid );
		if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
			throw Exception( "That session was not found." );

		m_map.erase( findresult );

		otl_connect localdatabase( MYSQL_CONNECT_STRING ); //don't know if session connection will still work
		otl_stream killsession( 3, "update sessions set status = 'CANCELLED' where sessionid = :sessid<unsigned>", localdatabase );
		killsession << (unsigned)sessionid << endr;

		response.sessiontype = SESSION_NONE;
		strcpy( response.message, "Session cancelled." );
	}
	catch( std::exception & e )
	{
		m_map.erase( sessionid );
		LogError( string( __FUNCTION__ ) + ": " + e.what ( ) );
		SetErrorResponse( response, e.what( ) );
	}
}

void SessionManager::UseBotNewGame( uint64 playerid, uint64 sessionid, const MessageSetNewGame & request )
{
	cout << "bot setnewgame with playreid=" << playerid << " sessionid=" << sessionid << " sblind=" << request.sblind << " bblind=" << request.bblind << " stacksize=" << request.stacksize << " myhand=" << tostr( request.myhand ) << " playernum=" << (int)request.playernum << endl;

	boost::shared_lock< boost::shared_mutex > sharinglock( m_maplock );

	SessionMap::iterator findresult = m_map.find( sessionid );
	if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
	{
		cout << "Playerid " << playerid << " attempted to use invalid session " << sessionid << endl;
		return;
	}

	try
	{
		otl_stream incrgames( 3, "update sessions set numgames = numgames + 1, lasttime = NOW() where sessionid = :sessid<unsigned>", * findresult->second->database );
		incrgames << (unsigned)sessionid << endr;

		findresult->second->iserror = false;
		findresult->second->errorstr = "";
		findresult->second->bot->setnewgame( (Player)request.playernum,
				request.myhand, request.sblind, request.bblind, request.stacksize );
	}
	catch( std::exception & e )
	{
		findresult->second->bot->GetLogger( )( string( "ERROR: " ) + e.what( ) );
		findresult->second->iserror = true;
		findresult->second->errorstr = e.what( );
	}
}

void SessionManager::UseBotNextRound( uint64 playerid, uint64 sessionid, const MessageSetNextRound & request )
{
	cout << "bot setnextround with playreid=" << playerid << " sessionid=" << sessionid << " newpot=" << request.newpot << " newboard=" << tostr( request.newboard ) << " gr=" << (int)request.gr << endl;

	boost::shared_lock< boost::shared_mutex > sharinglock( m_maplock );

	SessionMap::iterator findresult = m_map.find( sessionid );
	if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
	{
		cout << "Playerid " << playerid << " attempted to use invalid session " << sessionid << endl;
		return;
	}

	try
	{
		if( ! findresult->second->iserror )
			findresult->second->bot->setnextround( request.gr,
					request.newboard, request.newpot );
	}
	catch( std::exception & e )
	{
		findresult->second->bot->GetLogger( )( string( "ERROR: " ) + e.what( ) );
		findresult->second->iserror = true;
		findresult->second->errorstr = e.what( );
	}
}

void SessionManager::UseBotDoAction( uint64 playerid, uint64 sessionid, const MessageDoAction & request )
{
	cout << "bot doaction with playreid=" << playerid << " sessionid=" << sessionid << " amount=" << request.amount << " player=" << (int)request.player << " action=" << (int)request.action << endl;

	boost::shared_lock< boost::shared_mutex > sharinglock( m_maplock );

	SessionMap::iterator findresult = m_map.find( sessionid );
	if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
	{
		cout << "Playerid " << playerid << " attempted to use invalid session " << sessionid << endl;
		return;
	}

	try
	{
		if( ! findresult->second->iserror )
			findresult->second->bot->doaction( (Player)request.player,
					(Action)request.action, request.amount );
	}
	catch( std::exception & e )
	{
		findresult->second->bot->GetLogger( )( string( "ERROR: " ) + e.what( ) );
		findresult->second->iserror = true;
		findresult->second->errorstr = e.what( );
	}
}

void SessionManager::UseBotGetAction( uint64 playerid, uint64 sessionid, MessageSendBotAction & response )
{
	cout << "bot getaction with playerid=" << playerid << " sessionid=" << sessionid << endl;

	memset( response.message, 0, 200 );
	response.action = 0;
	response.amount = 0;
	response.level = 0;
	response.iserror = 1;

	boost::shared_lock< boost::shared_mutex > sharinglock( m_maplock );

	SessionMap::iterator findresult = m_map.find( sessionid );
	if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
	{
		strncpy( response.message, "Invalid session.", 199 );
	}
	else if( findresult->second->iserror )
	{
		strncpy( response.message, findresult->second->errorstr.c_str( ), 199 );
	}
	else
	{
		try
		{
			string actionstr;
			response.action = findresult->second->bot->getbotaction( 
					response.amount, actionstr, response.level );
			response.iserror = 0;
			strncpy( response.message, actionstr.c_str( ), 199 );
		}
		catch( std::exception & e )
		{
			findresult->second->bot->GetLogger( )( string( "ERROR: " ) + e.what( ) );
			strncpy( response.message, e.what( ), 199 );
			findresult->second->iserror = true;
			findresult->second->errorstr = e.what( );
		}
	}

	response.message[ 199 ] = 0;
}

void SessionManager::UseBotEndGame( uint64 playerid, uint64 sessionid )
{
	cout << "bot endgame with playreid=" << playerid << " sessionid=" << sessionid << endl;

	boost::shared_lock< boost::shared_mutex > sharinglock( m_maplock );

	SessionMap::iterator findresult = m_map.find( sessionid );
	if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
	{
		cout << "Playerid " << playerid << " attempted to use invalid session " << sessionid << endl;
		return;
	}

	try
	{
		if( ! findresult->second->iserror )
			findresult->second->bot->endofgame( );
	}
	catch( std::exception & e )
	{
		findresult->second->bot->GetLogger( )( string( "ERROR: " ) + e.what( ) );
		findresult->second->iserror = true;
		findresult->second->errorstr = e.what( );
	}
}

void SessionManager::UseBotEndGameCards( uint64 playerid, uint64 sessionid, const MessageEndOfGameCards & request )
{
	cout << "bot endgamewithcards with playreid=" << playerid << " sessionid=" << sessionid << " cards=" << tostr( request.opponentcards ) << endl;

	boost::shared_lock< boost::shared_mutex > sharinglock( m_maplock );

	SessionMap::iterator findresult = m_map.find( sessionid );
	if( findresult == m_map.end( ) || findresult->second->playerid != playerid )
	{
		cout << "Playerid " << playerid << " attempted to use invalid session " << sessionid << endl;
		return;
	}

	try
	{
		if( ! findresult->second->iserror )
			findresult->second->bot->endofgame( request.opponentcards );
	}
	catch( std::exception & e )
	{
		findresult->second->bot->GetLogger( )( string( "ERROR: " ) + e.what( ) );
		findresult->second->iserror = true;
		findresult->second->errorstr = e.what( );
	}
}

void SessionManager::LogError( const std::string & message )
{
	otl_connect localdatabase( MYSQL_CONNECT_STRING );
	cerr << "ERROR: " << message << endl;
	otl_stream errinserter( 50, "insert into errorlog( id, message ) values ( NULL, :msg<char[32000]> )", localdatabase );
	errinserter << message;
}

