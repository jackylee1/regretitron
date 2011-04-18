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
using std::vector;
using std::string;

class Reconciler
{
public:
	Reconciler( const std::string & reconcileagent, MTRand::uint32 seed, bool forceactions,
			LoggerClass & botapilogger,
			LoggerClass & botapireclog )
		: m_forceactions( forceactions )
		, m_haveacted( false )
		, m_bot( reconcileagent, false, seed, botapilogger, botapireclog )
	{ }
	void setnewgame(uint64 gamenumber, Player playernum, MyCardMask cards, double sblind, double bblind, double stacksize);
	void setnextround(int gr, MyCardMask newboard, double newpot);
	void doaction(Player player, Action a, double amount);
	void endofgame( );
	void endofgame( MyCardMask cards );
	LoggerClass & GetLogger( ) { return m_bot.GetLogger( ); }
private:
	void ExecuteQueue( );

	const bool m_forceactions;
	bool m_haveacted;
	Player m_me;
	BotAPI m_bot;
	std::deque< TaskThread::BindBasePtr > m_queue;
};

