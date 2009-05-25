#ifndef _gamestate_h_
#define _gamestate_h_

#include "poker_defs.h"
#include "constants.h"

enum Player
{
	P0 = 0,//first to act post-flop
	P1 = 1 //first to act pre-flop
};

class GameState 
{
public:
	void dealnewgame();
	int getpreflopcardsi(Player pl);
	int getflopcardsi(Player pl);
	int getturncardsi(Player pl);
	int getrivercardsi(Player pl);
	int gettwoprob0wins();

protected: //so TreeState can inherate access to these.
	CardMask priv0, flop, turn, river;
	int flopscore, turnscore, riverscore;
	int binnumber[2][4];

private: //but, for clarity, BotAPI does not use these.
	CardMask priv1;
	int twoprob0wins;
};

#define COMBINE(i2, i1, n1) ((i2)*(n1) + (i1))
inline int GameState::getpreflopcardsi(Player pl)
{
	return binnumber[pl][PREFLOP];
}

inline int GameState::getflopcardsi(Player pl)
{
	return COMBINE(flopscore, binnumber[pl][FLOP], BIN_FLOP_MAX);
}

inline int GameState::getturncardsi(Player pl)
{
	return COMBINE(turnscore, binnumber[pl][TURN], BIN_TURN_MAX);
}

inline int GameState::getrivercardsi(Player pl)
{
	return COMBINE(riverscore, binnumber[pl][RIVER], BIN_RIVER_MAX);
}

inline int GameState::gettwoprob0wins()
{
	return twoprob0wins;
}

#endif