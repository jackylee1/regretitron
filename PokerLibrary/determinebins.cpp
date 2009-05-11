#include "stdafx.h"
#include "determinebins.h"
#include "../HandIndexingV1/getindex2N.h"
#include "constants.h"

//some global pointer to data
HANDLE flopfile, turnfile, riverfile, flopfilemap, turnfilemap, riverfilemap;
binstruct * flopbins;
binstruct * turnbins;
binstruct * riverbins;

//I open the files here.
void initbins()
{
	//open the files
	flopfile = CreateFile(FLOPFILENAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	turnfile = CreateFile(TURNFILENAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	riverfile = CreateFile(RIVERFILENAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	//memory map the files
	flopfilemap = CreateFileMapping(flopfile, NULL, PAGE_READONLY, 0, 0, TEXT("flopbins"));
	turnfilemap = CreateFileMapping(turnfile, NULL, PAGE_READONLY, 0, 0, TEXT("turnbins"));
	riverfilemap = CreateFileMapping(riverfile, NULL, PAGE_READONLY, 0, 0, TEXT("riverbins"));

	//set global pointers to the files
	flopbins = (binstruct*) MapViewOfFile(flopfilemap, FILE_MAP_READ, 0, 0, 0); //size 0 maps entire file.
	turnbins = (binstruct*) MapViewOfFile(turnfilemap, FILE_MAP_READ, 0, 0, 0); //size 0 maps entire file.
	riverbins = (binstruct*) MapViewOfFile(riverfilemap, FILE_MAP_READ, 0, 0, 0); //size 0 maps entire file.

	//make sure it worked.
	if(flopfile == INVALID_HANDLE_VALUE || GetFileSize(flopfile, NULL) != FLOPFILESIZE || flopfilemap == NULL || flopbins == NULL)
		REPORT("could not open file with flop bins and memory map it.");
	if(turnfile == INVALID_HANDLE_VALUE || GetFileSize(turnfile, NULL) != TURNFILESIZE || turnfilemap == NULL || turnbins == NULL)
		REPORT("could not open file with turn bins and memory map it.");
	if(riverfile == INVALID_HANDLE_VALUE || GetFileSize(riverfile, NULL) != RIVERFILESIZE || riverfilemap == NULL || riverbins == NULL)
		REPORT("could not open file with river bins and memory map it.");
}

void closebins()
{
	UnmapViewOfFile(flopbins);
	UnmapViewOfFile(turnbins);
	UnmapViewOfFile(riverbins);
	CloseHandle(flopfilemap);
	CloseHandle(turnfilemap);
	CloseHandle(riverfilemap);
	CloseHandle(flopfile);
	CloseHandle(turnfile);
	CloseHandle(riverfile);
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
	flopbin = retrieve(flopbins[index / BINSPERSTRUCT], index % BINSPERSTRUCT);

	//turn
	CardMask_OR(board, flop, turn);
	index = getindex2N(priv, board, 4);
	turnbin = retrieve(turnbins[index / BINSPERSTRUCT], index % BINSPERSTRUCT);

	//river
	CardMask_OR(board, board, river);
	index = getindex2N(priv, board, 5);
	riverbin = retrieve(riverbins[index / BINSPERSTRUCT], index % BINSPERSTRUCT);
}

int getpreflopbin(const CardMask &priv)
{
	return getindex2(priv);
}

int getflopbin(const CardMask &priv, const CardMask &flop)
{
	int index = getindex2N(priv, flop, 3);
	return retrieve(flopbins[index / BINSPERSTRUCT], index % BINSPERSTRUCT);
}

int getturnbin(const CardMask &priv, const CardMask &flop, const CardMask &turn)
{
	CardMask board;
	int index;
	CardMask_OR(board, flop, turn);
	index = getindex2N(priv, board, 4);
	return retrieve(turnbins[index / BINSPERSTRUCT], index % BINSPERSTRUCT);
}

int getriverbin(const CardMask &priv, const CardMask &flop, const CardMask &turn, const CardMask &river)
{
	CardMask board;
	int index;
	CardMask_OR(board, flop, turn);
	CardMask_OR(board, board, river);
	index = getindex2N(priv, board, 5);
	return retrieve(riverbins[index / BINSPERSTRUCT], index % BINSPERSTRUCT);
}

