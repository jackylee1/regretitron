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
	int getstacksizemult() const { return mystrats[0]->gettree().getparams().stacksize / mystrats[0]->gettree().getparams().bblind; }
	bool islimit() const { return mystrats[0]->gettree().getparams().limit; }

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
	bool isdiagnosticson;

	//private data
	MTRand actionchooser;
	vector<Strategy*> mystrats;
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
	BetNode mynode;
	bool offtreebetallins; //when they bet and my tree doesn't have bets there
	int answer; //updated as soon as its the bot's turn to act
	enum { INVALID, WAITING_ACTION, WAITING_ROUND } bot_status;

	struct HistoryNode
	{   //the betting history is defined by gr and beti, but it's cleaner and allows more
		//error checking to store pot too.
		HistoryNode(int gr2, int pot2, int beti2) : gr(gr2), pot(pot2), beti(beti2) {}
		int gr;
		int pot;
		int beti;
	};

	//another walker incarnation used for indexing
	//only used once by BotAPI, so it is here.
	class BetHistoryIndexer
	{
	public:
		BetHistoryIndexer(BotAPI * parent) : parentbotapi(parent), history(), 
			counters(4, vector<int>(MAX_NODETYPES, 0)) {}
		void push(int gr, int pot, int beti);
		void reset();

		int getactioni() const { return current_actioni; }
		int getnuma() const { return current_numa; }

		const vector< HistoryNode >& gethistory() const { return history; }

	private:
		//we do not support copying.
		BetHistoryIndexer(const BetHistoryIndexer& rhs);
		BetHistoryIndexer& operator=(const BetHistoryIndexer& rhs);

		BotAPI * parentbotapi;

		bool go(int gr, int pot, int beti);

		vector< HistoryNode > history;
		vector< vector<int> > counters;
		int current_actioni;
		int current_numa;
	} historyindexer;
};

#endif
