#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include "servercommon.h"
#include "sessionmanager.h"
#include "../utility.h"
using namespace std;

//my global logger class, defined for each top-level project
//ConsoleLogger my_logger;
NullLogger my_logger;
LoggerClass & logger( my_logger );

NullLogger nulllog;
LoggerClass & botapireclog( nulllog );

class IntContainer
{
public:
	IntContainer( ) : m_counter( 0 ) { }
	class User
	{
	public:
		User( IntContainer & intcon ) : m_intcon( intcon )
		{
			boost::lock_guard< boost::mutex > havelock( m_intcon.m_lock );
			m_intcon.m_counter++;
		}
		~User( )
		{
			boost::lock_guard< boost::mutex > havelock( m_intcon.m_lock );
			m_intcon.m_counter--;
		}
	private:
		IntContainer & m_intcon;
	};
	int Get( ) { return m_counter; }
private:
	int m_counter;
	boost::mutex m_lock;
};
IntContainer syncedcounter;

typedef boost::shared_ptr<tcp::socket> SocketPtr;

void ConnectionThreadProc( SocketPtr sock, SessionManager & sessionmanager )
{
	IntContainer::User user( syncedcounter );
	try
	{
		while( true )
		{
			MessageHeader header;
			ReadStruct( *sock, header );

			if( header.protocolversion != SERVER_PROTOCOL_VERSION )
				throw Exception( "Client has protocol version " + tostr( (int)header.protocolversion ) + " but the server version is " + tostr( (int)SERVER_PROTOCOL_VERSION ) );

			switch( header.messagetype )
			{
				case MESSAGE_TYPE_CREATENEWTOURNEYSESSION:
				{
					MessageCreateNewTourneySession msg;
					ReadStruct( *sock, msg );

					if( header.sessionid != 0 )
						throw Exception( "Trying to create session with existing sessionid " + tostr( header.sessionid ) );

					MessageSendSessionState response;
					MessageHeader responseheader( MESSAGE_TYPE_SENDSESSIONSTATE );
					responseheader.playerid = header.playerid;
					responseheader.sessionid = sessionmanager.CreateTourneySession( header.playerid, msg, response );
					WriteStruct( *sock, responseheader, response );
					break;
				}
				case MESSAGE_TYPE_CREATENEWCASHSESSION:
				{
					MessageCreateNewCashSession msg;
					ReadStruct( *sock, msg );

					if( header.sessionid != 0 )
						throw Exception( "Trying to create session with existing sessionid " + tostr( header.sessionid ) );

					MessageSendSessionState response;
					MessageHeader responseheader( MESSAGE_TYPE_SENDSESSIONSTATE );
					responseheader.playerid = header.playerid;
					responseheader.sessionid = sessionmanager.CreateCashSession( header.playerid, msg, response );
					WriteStruct( *sock, responseheader, response );
					break;
				}
				case MESSAGE_TYPE_CLOSESESSION:
				{
					MessageCloseSession msg;
					ReadStruct( *sock, msg );

					MessageSendSessionState response;
					MessageHeader responseheader( MESSAGE_TYPE_SENDSESSIONSTATE );
					responseheader.playerid = header.playerid;
					responseheader.sessionid = 0;
					sessionmanager.CloseSession( header.playerid, header.sessionid, *sock, msg, response );
					WriteStruct( *sock, responseheader, response );
					break;
				}
				case MESSAGE_TYPE_CANCELSESSION:
				{
					MessageSendSessionState response;
					MessageHeader responseheader( MESSAGE_TYPE_SENDSESSIONSTATE );
					responseheader.playerid = header.playerid;
					responseheader.sessionid = 0;
					sessionmanager.CancelSession( header.playerid, header.sessionid, response );
					WriteStruct( *sock, responseheader, response );
					break;
				}
				case MESSAGE_TYPE_GETSESSIONSTATE:
				{
					MessageSendSessionState response;
					MessageHeader responseheader( MESSAGE_TYPE_SENDSESSIONSTATE );
					responseheader.playerid = header.playerid;
					responseheader.sessionid = sessionmanager.TestSession( header.playerid, header.sessionid, response );
					WriteStruct( *sock, responseheader, response );
					break;
				}

				case MESSAGE_TYPE_SETNEWGAME:
				{
					MessageSetNewGame msg;
					ReadStruct( *sock, msg );
					sessionmanager.UseBotNewGame( header.playerid, header.sessionid, msg );
					break;
				}
				case MESSAGE_TYPE_SETNEXTROUND:
				{
					MessageSetNextRound msg;
					ReadStruct( *sock, msg );
					sessionmanager.UseBotNextRound( header.playerid, header.sessionid, msg );
					break;
				}
				case MESSAGE_TYPE_DOACTION:
				{
					MessageDoAction msg;
					ReadStruct( *sock, msg );
					sessionmanager.UseBotDoAction( header.playerid, header.sessionid, msg );
					break;
				}
				case MESSAGE_TYPE_GETBOTACTION:
				{
					MessageSendBotAction response;
					MessageHeader responseheader( MESSAGE_TYPE_SENDBOTACTION );
					responseheader.playerid = header.playerid;
					responseheader.sessionid = header.sessionid;
					sessionmanager.UseBotGetAction( header.playerid, header.sessionid, response );
					WriteStruct( *sock, responseheader, response );
					break;
				}
				case MESSAGE_TYPE_ENDOFGAME:
				{
					sessionmanager.UseBotEndGame( header.playerid, header.sessionid );
					break;
				}
				case MESSAGE_TYPE_ENDOFGAMECARDS:
				{
					MessageEndOfGameCards msg;
					ReadStruct( *sock, msg );
					sessionmanager.UseBotEndGameCards( header.playerid, header.sessionid, msg );
					break;
				}

				default:
					throw Exception( "Received invalid message type " + tostr( (int)header.messagetype ) );
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception in thread: " << e.what() << "\n";
	}
}

void RunServer( short port )
{
	cout << "Starting BotAPI server on port " << port << " ..." << endl;

	SessionManager sessionmanager;
	sessionmanager.Init( );
	boost::asio::io_service ioservice;
	tcp::acceptor acceptor( ioservice, tcp::endpoint( tcp::v4(), port ) );
	while( true )
	{
		SocketPtr sock( new tcp::socket( ioservice ) );
		acceptor.accept( *sock );
		boost::thread mythread( boost::bind( ConnectionThreadProc, sock, boost::ref( sessionmanager ) ) );
	}
}

#ifndef _WIN32
void TryQuit( int )
{
	const int nusers = syncedcounter.Get( );
	cout << "Quitting with " << nusers << " connections..." << endl;
	exit( 0 );
}
#endif


int main( int argc, char * argv[ ] )
{
#ifndef _WIN32
	signal( SIGINT, & TryQuit ); // register signal handler for Ctrl-C
#endif
	try
	{

		if (argc != 2)
		{
			std::cerr << "Usage: " << argv[ 0 ] << " <port>" << endl;
			return -1;
		}

		RunServer( atoi( argv[ 1 ] ) );

	}
	catch( std::exception & e )
	{
		std::cerr << "Exception: " << e.what() << endl;
	}

	return 0;
}
