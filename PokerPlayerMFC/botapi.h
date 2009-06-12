#ifndef _botapi_h_
#define _botapi_h_

#include "strategy.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/constants.h"
#include "../MersenneTwister.h"
#include <vector>
#include <poker_defs.h>
using std::vector;

enum Action
{
	FOLD, //a player has folded
	CALL, //ends the betting, continuing at next round
	BET,  //keeps the betting going. could be check
	ALLIN
};


class DiagnosticsPage; //forward declaration

class BotAPI
{
public:
	BotAPI(bool diagon);
	~BotAPI();

	//control game progress
	void setnewgame(Player playernum, CardMask myhand, double sblind, double bblind, double stacksize); //can be called anytime
	void setnextround(int gr, CardMask newboard, double newpot); //must be called with gr = FLOP, TURN, or RIVER
	void doaction(Player player, Action a, double amount);
	Action getbotaction(double &amount);

	//diagnostics
	void setdiagnostics(bool onoff, CWnd* parentwin = NULL); //controls DiagnosticsPage

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

	//diagnostics window
	DiagnosticsPage * MyWindow;
	void populatewindow(CWnd * parentwin = NULL);
	void destroywindow();

	//private data
	Strategy * mystrats[MAX_STRATEGIES];
	int num_used_strats;
	Strategy * currstrat; //pointer from above array
	double multiplier; //ACTUAL big blind amount divided by big blind amount used by strategy
	vector<double> actualinv; //the invested amount from the ACTUAL game, SCALED by multiplier
	vector<int> perceivedinv; //the perceived invested amount, as determined by the steps in the betting tree
	double actualpot; //the potamount from the ACTUAL game, SCALED by multiplier
	int perceivedpot; //the perceived pot size

	Player myplayer; //current player number that the bot is playing as
	vector<CardMask> cards;

	int currentgr;
	int currentbeti;
	betihist_t betihistory;
	BetNode mynode;
	bool offtreebetallins; //when they bet and my tree doesn't have bets there
	bool isdiagnosticson;
	int answer; //updated as soon as its the bot's turn to act
	enum { INVALID, WAITING_ACTION, WAITING_ROUND } bot_status;
	MTRand actionchooser;
};

#endif
