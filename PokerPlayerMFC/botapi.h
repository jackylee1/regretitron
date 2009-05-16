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

//forward declaration
struct betnode;

class BotAPI : private GameState
{
public:

	BotAPI(bool diagon);
	~BotAPI();
	//these change sceni values and not beti
	//there purpose is to reset the state that corresponds to starting a new
	//betting round. i save a betting tree for each sceni.
	void setnewgame(int playernum, CardMask hand, float sblind, float bblind); //resets the game to the preflop, can be called at any time
	//these error check the gameround, the bettingnode, and the invested
	void setflop(CardMask theflop, float newpot); //these must be called in order, after preflop
	void setturn(CardMask theturn, float newpot); //pots are always in units of SB
	void setriver(CardMask theriver, float newpot);

	//this changes beti values and not sceni
	//this purpose is to transverse that betting tree, until
	//the game ends or we hit another game round or something
	void advancetree(int player, Action a, int amount);

	//tell you what to do fool
	Action getanswer(float &amount);

	//Diagnostics
	void setdiagnostics(bool onoff);

private:

	//public one is wrapper function for diagnostics
	void _advancetree(int player, Action a, int amount);
	//helperfunctions for advancetree
	int getbestbetact(betnode const * mynode, int betsize);
	int getallinact(betnode const * mynode);
	int getbethist(betnode const * mynode);

//set in setnewgame/flop/turn/river
	int myplayer;
	int currentgr;
	//exact pot amount (pre betting round) and exact invested amount (only 
	//current betting round, for each player) as the tree won't cut it when 
	//it comes to keeping track of these things.
	float currentpot;
	float invested[2]; 
	float multiplier;
	//perceived invested amount, as determined by the steps in the betting tree
	int perceived[2]; 
	int currentsceni;
//set in advance tree
	int currentbetinode;
	int bethist[3];

	//for that sticky situation where they bet when i don't want them to
	bool offtreebetallins;
	//whether we show diagnostics or not
	bool isdiagnosticson;
	//if we've ever played anything yet
	bool startedgame;
};

#endif