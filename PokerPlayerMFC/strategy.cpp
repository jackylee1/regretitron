#include "stdafx.h"
#include "strategy.h"
#include "../PokerLibrary/binstorage.h"
#include "../HandIndexingV1/getindex2N.h"
#include "../PokerLibrary/cardmachine.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/constants.h"
#ifndef TIXML_USE_TICPP
#define TIXML_USE_TICPP
#endif
#include "../TinyXML++/ticpp.h"

#ifdef _MSC_VER
#define system(x) 0
#endif

//loads a new strategy, reads xml
Strategy::Strategy(string xmlfilename, bool preload, LoggerClass & strategylogger) : 
	_xmlfilename(xmlfilename),
	tree(NULL),
	cardmach(NULL),
	dataoffset(4, vector<int64>(MAX_NODETYPES, 0)) // array of (4 x MAX_NODETYPES) 0's
{
	const string olddirectory = getdirectory();
	
	using namespace ticpp;
	int64 strategyfilesize;
	try //catches errors from xml parsing
	{
		//load the xml and then...
		Document doc(xmlfilename);
		doc.LoadFile();

		//...set working directory to xmlfilename directory
		string newdirectory = xmlfilename.substr(0,xmlfilename.find_last_of("/\\"));
		if(newdirectory != xmlfilename) //has directory info
			setdirectory(xmlfilename.substr(0,xmlfilename.find_last_of("/\\")));

		//check version

		Element* root = doc.FirstChildElement("strategy");
		if(!root->HasAttribute("version") || root->GetAttribute<int>("version") != SAVE_FORMAT_VERSION)
			REPORT("Unsupported XML file (older/newer version).", KNOWN);

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
			if(preload && cardsettings.filename[gr].length()>0)
				if(system(("cat "+cardsettings.filename[gr]+".bins > /dev/null").c_str()) != 0)
					REPORT("cat something failed");
			cardsettings.filesize[gr] = mybin->GetAttribute<int64>("filesize");
			cardsettings.bin_max[gr] = mybin->GetAttribute<int>("nbins");
		}

		//remain backwards compatible if the xml file doesn't have useboardbins
		cardsettings.useboardbins = 
			cardbins->FirstChildElement("meta")->HasAttribute("useboardbins")
			&& cardbins->FirstChildElement("meta")->GetAttribute<bool>("useboardbins");
		for(int gr=0; gr<4; gr++)
		{
			if( cardsettings.useboardbins )
			{
				ostringstream roundname;
				roundname << "boardround" << gr;
				Element* mybin = cardbins->FirstChildElement(roundname.str());
				cardsettings.boardbinsfilename[gr] = mybin->GetTextOrDefault("");
				if(preload && cardsettings.filename[gr].length()>0)
					if(system(("cat "+cardsettings.filename[gr]+".bins > /dev/null").c_str()) != 0)
						REPORT("cat something failed");
				cardsettings.boardbinsfilesize[gr] = mybin->GetAttribute<int64>("filesize");
				cardsettings.board_bin_max[gr] = mybin->GetAttribute<int>("nbins");
			}
			else
			{
				cardsettings.boardbinsfilename[gr] = "";
				cardsettings.boardbinsfilesize[gr] = 0;
				cardsettings.board_bin_max[gr] = 0;
			}
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
		treeroot = createtree(*tree, MAX_ACTIONS, strategylogger);

		//set strategy file

		Element* strat = doc.FirstChildElement("strategy")->FirstChildElement("solver")->FirstChildElement("savefile");
		strategyfilesize = strat->GetAttribute<int64>("filesize");
		if(strategyfilesize == 0)
			REPORT("the strategy file was not saved for this xml file.", KNOWN);
		if(preload)
			if(0!=system(("cat "+strat->GetText()+" > /dev/null").c_str()))
				REPORT("cat strategy failed");
		strategyfile.Open(strat->GetText(), strategyfilesize);
	}
	catch(ticpp::Exception &ex)
	{
		REPORT(ex.what(),KNOWN);
		exit(-1);
	}

	//load strategy file offsets

	strategyfile.Seek(0);
	int64 nextoffset = 4*MAX_NODETYPES*8; //should be start of data
	for(int gr=0; gr<4; gr++)
	{
		for(int n=MAX_ACTIONS; n>=2; n--) //loops must go in same order as done in memorymgr.
		{
			int64 thisoffset = strategyfile.Read<int64>();
			if(thisoffset != nextoffset)
				REPORT("redundant offset storage has revealed errors in strategy file");
			dataoffset[gr][n-2] = thisoffset;
			nextoffset = thisoffset + get_property(gettree(), maxorderindex_tag())[gr][n] * static_cast< int64 >( cardmach->getcardsimax(gr) ) * static_cast< int64 >(n+1);
		}
	}
	if(dataoffset[0][MAX_NODETYPES-1] != strategyfile.Tell() || strategyfilesize != nextoffset)
		REPORT("Strategy file failed final error checks.");
	setdirectory(olddirectory);
}

Strategy::~Strategy()
{
	delete cardmach;
	delete tree;
}

void Strategy::getprobs(int gr, int actioni, int numa, const vector<CardMask> &cards, vector<double> &probs)
{

	//get the cardi 

	int cardsi = cardmach->getcardsi(gr, cards);

	getprobs(gr, actioni, numa, cardsi, probs);
}

void Strategy::getprobs(int gr, int actioni, int numa, int cardsi, vector<double> &probs)
{

	//read the char probabilities, matching the offset to the memorymgr code.
	
	unsigned char charprobs[MAX_ACTIONS];
	unsigned char checksum;
	//seek ( offset + combinedindex * sizeofeachdata )
	strategyfile.Seek( dataoffset[gr][numa-2] + combine(cardsi, actioni, get_property(gettree(), maxorderindex_tag())[gr][numa]) * (numa+1) );
	for(int i=0; i<numa; i++)
		charprobs[i] = strategyfile.Read<unsigned char>();
	checksum = strategyfile.Read<unsigned char>();

	//convert the numa chars to numa doubles

	probs.resize(numa);
	unsigned int sum=0;
	for(int i=0; i<numa; i++)
		sum += charprobs[i];
	if((sum & 0x000000FF) != checksum)
		REPORT("checksum failed in strategy file!");
	for(int i=0; i<numa; i++)
		probs[i] = (double)charprobs[i]/sum;
}
