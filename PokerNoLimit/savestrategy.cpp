#include "stdafx.h"
#include "savestrategy.h"
#include "../PokerLibrary/treenolimit.h"
#include "memorymgr.h"
#include "../PokerLibrary/rephands.h"
#include "../PokerLibrary/constants.h"

using namespace std;


//this function takes a filename, and prints out a simple listing of the
//results for betting node 0, at the preflop. it attempts to
//emulate the output of this website if PUSHFOLD is true:
//http://www.daimi.au.dk/~bromille/pokerdata/SingleHandSmallBlind/SHSB.4000
//used only by my main routines in PokerNoLimit.cpp
void printfirstnodestrat(char const * const filename)
{
	ofstream log(filename);
	int maxa = pfloptree[0].numacts;
	int numa;
	float strataccum;
	bool isvalid[9];

	log << fixed << setprecision(5);

	for(int cardsi=CARDSI_PFLOP_MAX-1; cardsi>=0; cardsi--)
	{
		numa = getvalidity(0, &(pfloptree[0]), isvalid);
#if VECTORIZE
		vector<float>::iterator stratn, stratd, regret;
#else
		float * stratn, * stratd, * regret;
#endif
		betindexing(0,&(pftreebase->treedata[cardsi]),stratn,stratd,regret);

#if PUSHFOLD
		log << setw(5) << left << preflophandstring(cardsi)+":";
#else
		//print the scenario index and an example hand
		log << endl << "cardsi: " << cardsi << ":" << endl;
		log << preflophandstring(cardsi)+":" << endl;
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
				log << setw(14) << actionstring(a,PREFLOP,pfloptree,1.0)+": " 
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
			log << setw(14) << actionstring(maxa-1,PREFLOP,pfloptree,1.0)+": " 
				<< 100*(1-strataccum) << "%" << endl;
		}
	}
	log.close();
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