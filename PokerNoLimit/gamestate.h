#ifndef _gamestate_h_
#define _gamestate_h_

#include "flopalyzer.h"
#include "determinebins.h"

class GameState 
{
public:
	void dealnewgame();
	int getscenarioi(int gr, int playertoact, int pot, int bethist[3]);
	float getprob0wins();

private:
	CardMask priv0, priv1, flop, turn, river;
	int flopscore, turnscore, riverscore;
	int binnumber[2][4];
	int prob0wins;
};

inline float GameState::getprob0wins()
{
	return prob0wins;
}

#endif