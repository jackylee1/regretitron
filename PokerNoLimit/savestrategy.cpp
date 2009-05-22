#include "stdafx.h"
#include "savestrategy.h"
#include "../PokerLibrary/treenolimit.h"
#include "memorymgr.h"
#include "../PokerLibrary/rephands.h"
#include "../PokerLibrary/constants.h"

using namespace std;

inline float safedivide(float n, float d)
{
	if (d==0 && n!=0) //if we just didn't get there, then the numerator would also be zero
		REPORT("zero denominator!")
	else if(d==0 && n==0) //could have just never gotten there, or could be invalid action
		return 0; //zero probability either way.
	else //d!=0
		return n/d;
}

//this function takes a filename, and prints out a simple listing of the
//results for betting node 0, sceni 0-168, which is the preflop. it attempts to
//emulate the output of this website if PUSHFOLD is true:
//http://www.daimi.au.dk/~bromille/pokerdata/SingleHandSmallBlind/SHSB.4000
//used only by my main routines in PokerNoLimit.cpp
void printfirstnodestrat(char const * const filename)
{
	ofstream log(filename);
	float *stratt, *stratn, *stratd, *regret;
	int maxa = pfn[0].numacts;
	int numa;
	float strataccum;
	bool isvalid[9];

	log << fixed << setprecision(5);

	for(int sceni=SCENI_PREFLOP_MAX-1; sceni>=0; sceni--)
	{
		getpointers(sceni, 0, maxa, 0, stratt, stratn, stratd, regret); //ignore walkeri
		numa = getvalidity(0, &(pfn[0]), isvalid);

#if PUSHFOLD
		log << setw(5) << left << preflophandstring(sceni)+":";
#else
		//print the scenario index and an example hand
		log << endl << "Scenario " << sceni << ":";
		printsceniinfo(log, sceni, 5); //starts with spaces and ends with endl
#endif
		
		//remember we only store max-1 strategies. the last must be found based on all adding to 1.
		strataccum=0;
		for (int a=0; a<maxa-1; a++)
		{
			float myprob = stratn[a]/stratd[a];

			if(isvalid[a])
			{
				strataccum+=myprob;
#if PUSHFOLD
				if(myprob>0.98F) log << "fold" << endl;
				else if (myprob>0.02F)
#endif
				log << setw(14) << actionstring(a,PREFLOP,pfn,1.0)+": " 
					<< 100*stratn[a]/stratd[a] << "%" << endl;
			}
		}
		//finally, handle the last strat, if it is valid.
		if(isvalid[maxa-1])
		{
#if PUSHFOLD
			if((1-strataccum)>0.98F) log << "jam" << endl;
			else if ((1-strataccum)>0.02F)
#endif
			log << setw(14) << actionstring(maxa-1,PREFLOP,pfn,1.0)+": " 
				<< 100*(1-strataccum) << "%" << endl;
		}
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
			for(int a=0; a<maxa-1; a++) result[a] = safedivide(stratn[a],stratd[a]);

			f.write((char*)result, 8*sizeof(float));
		}
		
		for(; b<BETI3_CUTOFF; b++)
		{
			int maxa = (s<SCENI_PREFLOP_MAX) ? pfn[b].numacts : n[b].numacts;
			getpointers(s,b,maxa,0,stratt,stratn,stratd,regret);

			//we print all values, valid or not. 
			memset(result, 0, 8*sizeof(float));
			for(int a=0; a<maxa-1; a++) result[a] = safedivide(stratn[a],stratd[a]);

			f.write((char*)result, 2*sizeof(float));
		}
		
		for(; b<BETI2_CUTOFF; b++)
		{
			int maxa = (s<SCENI_PREFLOP_MAX) ? pfn[b].numacts : n[b].numacts;
			getpointers(s,b,maxa,0,stratt,stratn,stratd,regret);

			//we print all values, valid or not. 
			memset(result, 0, 8*sizeof(float));
			for(int a=0; a<maxa-1; a++) result[a] = safedivide(stratn[a],stratd[a]);

			f.write((char*)result, 1*sizeof(float));
		}
	}
	f.close();
}




void savexml(const string filename)
{
	using namespace ticpp;

	Document doc;  

	Declaration* decl = new Declaration("1.0","","");  
	doc.LinkEndChild(decl); 

	Element* root = new Element("strategysettingsv1");  
	doc.LinkEndChild(root);  

	Element* bins = new Element("bins");
	root->LinkEndChild(bins);

	Element* board;

	board = new Element("flop");
	board->SetAttribute("nbins", BIN_FLOP_MAX);
	//board->SetAttribute("filesize", FLOPFILESIZE);
	board->SetText(FLOPFILENAME);
	bins->LinkEndChild(board);

	board = new Element("turn");
	board->SetAttribute("nbins", BIN_TURN_MAX);
	//board->SetAttribute("filesize", TURNFILESIZE);
	board->SetText(TURNFILENAME);
	bins->LinkEndChild(board);

	board = new Element("river");
	board->SetAttribute("nbins", BIN_FLOP_MAX);
	//board->SetAttribute("filesize", RIVERFILESIZE);
	board->SetText(RIVERFILENAME);
	bins->LinkEndChild(board);

	doc.SaveFile(filename.c_str());
}