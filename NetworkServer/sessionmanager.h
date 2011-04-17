#pragma once

#include <map>
#include <boost/smart_ptr.hpp>
#include <boost/thread/shared_mutex.hpp>
#include "../PokerPlayerMFC/botapi.h"
#include "servercommon.h"
#include "database.h"
using std::string;


class SessionManager
{
public:
	SessionManager( )
		: m_database( MYSQL_CONNECT_STRING )
	{ }

	void Init( );
	uint64 CreateTourneySession( uint64 playerid, const MessageCreateNewTourneySession & request, MessageSendSessionState & response );
	uint64 CreateCashSession( uint64 playerid, const MessageCreateNewCashSession & request, MessageSendSessionState & response );
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
	unsigned GetBotID( GameTypeEnum gametype );

	typedef boost::shared_ptr< BotAPI > BotPtr;
	struct SessionStruct
	{
		BotPtr bot;
		uint64 playerid;
		SessionTypeEnum type;
		bool iserror;
		string errorstr;
	};
	typedef std::map< uint64, SessionStruct > SessionMap;

	uint64 m_nextsessionid;

	SessionMap m_map;
	boost::shared_mutex m_maplock;

	otl_connect m_database;
};
