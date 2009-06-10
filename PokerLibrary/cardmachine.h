#ifndef __get_cards_h__
#define __get_cards_h__

#include "../MersenneTwister.h"
#include "../utility.h" //for uint64
#include <poker_defs.h> //has CardMask

#define COMPILE_FLOPALYZER_TESTCODE 0

//used to pass in info about what cards to use
struct cardsettings_t
{
	int bin_max[4];
	char filename[4][64];
	uint32 filesize[4];
};

class CardMachine
{
public:
	//opens the bin files, putting them into memory
	CardMachine(cardsettings_t cardsettings, bool seedrand = false, int seedwith = 0);
	~CardMachine();

	//randomly deals cards, filling in required information, not thread safe
	void getnewgame(int cardsi[4][2], int &twoprob0wins);

	inline int getcardsimax(int gr) const { return cardsi_max[gr]; }

#if COMPILE_FLOPALYZER_TESTCODE
	static void testflopalyzer();
#endif

private:
	//my precious
	MTRand mersenne;

	//contains the entire bin files
	uint64 * bindata[4];

	//computes useful index-like numbers about the boards
	static int flopalyzer(const CardMask flop);
	static int turnalyzer(const CardMask flop, const CardMask turn);
	static int rivalyzer(const CardMask flop, const CardMask turn, const CardMask river);

	//max values, first two set dynamically, but then never change (wish i could easily make them const)
	int cardsi_max[4];
	int bin_max[4];
	//must be initialized in the cpp file, as it is an array (not integral data member)
	static const int FLOPALYZER_MAX[4];
};

#endif

