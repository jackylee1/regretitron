
const int BINSPERSTRUCT = 6;
const int BIN_MAX = 32;

struct binstruct
{
	//each stores values from 0-31
	//30 bits total
	unsigned int bin0 : 5;
	unsigned int bin1 : 5;
	unsigned int bin2 : 5;
	unsigned int bin3 : 5;
	unsigned int bin4 : 5;
	unsigned int bin5 : 5;
};

//used for memmapping.
const LPCTSTR FLOPFILENAME = TEXT("files/flop32BINS.dat");
const LPCTSTR TURNFILENAME = TEXT("files/turn32BINS.dat");
const LPCTSTR RIVERFILENAME = TEXT("files/river32BINS.dat");

//ensure INDEX2N_MAX is initialized by the time we get here.
const unsigned int FLOPFILESIZE = (INDEX23_MAX/BINSPERSTRUCT+1)*sizeof(binstruct);
const unsigned int TURNFILESIZE = (INDEX24_MAX/BINSPERSTRUCT+1)*sizeof(binstruct);
const unsigned int RIVERFILESIZE = (INDEX25_MAX/BINSPERSTRUCT+1)*sizeof(binstruct);

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
	default:
		REPORT("invalid piece of binstruct!");
	}
}

void store(binstruct &word, int bin, int n)
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
	default: REPORT("failed n in store");
	}
}
