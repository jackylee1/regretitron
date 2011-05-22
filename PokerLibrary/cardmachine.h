#ifndef __get_cards_h__
#define __get_cards_h__

#include "../MersenneTwister.h"
#include "../utility.h" //for uint64
#include "binstorage.h"
#include <poker_defs.h> //has CardMask
#include <string>
#include <vector>
using namespace std;

#define COMPILE_FLOPALYZER_TESTCODE 0

//used to pass in info about what cards to use
struct cardsettings_t
{
	int bin_max[4];
	string filename[4];
	int64 filesize[4];
	bool usehistory;
	bool useflopalyzer;
	bool useboardbins;
	int board_bin_max[4];
	string boardbinsfilename[4];
	int64 boardbinsfilesize[4];
};

class CardMachine
{
public:
	//opens the bin files, putting them into memory
	CardMachine(cardsettings_t cardsettings, bool issolver, MTRand::uint32 = MTRand::gettimeclockseed( ) );
	~CardMachine();

	inline const cardsettings_t& getparams() const { return myparams; }
	inline int getcardsimax(int gr) const { return cardsi_max[gr]; }
	inline MTRand::uint32 getrandseed( ) const { return m_randseed; }

	void getnewgame(int cardsi[4][2], int &twoprob0wins);
	int getindices(int gr, const vector<CardMask> &cards, vector<int> &handi, vector<int> &boardi); //returns cardsi
	inline int getcardsi(int gr, const vector<CardMask> &cards)
		{ vector<int> dummy1; vector<int> dummy2; return getindices(gr, cards, dummy1, dummy2); }
	void findexamplehand(int gr, const vector<int> &handi, vector<int> &boardi, vector<CardMask> &cards);
	int preflophandinfo(int index, string &handstring); //index is getindex2() index, return cardsi


#if COMPILE_FLOPALYZER_TESTCODE
	static void testflopalyzer();
#endif

	static cardsettings_t makecardsettings( 
			string pfbin, string fbin, string tbin, string rbin,
			int boardfbin, int boardtbin, int boardrbin,
			bool usehistory, bool useflopalyzer, bool useboardbins );

private:
	//we do not support copying.
	CardMachine(const CardMachine& rhs);
	CardMachine& operator=(const CardMachine& rhs);

	//computes useful index-like numbers about the boards
	static int flopalyzer(const CardMask flop);
	static int turnalyzer(const CardMask flop, const CardMask turn);
	static int rivalyzer(const CardMask flop, const CardMask turn, const CardMask river);

	//private data
	const cardsettings_t myparams;
	vector<int> cardsi_max; //constant too
	static const int FLOPALYZER_MAX[4]; //must be initialized in the cpp file, as it is an array (not integral data member)
	vector<PackedBinFile *> binfiles;
	vector<PackedBinFile *> boardbinfiles;
	bool solving; //is CardMachine being used by Solver or by BotAPI?
	MTRand::uint32 m_randseed;
	MTRand randforsolver;
	MTRand randforexamplehands;
};

#endif

