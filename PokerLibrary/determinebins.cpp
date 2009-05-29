#include "stdafx.h"
#include "determinebins.h"
#include "../HandIndexingV1/getindex2N.h"
#include "constants.h"
#include "binstorage.h"

//some global pointer to data

unsigned long long * flopbins;
unsigned long long * turnbins;
unsigned long long * riverbins;

//I open the files here.
//helper function for the below.
void loadbinfile(char const * const filename, const int bytes, unsigned long long* &buf)
{
	if(sizeof(unsigned long long) != 8) REPORT("well bloody hell...");
	if(bytes%8 != 0) REPORT("Your file is ragged.");

	//allocate memory

	buf = new unsigned long long[bytes/8];

	//open the file, read it in, and close it

	std::ifstream f(filename, std::ifstream::binary);
	f.read((char*)buf, bytes);
	if(f.gcount()!=bytes || f.eof() || EOF!=f.peek())
		REPORT("a bin file could not be found, or is corrupted.");
	f.close();
}

void initbins()
{
	loadbinfile(RIVERFILENAME, binfilesize(BIN_RIVER_MAX, INDEX25_MAX), riverbins);
	loadbinfile(TURNFILENAME, binfilesize(BIN_TURN_MAX, INDEX24_MAX), turnbins);
	loadbinfile(FLOPFILENAME, binfilesize(BIN_FLOP_MAX, INDEX23_MAX), flopbins);
}

void closebins()
{
	delete[] flopbins;
	delete[] turnbins;
	delete[] riverbins;
}

//read the data in those files
void determinebins(const CardMask &priv, 
						  const CardMask &flop, const CardMask &turn, const CardMask &river,
						  int &preflopbin, int &flopbin, int &turnbin, int &riverbin)
{
	CardMask board; //used to OR together the board cards
	int index; // avoid calculating index twice

	//preflop (just index now)
	preflopbin = getindex2(priv);

	//flop
	index = getindex2N(priv, flop, 3);
	flopbin = retrieve(flopbins, index, BIN_FLOP_MAX);

	//turn
	CardMask_OR(board, flop, turn);
	index = getindex2N(priv, board, 4);
	turnbin = retrieve(turnbins, index, BIN_TURN_MAX);

	//river
	CardMask_OR(board, board, river);
	index = getindex2N(priv, board, 5);
	riverbin = retrieve(riverbins, index, BIN_RIVER_MAX);
}

int getpreflopbin(const CardMask &priv)
{
	return getindex2(priv);
}

int getflopbin(const CardMask &priv, const CardMask &flop)
{
	return retrieve(flopbins, getindex2N(priv, flop, 3), BIN_FLOP_MAX);
}

int getturnbin(const CardMask &priv, const CardMask &flop, const CardMask &turn)
{
	CardMask board;
	CardMask_OR(board, flop, turn);
	return retrieve(turnbins, getindex2N(priv, board, 4), BIN_TURN_MAX);
}

int getriverbin(const CardMask &priv, const CardMask &flop, const CardMask &turn, const CardMask &river)
{
	CardMask board;
	CardMask_OR(board, flop, turn);
	CardMask_OR(board, board, river);
	return retrieve(riverbins, getindex2N(priv, board, 5), BIN_RIVER_MAX);
}

