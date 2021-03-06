#ifndef _botapi_h_
#define _botapi_h_

#include "strategy.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/constants.h"
#include "../MersenneTwister.h"
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
	BET   //keeps the betting going. could be check, or betting all-in
};


class DiagnosticsPage; //forward declaration

class BotAPI
{
public:
	static NullLogger botapinulllogger;

	BotAPI( string xmlfile, bool preload, MTRand::uint32 randseed, 
			LoggerClass & botapilogger,
			LoggerClass & botapireclog );
	~BotAPI();

	//control game progress

	//sblind, bblind - self explanatory
	//stacksize - smallest for ONE player
	void setnewgame(Player playernum, CardMask myhand, double sblind, double bblind, double stacksize); //can be called anytime

	//gr - zero offset gameround
	//newboard - only the NEW card(s) from this round
	//newpot - this is the pot for just ONE player
	void setnextround(int gr, CardMask newboard, double newpot); //must be called with gr = FLOP, TURN, or RIVER

	//amount is only used during a call or a bet
	//amount called - the total amount from ONE player to complete all bets from this round
	//amount bet - the total amount wagered from ONE player, beyond what the other has put out so far
	void doaction(Player player, Action a, double amount);

	//this version just gives you what you need
	Action getbotaction( double & amount ) { return getbotaction_internal( amount, false, FOLD, 0 ); }
	//this version just returns more info (which can be used to make the action human readable)
	Action getbotaction( double & amount, string & actionstr, int & level )
	{
		const Action act = getbotaction_internal( amount, false, FOLD, 0 );
		getactionstring( act, amount, actionstr, level );
		return act;
	}
	//this version just forces an action instead of getting it
	void forcebotaction( Action act, double amount, string & actionstr, int & level )
	{
		double amountcheck;
		const Action actcheck = getbotaction_internal( amountcheck, true, act, amount );
		if( ! ( act == actcheck && amount == amountcheck ) ) 
			REPORT( "forcebotaction failed" );
		getactionstring( act, amount, actionstr, level );
	}

	//these are for logging (possibly error checking) purposes
	void endofgame( );  // for when cards are unknown
	void endofgame( CardMask opponentcards ); //for when cards are known

	//diagnostics
#ifdef _MFC_VER
	void setdiagnostics(bool onoff, CWnd* parentwin = NULL); //controls DiagnosticsPage
#endif
	double getstacksizemult() const 
	{ 
		if( isportfolio( ) ) REPORT( "getstacksizemult not defined for portfolio strats" );
		return (double) get_property(mystrats[0]->gettree(), settings_tag()).stacksize / get_property(mystrats[0]->gettree(), settings_tag()).bblind; 
	}
	bool islimit() const { return get_property(mystrats[0]->gettree(), settings_tag()).limit; }
	bool isportfolio() const { return mystrats.size() >= 2; }
	string getxmlfile() const { return myxmlfile; }
	LoggerClass & GetLogger( ) { return m_logger; }

private:
	//amount is the total amount of a wager when betting/raising (relative to the beginning of the round)
	//          the amount aggreed upon when calling (same as above) or folding (not same as above)
	//forcing: if doforceaction is false, proceed as usual,
	//         otherwise, force the bot's action to be the forced values
	Action getbotaction_internal( double & amount, bool doforceaction, Action forceaction, double forceamount );

	//this function returns human readable info designed to be used at the time of a call to getbotaction
	void getactionstring( Action act, double amount, string & actionstr, int & level );

	//we do not support copying.
	BotAPI(const BotAPI& rhs);
	BotAPI& operator=(const BotAPI& rhs);

	//helperfunctions
	void dofold(Player pl, double amount);
	void docall(Player pl, double amount);
	void dobet(Player pl, double amount);
	void doallin(Player pl);
	void processmyturn();
	double mintotalwager();
	void logendgame( bool iwin, const std::string & story );

	//xml file
	string myxmlfile;

	//diagnostics window
	DiagnosticsPage * MyWindow;
#ifdef _MFC_VER
	void populatewindow(CWnd * parentwin = NULL);
	void destroywindow();
#endif
	bool isdiagnosticson;

	//private data
	LoggerClass & m_logger;
	LoggerClass & m_reclog;

#if PLAYOFFDEBUG > 0
	MTRand &actionchooser;
#else
	MTRand actionchooser;
#endif
	vector<Strategy*> mystrats;
	Strategy * currstrat; //pointer from above array

	double multiplier; //ACTUAL big blind amount divided by big blind amount used by strategy
	double actualstacksize; //actual stacksize, SCALED by multiplier, needed for limit
	double actualbblind; //needed for no-limit
	vector<double> actualinv; //the invested amount from the ACTUAL game, SCALED by multiplier
	vector<int> perceivedinv; //the perceived invested amount, as determined by the steps in the betting tree
	double actualpot; //the potamount from the ACTUAL game, SCALED by multiplier
	int perceivedpot; //the perceived pot size

	Player myplayer; //current player number that the bot is playing as
	vector<CardMask> cards;

	int currentgr;
	Vertex currentnode;
	bool offtreebetallins; //when they bet and my tree doesn't have bets there
	enum { INVALID, WAITING_ACTION, WAITING_ROUND, WAITING_ENDOFGAME } bot_status;

	//for error checking and logging
	Action lastchosenaction;
	double lastchosenamount;
};

#endif
