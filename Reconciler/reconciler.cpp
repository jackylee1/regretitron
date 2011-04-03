#include "reconciler.h"

// ............................................
// this is the logger used by BotAPI and others

FileLogger my_logger( "reconciler.regretinator.log", false );
//ConsoleLogger my_logger;
//NullLogger my_logger;

// ............................................
// this is the logger used by Reconciler's reconciler log

FileLogger reclog( "reconciler.reclog.log", false );
//ConsoleLogger reclog;
//NullLogger reclog;

// ............................................
// this is the logger used by BotAPI's reconciler log

FileLogger my_botapireclog( "reconciler.botapireclog.log", false );
//ConsoleLogger my_botapireclog;
//NullLogger my_botapireclog;

// finally, the references used by other modules of the program
LoggerClass & logger( my_logger );
LoggerClass & botapireclog( my_botapireclog );

// ............................................
void Reconciler::setnewgame(uint64 gamenumber, Player playernum, MyCardMask cards, double sblind, double bblind, double stacksize)
{
	m_me = playernum;
	m_bot.setnewgame( playernum, cards.Get( ), sblind, bblind, stacksize );
	reclog( "NEW GAME P" + tostr( playernum ) + " of " + tostr( sblind ) + "/" + tostr( bblind ) + " at " + tostr( stacksize ) + " with " + tostr( cards.Get( ) ) );
}

void Reconciler::setnextround(int gr, MyCardMask newboard, double newpot)
{
	m_bot.setnextround( gr, newboard.Get( ), newpot );

	switch( gr )
	{
#define NEWROUNDSTRING( gr ) case gr: reclog( #gr" " + tostr( newboard.Get( ) ) + " with pot " + tostr( newpot ) ); break;
		NEWROUNDSTRING( FLOP )
		NEWROUNDSTRING( TURN )
		NEWROUNDSTRING( RIVER )
	}
}

void Reconciler::doaction(Player player, Action action, double amount)
{
	if( player == m_me ) // bot is about to go, must clear the pipes
	{
		string actionstr;
		int level;
		if( m_forceactions )
			m_bot.forcebotaction( action, amount, actionstr, level ); //tell the bot what to do
		else
			action = m_bot.getbotaction( amount ); //get an action from the bot
	}

	m_bot.doaction( player, action, amount );

	switch( action )
	{
#define ACTIONSTRING( a ) case a: reclog( #a" " + tostr( amount ) + " by P" + tostr( player ) ); break;
		ACTIONSTRING( FOLD )
		ACTIONSTRING( CALL )
		ACTIONSTRING( BET )
		ACTIONSTRING( BETALLIN )
	}
}

void Reconciler::endofgame( )
{
	m_bot.endofgame( );
	reclog( "GAMEOVER (opponent mucks)\n" );
}

void Reconciler::endofgame( MyCardMask cards )
{
	m_bot.endofgame( cards.Get( ) );
	reclog( "GAMEOVER (opponent has " + tostr( cards.Get( ) ) + ")\n" );
}



