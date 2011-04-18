#include "reconciler.h"

// ............................................
void Reconciler::setnewgame(uint64 gamenumber, Player playernum, MyCardMask cards, double sblind, double bblind, double stacksize)
{
	if( m_haveacted )
		ExecuteQueue( );
	else
		m_queue.clear( );

	m_me = playernum;
	m_haveacted = false;
	m_iserror = false;
	m_queue.push_back( TaskThread::bind( & m_bot, & BotAPI::setnewgame, playernum, cards.Get( ), sblind, bblind, stacksize ) );
}

void Reconciler::setnextround(int gr, MyCardMask newboard, double newpot)
{
	m_queue.push_back( TaskThread::bind( & m_bot, & BotAPI::setnextround, gr, newboard.Get( ), newpot ) );
	if( m_haveacted )
		ExecuteQueue( );
}

void Reconciler::doaction(Player player, Action action, double amount)
{
	if( player == m_me ) // bot is about to go, must clear the pipes
	{
		ExecuteQueue( );
		string actionstr;
		int level;
		try
		{
			if( m_forceactions )
				m_bot.forcebotaction( action, amount, actionstr, level ); //tell the bot what to do
			else
				action = m_bot.getbotaction( amount ); //get an action from the bot
		}
		catch( std::exception & e )
		{
			m_bot.GetLogger( )( string( "ERROR: " ) + e.what( ) );
			m_iserror = true;
		}
		m_queue.push_back( TaskThread::bind( & m_bot, & BotAPI::doaction, player, action, amount ) );
		ExecuteQueue( );
		m_haveacted = true;
	}
	else
		m_queue.push_back( TaskThread::bind( & m_bot, & BotAPI::doaction, player, action, amount ) );
}

void Reconciler::endofgame( )
{
	m_queue.clear( );
	if( m_haveacted )
		m_queue.push_back( TaskThread::bind( & m_bot, 
					static_cast< void( BotAPI::* )( ) >( & BotAPI::endofgame ) ) );
}

void Reconciler::endofgame( MyCardMask cards )
{
	m_queue.clear( );
	if( m_haveacted )
		m_queue.push_back( TaskThread::bind( & m_bot, 
					static_cast< void( BotAPI::* )( CardMask ) >( & BotAPI::endofgame ), 
					cards.Get( ) ) );
}


void Reconciler::ExecuteQueue( )
{
	while( ! m_queue.empty( ) && ! m_iserror )
	{
		try
		{
			m_queue.front( )->run( );
		}
		catch( std::exception & e )
		{
			m_bot.GetLogger( )( string( "ERROR: " ) + e.what( ) );
			m_iserror = true;
		}
		m_queue.pop_front( );
	}
	if( m_iserror )
		m_queue.clear( );
}


