#include "stdafx.h"
#include "memorymgr.h"

//Idea: we need to store the current strategy, average strategy (numerator &
// denominator), and accumulated total regret for each combination of 
// action, beti, and sceni

//these are pointers to chars because I want byte granularity.
#if N_CHUNKS > 1
char *datapointers[N_CHUNKS];
#else
char *datapointer;
#endif

//returns pointers for the four data arrays within the correct 
//datachunk for the correct sceni and beti
inline void getpointers(int sceni, int beti, int maxa, 
				 float * &stratt, float * &stratn, float * &stratd, float * &regret)
{
	//since this is char pointer, addition adds in multiples of 1, and our units here are always bytes.
#if N_CHUNKS > 1
	char * betiarray = datapointers[sceni/SCENIPERCHUNK] + (sceni%SCENIPERCHUNK)*SCENARIODATA_BYTES;
#else
	char * betiarray = datapointer + sceni*SCENARIODATA_BYTES;
#endif

	//set variables to pointers pointing into the data arrays, at the right spot
	if (beti<BETI9_CUTOFF)
	{
		if(maxa!=9 && maxa!=8) REPORT("invalid maxa, should be 8 or 9");
		stratt = (float*)(betiarray + STRATT9_OFFSET + beti*8*sizeof(float));
		stratn = (float*)(betiarray + STRATN9_OFFSET + beti*8*sizeof(float));
		stratd = (float*)(betiarray + STRATD9_OFFSET + beti*8*sizeof(float));
		regret = (float*)(betiarray + REGRET9_OFFSET + beti*9*sizeof(float));
	}
	else if (beti<BETI3_CUTOFF)
	{
		if(maxa!=3) REPORT("invalid maxa, should be 3");
		stratt = (float*)(betiarray + STRATT3_OFFSET + (beti-BETI9_CUTOFF)*2*sizeof(float));
		stratn = (float*)(betiarray + STRATN3_OFFSET + (beti-BETI9_CUTOFF)*2*sizeof(float));
		stratd = (float*)(betiarray + STRATD3_OFFSET + (beti-BETI9_CUTOFF)*2*sizeof(float));
		regret = (float*)(betiarray + REGRET3_OFFSET + (beti-BETI9_CUTOFF)*3*sizeof(float));
	}
	else if (beti<BETI2_CUTOFF)
	{
		if(maxa!=2) REPORT("invalid maxa, should be 2");
		stratt = (float*)(betiarray + STRATT2_OFFSET + (beti-BETI3_CUTOFF)*1*sizeof(float));
		stratn = (float*)(betiarray + STRATN2_OFFSET + (beti-BETI3_CUTOFF)*1*sizeof(float));
		stratd = (float*)(betiarray + STRATD2_OFFSET + (beti-BETI3_CUTOFF)*1*sizeof(float));
		regret = (float*)(betiarray + REGRET2_OFFSET + (beti-BETI3_CUTOFF)*2*sizeof(float));
	}
}


void initmem()
{
#if N_CHUNKS > 1
	for(int i=0; i<N_CHUNKS; i++)
	{
		datapointers[i] = new char[SCENIPERCHUNK*SCENARIODATA_BYTES];
		memset(datapointers[i], 0, SCENIPERCHUNK*SCENARIODATA_BYTES);
	}
#else
	datapointer = new char[SCENI_MAX*SCENARIODATA_BYTES];
	memset(datapointer, 0, SCENI_MAX*SCENARIODATA_BYTES);
#endif
}

void closemem()
{
#if N_CHUNKS > 1
	for(int i=0; i<N_CHUNKS; i++)
	{
		delete [] datapointers[i];
	}
#else
	delete [] datapointer;
#endif
}