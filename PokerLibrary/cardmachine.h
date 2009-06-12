#ifndef __get_cards_h__
#define __get_cards_h__

#include "../MersenneTwister.h"
#include "../utility.h" //for uint64
#include <poker_defs.h> //has CardMask
#include <string>
#include <vector>
#include <fstream>
using namespace std;

#define COMPILE_FLOPALYZER_TESTCODE 0

//used to pass in info about what cards to use
struct cardsettings_t
{
	int bin_max[4];
	string filename[4];
	uint32 filesize[4];
};

class CardMachine
{
public:
	//opens the bin files, putting them into memory
	CardMachine(cardsettings_t cardsettings, bool issolver, bool seedrand = false, int seedwith = 0);
	~CardMachine();

	inline const cardsettings_t& getparams() const { return myparams; }
	inline int getcardsimax(int gr) const { return cardsi_max[gr]; }

	void getnewgame(int cardsi[4][2], int &twoprob0wins);
	int getindices(int gr, const vector<CardMask> &cards, int &handi, int &boardi); //returns cardsi
	inline int getcardsi(int gr, const vector<CardMask> &cards)
		{ int dummy1, dummy2; return getindices(gr, cards, dummy1, dummy2); }
	void findexamplehand(int gr, int handi, int boardi, vector<CardMask> &cards);
	string preflophandstring(int index);


#if COMPILE_FLOPALYZER_TESTCODE
	static void testflopalyzer();
#endif

private:
	//we do not support copying.
	CardMachine(const CardMachine& rhs);
	CardMachine& operator=(const CardMachine& rhs);

	//computes useful index-like numbers about the boards
	static int flopalyzer(const CardMask flop);
	static int turnalyzer(const CardMask flop, const CardMask turn);
	static int rivalyzer(const CardMask flop, const CardMask turn, const CardMask river);

	const cardsettings_t myparams;
	int cardsi_max[4]; //constant too
	static const int FLOPALYZER_MAX[4]; //must be initialized in the cpp file, as it is an array (not integral data member)

	//is CardMachine being used by Solver or by BotAPI?
	bool solving;

	//if we are solving, these are used
	MTRand randforsolver;
	MTRand randforexamplehands;
	uint64 * bindata[4]; //should replace with this type of shit: http://ubuntuforums.org/showthread.php?t=67867

	//if we are playing, these are used
	ifstream filepointers[4];
	int readbintable(const int gr, const int index);
};

#endif

