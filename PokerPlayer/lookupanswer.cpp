#include "stdafx.h"
#include "lookupanswer.h"
#include "../PokerLibrary/constants.h"


void readstrategy(int sceni, int beti, float * const probabilities)
{
	//calculate offset in bytes
	unsigned long long int offset;//64 bit just in case
	int nelmt;
	std::ifstream f;
	offset = sceni * SCENI_STRATDUMP_BYTES;

	if(beti < BETI9_CUTOFF)
	{
		offset += STRATDUMP9_OFFSET + beti*8*sizeof(float);
		nelmt=8;
	}
	else if(beti < BETI3_CUTOFF)
	{
		offset += STRATDUMP3_OFFSET + (beti-BETI9_CUTOFF)*2*sizeof(float);
		nelmt=2;
	}
	else if(beti < BETI2_CUTOFF)
	{
		offset += STRATDUMP2_OFFSET + (beti-BETI3_CUTOFF)*1*sizeof(float);
		nelmt=1;
	}

	//open file, read nbytes, set probabilities[nbytes]=1-sum(rest)
	f.open("my data", std::ifstream::binary);
	f.read((char*const)probabilities, nelmt*sizeof(float));
	f.close();

	//fill in that last one here.
	probabilities[nelmt] = 1-std::accumulate(probabilities, probabilities + nelmt, 0.0);
}
	

	
