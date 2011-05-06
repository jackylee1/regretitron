#pragma once

#include "servercommon.h"
#include <map>
#include <boost/smart_ptr.hpp>
#include <boost/thread/shared_mutex.hpp>
#include "../PokerPlayerMFC/botapi.h"
#include "database.h"
using std::string;


class SessionManager
{
public:
	SessionManager( )
	{ }

	void Init( );
	uint64 CreateSession( uint64 playerid, const MessageCreateNewSession & request, MessageSendSessionState & response );
	uint64 TestSession( uint64 playerid, uint64 sessionid, MessageSendSessionState & response );
	void CloseSession( uint64 playerid, uint64 sessionid, tcp::socket & socket, const MessageCloseSession & request, MessageSendSessionState & response );
	void CancelSession( uint64 playerid, uint64 sessionid, MessageSendSessionState & response );
	void UseBotNewGame( uint64 playerid, uint64 sessionid, const MessageSetNewGame & request );
	void UseBotNextRound( uint64 playerid, uint64 sessionid, const MessageSetNextRound & request );
	void UseBotDoAction( uint64 playerid, uint64 sessionid, const MessageDoAction & request );
	void UseBotGetAction( uint64 playerid, uint64 sessionid, MessageSendBotAction & response );
	void UseBotEndGame( uint64 playerid, uint64 sessionid );
	void UseBotEndGameCards( uint64 playerid, uint64 sessionid, const MessageEndOfGameCards & request );

private:
	unsigned GetBotID( GameTypeEnum gametype, otl_connect & database );
	void LogError( const std::string & message );

	struct SessionStruct
	{
		SessionStruct( uint64 newbotid, uint64 newsessionid, const string & newxmlpath, uint64 newplayerid );
		~SessionStruct( );

		BotAPI * bot;
		FileLogger * botapilogger;
		FileLogger * botapireclog;
		MTRand::uint32 botseed;
		uint64 playerid;
		bool iserror;
		string errorstr;
		std::auto_ptr< otl_connect > database;
	};
	typedef boost::shared_ptr< SessionStruct > SessionPtr;
	typedef std::map< uint64, SessionPtr > SessionMap;

	uint64 m_nextsessionid;

	SessionMap m_map;
	boost::shared_mutex m_maplock;
};
