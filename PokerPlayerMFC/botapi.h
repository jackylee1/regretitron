#ifndef _botapi_h_
#define _botapi_h_

#include "../PokerLibrary/gamestate.h"

enum Action
{
	FOLD, //a player has folded
	CALL, //ends the betting, continuing at next round
	BET,  //keeps the betting going. could be check i spose
	ALLIN //a player has BET all-in. otherwise should use CALL
};

enum Player
{
	P0 = 0,//first to act post-flop
	P1 = 1 //first to act pre-flop
};

//forward declaration
struct betnode;

class BotAPI : private GameState
{
public:

	//--------------------------------------------------------
	BotAPI(bool diagon);
	~BotAPI();

	//--------------------------------------------------------
	//these change sceni values and not beti
	//there purpose is to reset the state that corresponds to starting a new
	//betting round. i save a betting tree for each sceni.

	//resets the game to the preflop, can be called at any time
	void setnewgame(Player playernum, CardMask hand, double sblind, double bblind, double stacksize);
	//these error check the gameround, the bettingnode, and the invested
	void setflop(CardMask theflop, double newpot); //these must be called in order, after preflop
	void setturn(CardMask theturn, double newpot);
	void setriver(CardMask theriver, double newpot);

	//--------------------------------------------------------
	//- these change beti values and not sceni
	//- these transverse that betting tree, until
	//the game ends or we hit another game round or something
	//- all the real logic is in the doXX helper functions below
	//- this function is a wrapper function for diagnostics
	void advancetree(Player player, Action a, double amount);

	//--------------------------------------------------------
	//tell you what to do fool
	Action getanswer(double &amount);

	//--------------------------------------------------------
	//Diagnostics
	void setdiagnostics(bool onoff);

private:

	//helperfunctions for advancetree
	void docall(Player pl, double amount);
	void dobet(Player pl, double amount);
	void doallin(Player pl);
	int getbestbetact(double betsize);
	int getallinact();
	int getbethist();
	void processmyturn();
	//helperfunctions in general
	Player currentplayer();
	double mintotalwager();

	//current player number that the bot is playing as
	Player myplayer;
	//uses the values as defined in PokerLibrary/constants.h
	int currentgr;
	//exact pot amount (pre betting round) and exact invested amount (only 
	//current betting round, for each player) as the tree won't cut it when 
	//it comes to keeping track of these things.
	//the potamount from the ACTUAL game, SCALED by multiplier
	double currentpot;
	//the invested amount from the ACTUAL game, SCALED by multiplier
	double invested[2]; 
	//the perceived invested amount, as determined by the steps in the betting tree
	int perceived[2];
	//the ratio between the ACTUAL big blind amount and the
	//big blind amount the bot used to solve the game
	double multiplier;
	//the current scenario index computed each change in gameround
	int currentsceni;
	//the current betting node index, computed each change in betting action
	int currentbetinode;
	betnode const * mynode;
	//betting history, updated upon completion of a game/betting round
	int bethist[3];

	//for that sticky situation where they bet when i don't want them to
	bool offtreebetallins;
	//whether we show diagnostics or not
	bool isdiagnosticson;

	//updated when it's the bot's turn to act
	//index of the chosen action in the betting tree
	//can be overridden in diagnostic box
	int answer;
};

#endif