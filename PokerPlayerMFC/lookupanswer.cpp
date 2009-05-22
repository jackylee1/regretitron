#include "stdafx.h"
#include "lookupanswer.h"
#include "../PokerLibrary/constants.h"


void readstrategy(int sceni, int beti, double * const probabilities, int n_acts)
{
	unsigned long long int offset;//64 bit just in case
	float floatprobs[9]; //the strategy is stored as floats, not doubles
	std::ifstream f;

	//calculate offset in bytes, values all stored as floats
	offset = sceni * SCENI_STRATDUMP_BYTES;
	if(beti < BETI9_CUTOFF)
		offset += STRATDUMP9_OFFSET + beti*8*sizeof(float);
	else if(beti < BETI3_CUTOFF)
		offset += STRATDUMP3_OFFSET + (beti-BETI9_CUTOFF)*2*sizeof(float);
	else if(beti < BETI2_CUTOFF)
		offset += STRATDUMP2_OFFSET + (beti-BETI3_CUTOFF)*1*sizeof(float);

	//n_acts is the actual number of actions for this node from the betting
	//tree itself. we have stored exactly that many - 1 actions here
	//the last one is determined from those initial n - 1 actions.
	f.open("strategy/13ss - 256,64,12bins - 51.2million.strat", std::ifstream::binary);
	f.seekg((std::streamoff)offset);
	f.read((char*)floatprobs, (n_acts-1)*sizeof(float));
	if(!f.good() || offset > 0x7fffffffUL)
	{
		MessageBox(NULL,TEXT("Strategy files not found/working. Using random. Could choose invalid actions."),TEXT("PokerPlayer"),MB_ICONSTOP|MB_OK);
		for(int a=0; a<n_acts-1; a++) floatprobs[a] = 1.0F/n_acts;
	}
	f.close();

	//now convert to doubles
	for(int a=0; a<n_acts-1; a++)
		probabilities[a] = (double)floatprobs[a];

	//finally, calculate the last element
	probabilities[n_acts-1] = 1-std::accumulate(probabilities, probabilities + n_acts-1, 0.0);
	//zero the rest
	for(int a=n_acts; a<9; a++)
		probabilities[a]=0;
}
	

	
