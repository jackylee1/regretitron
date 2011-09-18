#pragma once

#include <boost/asio.hpp> 
#include "../utility.h"
using boost::asio::ip::tcp;

const uint8 SERVER_PROTOCOL_VERSION = 4;

template< typename STRUCT > inline void WriteStruct( tcp::socket & sock, const STRUCT & msg )
{
	boost::asio::write( sock, boost::asio::buffer( & msg, sizeof( STRUCT ) ) );
}

template< typename STRUCT1, typename STRUCT2 > inline void WriteStruct( tcp::socket & sock, const STRUCT1 & msg1, const STRUCT2 & msg2 )
{
	WriteStruct( sock, msg1 );
	WriteStruct( sock, msg2 );
}

template< typename STRUCT > inline void ReadStruct( tcp::socket & sock, STRUCT & msg )
{
	boost::asio::read( sock, boost::asio::buffer( & msg, sizeof( STRUCT ) ) );
}

enum MessageTypeEnum
{
	//client to server messages

	//session management -- all respond by sending session state
	MESSAGE_TYPE_CREATENEWSESSION = 0,
	MESSAGE_TYPE_CLOSESESSION = 2,
	MESSAGE_TYPE_CANCELSESSION = 3,
	MESSAGE_TYPE_GETSESSIONSTATE = 4,

	//botapi functions -- none have a response except GETBOTACTION
	MESSAGE_TYPE_SETNEWGAME = 10,
	MESSAGE_TYPE_SETNEXTROUND = 11,
	MESSAGE_TYPE_DOACTION = 12,
	MESSAGE_TYPE_GETBOTACTION = 13,
	MESSAGE_TYPE_ENDOFGAME = 14,
	MESSAGE_TYPE_ENDOFGAMECARDS = 15,

	//server to client messages

	MESSAGE_TYPE_SENDSESSIONSTATE = 20,
	MESSAGE_TYPE_SENDBOTACTION = 21,
	MESSAGE_TYPE_SENDERROR = 22

};

enum GameTypeEnum
{
	GAME_PLAYMONEY_LIMIT = 1,
	GAME_PLAYMONEY_NOLIMIT = 2,
	GAME_REALMONEY_LIMIT = 10,
	GAME_REALMONEY_NOLIMIT = 20
};

enum SessionTypeEnum
{
	SESSION_NONE,
	SESSION_OPEN
};


struct MessageHeader
{
	MessageHeader( ) { }

	MessageHeader( MessageTypeEnum type )
		: playerid( -1 )
		, sessionid( 0 )
		, messagetype( type )
		, protocolversion( SERVER_PROTOCOL_VERSION )
	{ }

	uint64 playerid;
	uint64 sessionid;
	uint8 messagetype;
	uint8 protocolversion; 
};
BOOST_STATIC_ASSERT( sizeof( MessageHeader ) == 24 );


//session management


struct MessageCreateNewSession
{
	uint8 gametype;
};
BOOST_STATIC_ASSERT( sizeof( MessageCreateNewSession ) == 1 );

const int CLOSESESSION_MAXFILES = 4;
const int CLOSESESSION_MAXFILENAMEBYTES = 1024;
const int CLOSESESSION_NOTESLENGTH = 10000;
struct MessageCloseSession
{
	char filename[ CLOSESESSION_MAXFILES ][ CLOSESESSION_MAXFILENAMEBYTES ];
	uint64 filelength[ CLOSESESSION_MAXFILES ];
	char notes[ CLOSESESSION_NOTESLENGTH ];
};
BOOST_STATIC_ASSERT( sizeof( MessageCloseSession ) == ( CLOSESESSION_MAXFILENAMEBYTES + 8 ) * CLOSESESSION_MAXFILES + CLOSESESSION_NOTESLENGTH );


//BotAPI functions


struct MessageSetNewGame
{
	double sblind;
	double bblind;
	double stacksize;
	CardMask myhand;
	uint8 playernum;
};
BOOST_STATIC_ASSERT( sizeof( MessageSetNewGame ) == 40 );

struct MessageSetNextRound
{
	double newpot;
	CardMask newboard;
	uint8 gr;
};
BOOST_STATIC_ASSERT( sizeof( MessageSetNextRound ) == 24 );

struct MessageDoAction
{
	double amount;
	uint8 player;
	uint8 action;
};
BOOST_STATIC_ASSERT( sizeof( MessageDoAction ) == 16 );

struct MessageEndOfGameCards
{
	CardMask opponentcards;
};
BOOST_STATIC_ASSERT( sizeof( MessageEndOfGameCards ) == 8 );


//server to client messages


struct MessageSendSessionState
{
	char message[ 1024 ];
	uint8 sessiontype;
};
BOOST_STATIC_ASSERT( sizeof( MessageSendSessionState ) == 1024 + 1 );

struct MessageSendBotAction
{
	char message[ 200 ];
	double amount;
	int32 level;
	uint8 action;
	uint8 iserror;
};
BOOST_STATIC_ASSERT( sizeof( MessageSendBotAction ) == 216 );

struct MessageSendError
{
	char message[ 2048 ];
};
BOOST_STATIC_ASSERT( sizeof( MessageSendError ) == 2048 );

