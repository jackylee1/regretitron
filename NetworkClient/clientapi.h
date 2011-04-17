#pragma once

#include "../NetworkServer/servercommon.h"
#include "../PokerLibrary/constants.h"
#include "../utility.h"
#include <vector>
#include <string>
#include <poker_defs.h>
using std::vector;
using std::string;

enum Action
{
	FOLD, //a player has folded
	CALL, //ends the betting, continuing at next round, could be calling all-in
	BET,  //keeps the betting going. could be check
	BETALLIN //this is a bet of all-in
};


class ClientAPI
{
public:
	ClientAPI( ) 
		: m_sessiontype( SESSION_NONE )
		, m_ioservice( )
		, m_socket( m_ioservice )
	{ }
	~ClientAPI( ) { }

	//startup event
	void Init( );

	//create new session event
	void CreateNewTourneySession( const MessageCreateNewTourneySession & request );
	void CreateNewCashSession( const MessageCreateNewCashSession & request );

	//game events
	void setnewgame(Player playernum, CardMask myhand, double sblind, double bblind, double stacksize);
	void setnextround(int gr, CardMask newboard, double newpot);
	void doaction(Player player, Action a, double amount);
	Action getbotaction( double & amount, string & actionstr, int & level );
	Action getbotaction( double & amount )
	{
		string unusedstr;
		int unusedlevel;
		return getbotaction( amount, unusedstr, unusedlevel );
	}
	void endofgame( );  // for when cards are unknown
	void endofgame( CardMask opponentcards ); //for when cards are known
	bool islimit( ) { return true; } 

	//part of BotAPI not implemented in ClientAPI
	string getxmlfile() const { return "Using network client."; }
#ifdef _MFC_VER
	void setdiagnostics( bool, CWnd* = NULL) { }
#endif

	//close out session event
	void CloseSession( const vector< string > &, MessageCloseSession & request );
	void CancelSession( );

	SessionTypeEnum GetSessionType( ) { return m_sessiontype; }

private:
	void ReadHeader( MessageTypeEnum expectedtype );
	void ReadSessionState( bool showpopup );

	MessageHeader m_commonheader;
	SessionTypeEnum m_sessiontype;

	boost::asio::io_service m_ioservice;
	tcp::socket m_socket;
};

