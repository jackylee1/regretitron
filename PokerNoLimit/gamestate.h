#ifndef _gamestate_h_
#define _gamestate_h_

#include "flopalyzer.h"
#include "determinebins.h"

class GameState 
{
public:
	void dealnewgame();
	int getscenarioi(int gr, int playertoact, int pot, int bethist[3]);
	int gettwoprob0wins();

private:
	CardMask priv0, priv1, flop, turn, river;
	int flopscore, turnscore, riverscore;
	int binnumber[2][4];
	int twoprob0wins;
};

inline int GameState::gettwoprob0wins()
{
	return twoprob0wins;
}

#endif