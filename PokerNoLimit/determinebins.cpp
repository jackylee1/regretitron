#include "stdafx.h"
#include "determinebins.h"

/*The following data and struct should be copied from HandSSCalculator.*/
/*********************************************************************/

struct binstruct
{
	//30 bits
	unsigned int bin0 : 3;
	unsigned int bin1 : 3;
	unsigned int bin2 : 3;
	unsigned int bin3 : 3;
	unsigned int bin4 : 3;

	unsigned int bin5 : 3;
	unsigned int bin6 : 3;
	unsigned int bin7 : 3;
	unsigned int bin8 : 3;
	unsigned int bin9 : 3;
};

inline int retrieve(binstruct word, int n)
{
	switch (n)
	{
	case 0: return word.bin0;
	case 1: return word.bin1;
	case 2: return word.bin2;
	case 3: return word.bin3;
	case 4: return word.bin4;
	case 5: return word.bin5;
	case 6: return word.bin6;
	case 7: return word.bin7;
	case 8: return word.bin8;
	case 9: return word.bin9;
	default:
		REPORT("invalid piece of binstruct!");
	}
}

/**********************************************************************/

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
