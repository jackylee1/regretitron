#include "stdafx.h"
#include "strategy.h"
#include "../PokerLibrary/binstorage.h"
#include "../HandIndexingV1/getindex2N.h"
#include "../PokerLibrary/cardmachine.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/constants.h"
#define TIXML_USE_TICPP
#include "../TinyXML++/ticpp.h"

//loads a new strategy, reads xml
Strategy::Strategy(string xmlfilename) : 
	tree(NULL),
	cardmach(NULL),
	dataoffset(4, vector<int64>(MAX_NODETYPES, 0)), // array of (4 x MAX_NODETYPES) 0's
	actionmax(4, vector<int>(MAX_NODETYPES, 0)) // 2D array of ints all zero
{
	using namespace ticpp;
	try //catches errors from xml parsing
	{
		Document doc(xmlfilename);
		doc.LoadFile();

		//check version

		Element* root = doc.FirstChildElement("strategy");
		if(!root->HasAttribute("version") || root->GetAttribute<int>("version") != SAVE_FORMAT_VERSION)
			REPORT("Unsupported XML file (older/newer version).");

		//set CardMachine

		Element* cardbins = root->FirstChildElement("cardbins");
		cardsettings_t cardsettings;
		cardsettings.usehistory = cardbins->FirstChildElement("meta")->GetAttribute<bool>("usehistory");
		cardsettings.useflopalyzer = cardbins->FirstChildElement("meta")->GetAttribute<bool>("useflopalyzer");
		for(int gr=0; gr<4; gr++)
		{
			ostringstream roundname;
			roundname << "round" << gr;
			Element* mybin = cardbins->FirstChildElement(roundname.str());
			cardsettings.filename[gr] = mybin->GetTextOrDefault("");
			cardsettings.filesize[gr] = mybin->GetAttribute<uint64>("filesize");
			cardsettings.bin_max[gr] = mybin->GetAttribute<int>("nbins");
		}
		cardmach = new CardMachine(cardsettings, false);

		//set BettingTree

		treesettings_t treesettings;
		Element* xmltree = doc.FirstChildElement("strategy")->FirstChildElement("tree");
		treesettings.sblind = xmltree->FirstChildElement("blinds")->GetAttribute<int>("sblind");
		treesettings.bblind = xmltree->FirstChildElement("blinds")->GetAttribute<int>("bblind");
		for(int i=0; i<6; i++)
		{
			ostringstream BN;
			BN << "B" << i+1;
			treesettings.bets[i] = xmltree->FirstChildElement("bets")->GetAttribute<int>(BN.str()); 
			for(int j=0; j<6; j++)
			{
				ostringstream raiseN, RNM;
				raiseN << "raise" << i+1;
				RNM << "R" << i+1 << j+1;
				treesettings.raises[i][j] = xmltree->FirstChildElement(raiseN.str())->GetAttribute<int>(RNM.str());
			}
		}
		treesettings.stacksize = xmltree->FirstChildElement("meta")->GetAttribute<int>("stacksize");
		treesettings.pushfold = xmltree->FirstChildElement("meta")->GetAttribute<bool>("pushfold");
		treesettings.limit = xmltree->FirstChildElement("meta")->GetAttribute<bool>("limit");
		tree = new BettingTree(treesettings);

		//set action max

		GetTreeSize(*tree, actionmax);

		//set strategy file

		Element* strat = doc.FirstChildElement("strategy")->FirstChildElement("solver")->FirstChildElement("savefile");
		if(strat->GetAttribute<uint64>("filesize") == 0)
			REPORT("the strategy file was not saved for this xml file.");
		strategyfile.open((strat->GetText()).c_str(), ifstream::binary);
		if(!strategyfile.good() || !strategyfile.is_open())
			REPORT("strategy file not found");
		strategyfile.seekg(0, ios::end);
		if(!strategyfile.good() || (uint64)strategyfile.tellg()!=strat->GetAttribute<uint64>("filesize"))
			REPORT("strategy file found but not correct size");
	}
	catch(Exception &ex)
	{
		REPORT(ex.what());
	}

	//load strategy file offsets

	strategyfile.seekg(0);
	for(int gr=0; gr<4; gr++)
	{
		int64 lastoffset = 0;
		for(int i=7; i>=0; i--) //loops must go in same order as done in memorymgr.
		{
			int64 thisoffset;
			strategyfile.read((char*)&thisoffset, 8);
			if(thisoffset < lastoffset)
				REPORT("Strategy file is corrupt (offsets must grow)");
			dataoffset[gr][i] = thisoffset;
			lastoffset = thisoffset;
		}
	}
	if(dataoffset[0][7] != strategyfile.tellg() || dataoffset[0][7] != 4*MAX_NODETYPES*8 || !strategyfile.good())
		REPORT("Strategy flie is corrupt (first data offset not as expected)");
}

Strategy::~Strategy()
{
	strategyfile.close();
	delete cardmach;
	delete tree;
}

void Strategy::getprobs(int gr, int actioni, int numa, const vector<CardMask> &cards, vector<double> &probs)
{

	//get the cardi 

	int cardsi = cardmach->getcardsi(gr, cards);

	//read the char probabilities, matching the offset to the memorymgr code.
	
	unsigned char charprobs[MAX_ACTIONS];
	//seekg ( offset + combinedindex * sizeofeachdata )
	strategyfile.seekg( dataoffset[gr][numa-2] + combine(cardsi, actioni, actionmax[gr][numa-2]) * (int64)numa );
	strategyfile.read((char*)charprobs, numa);
	if(strategyfile.gcount()!=numa || strategyfile.eof())
		REPORT("strategy file has failed us.");

	//convert the numa chars to numa doubles

	probs.resize(numa);
	int sum=0;
	for(int i=0; i<numa; i++)
		sum += charprobs[i];
	for(int i=0; i<numa; i++)
		probs[i] = (double)charprobs[i]/sum;
}
