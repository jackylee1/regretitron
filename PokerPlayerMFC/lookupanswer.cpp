#include "stdafx.h"
#include "lookupanswer.h"
#include "../PokerLibrary/constants.h"


void readstrategy(int sceni, int beti, float * const probabilities, int n_acts)
{
	//calculate offset in bytes
	unsigned long long int offset;//64 bit just in case
	std::ifstream f;
	offset = sceni * SCENI_STRATDUMP_BYTES;

	if(beti < BETI9_CUTOFF)
		offset += STRATDUMP9_OFFSET + beti*8*sizeof(float);
	else if(beti < BETI3_CUTOFF)
		offset += STRATDUMP3_OFFSET + (beti-BETI9_CUTOFF)*2*sizeof(float);
	else if(beti < BETI2_CUTOFF)
		offset += STRATDUMP2_OFFSET + (beti-BETI3_CUTOFF)*1*sizeof(float);

	//open file, read nbytes, set probabilities[nbytes]=1-sum(rest)
	f.open("my data", std::ifstream::binary);
	f.read((char*const)probabilities, (n_acts-1)*sizeof(float));
	if(!f.good())
	{
		MessageBox(NULL,TEXT("Strategy files not found/working. Using random."),TEXT("PokerPlayer"),MB_ICONSTOP|MB_OK);
		for(int a=0; a<n_acts-1; a++) probabilities[a] = 1.0F/n_acts;
	}
	f.close();

	//fill in that last one here.
	probabilities[n_acts-1] = 1-std::accumulate(probabilities, probabilities + n_acts-1, 0.0);
}
	

	
