#include "stdafx.h"
#include "savestrategy.h"
#include "../PokerLibrary/treenolimit.h"
#include "memorymgr.h"
#include "../PokerLibrary/rephands.h"
#include "../PokerLibrary/constants.h"

using namespace std;

#ifdef __GNUC__ //needed to print __float128 on linux
inline ostream& operator << (ostream& os, const __float128& n) { return os << (__float80)n; }
#endif

//this function takes a filename, and prints out a simple listing of the
//results for betting node 0, at the preflop. it attempts to
//emulate the output of this website if PUSHFOLD is true:
//http://www.daimi.au.dk/~bromille/pokerdata/SingleHandSmallBlind/SHSB.4000
//used only by my main routines in PokerNoLimit.cpp
void printfirstnodestrat(char const * const filename)
{
	//tree stuff
	betnode mynode; // will hold compressed node
	getnode(PREFLOP, 0, 0, mynode);
	int numa = mynode.numacts;

	//log stuff
	ofstream log(filename);
	log << fixed << setprecision(5);

	fpworking_type strataccum;
	for(int cardsi=CARDSI_PFLOP_MAX-1; cardsi>=0; cardsi--)
	{
		//data stuff
		fpstore_type * stratn, * stratd, * regret;
		dataindexing(PREFLOP, numa, 0, cardsi, stratn, stratd, regret);

#if PUSHFOLD
		log << setw(5) << left << preflophandstring(cardsi)+":";
#else
		//print the scenario index and an example hand
		log << endl << "cardsi: " << cardsi << ":" << endl;
		log << preflophandstring(cardsi)+":" << endl;
#endif
		
		strataccum=0;
		for (int a=0; a<numa-1; a++)
		{
			fpworking_type myprob = (fpworking_type)stratn[a] / *stratd;

			strataccum+=myprob;
#if PUSHFOLD
			if(myprob>0.98) log << "fold" << endl;
			else if (myprob>0.02)
#endif
			log << setw(14) << actionstring(PREFLOP,a,mynode,1.0)+": "
				<< 100*stratn[a] / *stratd << "%" << endl;
		}
		//finally, handle the last strat
#if STORENTHSTRAT
		fpworking_type laststrat = stratn[numa-1] / *stratd;
#else
		fpworking_type laststrat = 1-strataccum;
#endif

#if PUSHFOLD
		if(laststrat>0.98F) log << "jam" << endl;
		else if (laststrat>0.02F)
#endif
		log << setw(14) << actionstring(PREFLOP,numa-1,mynode,1.0)+": " 
			<< 100*laststrat << "%" << endl;

#if STORENTHSTRAT && PRINTSUM
		log << "Sum of strats: " 
			<< (fpworking_type)std::accumulate(stratn, stratn+numa, (fpworking_type)0) / *stratd << endl;
#endif
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
