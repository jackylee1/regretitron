#ifndef _gamestate_h_
#define _gamestate_h_

#include "poker_defs.h"

class GameState 
{
public:
	void dealnewgame();
	int getscenarioi(int gr, int ptoact, int pot, int bethist[3]);
	int gettwoprob0wins();

protected: //so TreeState can inherate access to these.
	CardMask priv0, flop, turn, river;
	int flopscore, turnscore, riverscore;
	int binnumber[2][4];

private: //but, for clarity, BotAPI does not use these.
	CardMask priv1;
	int twoprob0wins;
};

inline int GameState::gettwoprob0wins()
{
	return twoprob0wins;
}

#endif