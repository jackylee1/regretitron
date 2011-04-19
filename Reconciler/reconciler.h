#pragma once

#include "../utility.h"
#include "../PokerLibrary/constants.h"
#include "../MersenneTwister.h"
#include "../PokerPlayerMFC/botapi.h"
#include <vector>
#include <string>
#include <poker_defs.h>
#include "MyCardMask.h"
#include "TaskThread.h"
#include "reconcilerhelper.h"
using std::vector;
using std::string;

class Reconciler
{
public:
	Reconciler( BotAPI & bot, bool forceactions, ReconcilerHelper & errhandler )
		: m_errhandler( errhandler )
		, m_forceactions( forceactions )
		, m_haveacted( false )
		, m_iserror( false )
		, m_bot( bot )
	{ }
	void setnewgame(uint64 gamenumber, Player playernum, MyCardMask cards, double sblind, double bblind, double stacksize);
	void setnextround(int gr, MyCardMask newboard, double newpot);
	void doaction(Player player, Action a, double amount);
	void endofgame( );
	void endofgame( MyCardMask cards );
	LoggerClass & GetLogger( ) { return m_bot.GetLogger( ); }
	void OnError( const std::string & errmsg ) { m_errhandler.OnError( errmsg ); }
private:
	void ExecuteQueue( );

	ReconcilerHelper & m_errhandler;
	const bool m_forceactions;
	bool m_haveacted;
	bool m_iserror;
	Player m_me;
	BotAPI & m_bot;
	std::deque< TaskThread::BindBasePtr > m_queue;
};

