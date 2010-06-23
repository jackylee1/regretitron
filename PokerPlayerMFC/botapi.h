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
	BET,  //keeps the betting going. could be check
	BETALLIN //this is a bet of all-in
};


class DiagnosticsPage; //forward declaration

class BotAPI
{
public:
	BotAPI(string xmlfile, string botname = "bot", bool preload = false);
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

	//amount is the total amount of a wager when betting/raising (relative to the beginning of the round)
	//          the amount aggreed upon when calling (same as above) or folding (not same as above)
	Action getbotaction(double &amount);

	//diagnostics
#ifdef _MFC_VER
	void setdiagnostics(bool onoff, CWnd* parentwin = NULL); //controls DiagnosticsPage
#endif
	double getstacksizemult() const 
		{ return (double) get_property(mystrats[0]->gettree(), settings_tag()).stacksize / get_property(mystrats[0]->gettree(), settings_tag()).bblind; }
	bool islimit() const { return get_property(mystrats[0]->gettree(), settings_tag()).limit; }
	bool isportfolio() const { return mystrats.size() >= 2; }
	string getxmlfile() const { return myxmlfile; }

private:
	//we do not support copying.
	BotAPI(const BotAPI& rhs);
	BotAPI& operator=(const BotAPI& rhs);

	//helperfunctions
	void docall(Player pl, double amount);
	void dobet(Player pl, double amount);
	void doallin(Player pl);
	void processmyturn();
	double mintotalwager();

	//xml file
	string myxmlfile;

	//logging file
	string myname;

	//diagnostics window
	DiagnosticsPage * MyWindow;
#ifdef _MFC_VER
	void populatewindow(CWnd * parentwin = NULL);
	void destroywindow();
#endif
	bool isdiagnosticson;

	//private data
#if PLAYOFFDEBUG > 0
	MTRand &actionchooser;
#else
	MTRand actionchooser;
#endif
	vector<Strategy*> mystrats;
	Strategy * currstrat; //pointer from above array

	double multiplier; //ACTUAL big blind amount divided by big blind amount used by strategy
	double truestacksize; //actual stacksize, SCALED by multiplier
	vector<double> actualinv; //the invested amount from the ACTUAL game, SCALED by multiplier
	vector<int> perceivedinv; //the perceived invested amount, as determined by the steps in the betting tree
	double actualpot; //the potamount from the ACTUAL game, SCALED by multiplier
	int perceivedpot; //the perceived pot size

	Player myplayer; //current player number that the bot is playing as
	vector<CardMask> cards;

	int currentgr;
	Vertex currentnode;
	bool offtreebetallins; //when they bet and my tree doesn't have bets there
	enum { INVALID, WAITING_ACTION, WAITING_ROUND } bot_status;
};

#endif
