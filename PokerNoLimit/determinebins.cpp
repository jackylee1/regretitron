#include "stdafx.h"
#include "determinebins.h"

//some global pointer to data
char * flopbins;
char * turnbins;
char * riverbins;

//http://social.microsoft.com/Forums/en-US/netfxbcl/thread/902ce33d-93c6-4f95-8814-6cdac9ae874b/

void initbin()
{
	//memory map the files that hold all the bins
	HANDLE flopfilemap = CreateFileMapping(flopfile, NULL, PAGE_READONLY, 0, 0, "flopbins");
	//and set global pointer to the files
	flopbins = MapViewOfFile(flopfilemap, FILE_MAP_READ, 0, 0, 
}

inline void determinebins(const CardMask &priv, const CardMask &flop, const CardMask &turn, const CardMask &river,
				   int &preflopbin, int &flopbin, int &turnbin, int &riverbin)
{
	CardMask board;

	preflopbin = getindex2(priv);

	flopbin = flopbins[getindex2N(priv, flop, 3)];

	CardMask_OR(board, flop, turn);
	turnbin = turnbins[getindex2N(priv, board, 4)];

	CardMask_OR(board, board, river);
	riverbin = riverbins[getindex2N(priv, board, 5)];
}