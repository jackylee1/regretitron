#include "stdafx.h"
#include "savestrategy.h"
#include "../PokerLibrary/treenolimit.h"
#include "memorymgr.h"
#include "../PokerLibrary/rephands.h"
#include "../PokerLibrary/constants.h"

using namespace std;

void printfirstnodestrat(char const * const filename)
{
	ofstream log(filename);
	float *stratt, *stratn, *stratd, *regret;
	int maxa = pfn[0].numacts;
	int numa;
	float strataccum;
	bool isvalid[9];

	log << fixed << setprecision(5);

	for(int sceni=0; sceni<SCENI_PREFLOP_MAX; sceni++)
	{
		getpointers(sceni, 0, maxa, 0, stratt, stratn, stratd, regret); //ignore walkeri
		numa = getvalidity(0, &(pfn[0]), isvalid);

		//print the scenario index and an example hand
		log << endl << "Scenario " << sceni << ":";
		printrepinfo(log, sceni, 5); //starts with spaces and ends with endl
		
		//remember we only store max-1 strategies. the last must be found based on all adding to 1.
		strataccum=0;
		for (int a=0; a<maxa-1; a++)
		{
			if(isvalid[a])
			{
				strataccum+=stratn[a]/stratd[a];
				log << "   " << actionstring(a,PREFLOP,0) << 100*stratn[a]/stratd[a] << "%" << endl;
			}
		}
		//finally, handle the last strat, if it is valid.
		if(isvalid[maxa-1])
			log << "   " << actionstring(maxa-1,PREFLOP,0) << 100*(1-strataccum) << "%" << endl;
	}
	log.close();
}

void dumpstratresult(const char * const filename)
{
	ofstream f(filename, ofstream::binary);
	float *stratt, *stratn, *stratd, *regret;
	float result[8];

	//dumps out the strategy result in it's own format.
	for(int s=0; s<SCENI_MAX; s++)
	{
		int b=0;

		for(; b<BETI9_CUTOFF; b++)
		{
			int maxa = (s<SCENI_PREFLOP_MAX) ? pfn[b].numacts : n[b].numacts;
			getpointers(s,b,maxa,0,stratt,stratn,stratd,regret);

			//we print all values, valid or not. 
			memset(result, 0, 8*sizeof(float));
			for(int a=0; a<maxa-1; a++) result[a] = stratn[a]/stratd[a];

			f.write((char*)result, 8*sizeof(float));
		}
		
		for(; b<BETI3_CUTOFF; b++)
		{
			int maxa = (s<SCENI_PREFLOP_MAX) ? pfn[b].numacts : n[b].numacts;
			getpointers(s,b,maxa,0,stratt,stratn,stratd,regret);

			//we print all values, valid or not. 
			memset(result, 0, 8*sizeof(float));
			for(int a=0; a<maxa-1; a++) result[a] = stratn[a]/stratd[a];

			f.write((char*)result, 2*sizeof(float));
		}
		
		for(; b<BETI2_CUTOFF; b++)
		{
			int maxa = (s<SCENI_PREFLOP_MAX) ? pfn[b].numacts : n[b].numacts;
			getpointers(s,b,maxa,0,stratt,stratn,stratd,regret);

			//we print all values, valid or not. 
			memset(result, 0, 8*sizeof(float));
			for(int a=0; a<maxa-1; a++) result[a] = stratn[a]/stratd[a];

			f.write((char*)result, 1*sizeof(float));
		}
	}
	f.close();
}


