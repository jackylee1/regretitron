#ifndef __binstruct8_h__
#define __binstruct8_h__

const int BIN_MAX = 8;

const int BINSPERSTRUCT = 10;

struct binstruct
{
	//each stores values from 0-7
	//30 bits total
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

//used for memmapping.
const LPCTSTR FLOPFILENAME = TEXT("files/flop8BINS.dat");
const LPCTSTR TURNFILENAME = TEXT("files/turn8BINS.dat");
const LPCTSTR RIVERFILENAME = TEXT("files/river8BINS.dat");

//ensure INDEX2N_MAX is initialized by the time we get here.
const unsigned int FLOPFILESIZE = (INDEX23_MAX/BINSPERSTRUCT+1)*sizeof(binstruct);
const unsigned int TURNFILESIZE = (INDEX24_MAX/BINSPERSTRUCT+1)*sizeof(binstruct);
const unsigned int RIVERFILESIZE = (INDEX25_MAX/BINSPERSTRUCT+1)*sizeof(binstruct);

//these must be inline to avoid using a cpp file:

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

inline void store(binstruct &word, int bin, int n)
{
	if(bin<0 || bin>=BIN_MAX)
		REPORT("failed bin number in store");
	switch(n)
	{
	case 0: word.bin0 = bin; break;
	case 1: word.bin1 = bin; break;
	case 2: word.bin2 = bin; break; 
	case 3: word.bin3 = bin; break;
	case 4: word.bin4 = bin; break;
	case 5: word.bin5 = bin; break;
	case 6: word.bin6 = bin; break;
	case 7: word.bin7 = bin; break;
	case 8: word.bin8 = bin; break;
	case 9: word.bin9 = bin; break;
	default: REPORT("failed n in store");
	}
}

#endif