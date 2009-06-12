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

		//set CardMachine

		cardsettings_t cardsettings;
		for(int gr=0; gr<4; gr++)
		{
			ostringstream roundname;
			roundname << "round" << gr;
			Element* bin = doc.FirstChildElement("paramsv1")->FirstChildElement("bins")->FirstChildElement(roundname.str());
			cardsettings.filename[gr] = bin->GetTextOrDefault("");
			cardsettings.filesize[gr] = bin->GetAttribute<uint64>("filesize");
			cardsettings.bin_max[gr] = bin->GetAttribute<int>("nbins");
		}
		cardmach = new CardMachine(cardsettings, false);

		//set BettingTree

		treesettings_t treesettings;
		Element* xmltree = doc.FirstChildElement("paramsv1")->FirstChildElement("tree");
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
		treesettings.stacksize = xmltree->FirstChildElement("sspf")->GetAttribute<int>("stacksize");
		treesettings.pushfold = xmltree->FirstChildElement("sspf")->GetAttribute<bool>("pushfold");
		tree = new BettingTree(treesettings);

		//set action max

		GetTreeSize(*tree, actionmax);

		//set strategy file

		Element* strat = doc.FirstChildElement("paramsv1")->FirstChildElement("solver")->FirstChildElement("savefile");
		if(strat->GetAttribute<uint64>("filesize") == 0)
		{
			WARN("the strategy file was not saved for this xml file.");
			exit(-1);
		}
		strategyfile.open((strat->GetText()).c_str(), ifstream::binary);
		if(!strategyfile.good() || !strategyfile.is_open())
			REPORT("strategy file not found");
		strategyfile.seekg(0, ios::end);
		if(!strategyfile.good() || (uint64)strategyfile.tellg()!=strat->GetAttribute<uint64>("filesize"))
			REPORT("strategy file found but not correct size");
	}
	catch(Exception &ex)
	{
		WARN(ex.what());
		exit(-1);
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

void Strategy::getprobs(const betihist_t &bh, const vector<CardMask> &cards, vector<double> &probs)
{

	// get the action index

	Indexer myindexer(bh, this);
	pair<int,int> numaacti = myindexer.getindex();
	const int &numa = numaacti.first;
	const int &actioni = numaacti.second;
	const int &gr = bh.back().first;

	//get the cardi 

	int cardsi = cardmach->getcardsi(gr, cards); //bh.back().first is the current gr

	//read the char probabilities, matching the offset to the memorymgr code.
	
	unsigned char charprobs[MAX_ACTIONS-1];
	strategyfile.seekg( dataoffset[gr][numa-2] + COMBINE(cardsi, actioni, actionmax[gr][numa-2]) * (numa-1) );
	strategyfile.read((char*)charprobs, numa-1);
	if(strategyfile.gcount()!=numa-1 || strategyfile.eof())
		REPORT("strategy file has failed us.");

	//convert the numa-1 chars to numa doubles

	probs.resize(numa);
	unsigned char sum=0;
	for(int i=0; i<numa-1; i++)
	{
		sum+=charprobs[i];
		probs[i] = (double)charprobs[i]/256.0;
	}
	probs[numa-1] = (double)(256-sum)/256.0;

}

Strategy::Indexer::Indexer(const betihist_t &betihist, Strategy * strat) :
	bh(betihist), 
	depth(0),
	counters(4, vector<int>(MAX_NODETYPES, 0)), //2D vector filled with 0's
	thisstrat(strat)
{
	if(bh[depth].first!=PREFLOP || bh[depth].second!=0)
		REPORT("invalid first node");
}

//pair is numa and actioni
pair<int,int> Strategy::Indexer::go(int gr, int pot, int beti)
{
	BetNode mynode;
	thisstrat->tree->getnode(gr, pot, beti, mynode);
	const int &numa = mynode.numacts; //for ease of typing

	if(++depth == bh.size())
		return pair<int,int>(numa, counters[gr][numa-2]); //need to get numa out using pair
	else
		counters[gr][numa-2]++;

	for(int a=0; a<numa; a++)
	{
		//depth points to the index in the vector of the NEXT
		//action we want to find. 
		switch(mynode.result[a])
		{
		case BetNode::NA:
				REPORT("invalid tree");
		case BetNode::FD: //no next action
		case BetNode::AI: //no next action
				continue;
		case BetNode::GO:
				if(gr!=RIVER)
				{
					if(bh[depth].first==gr+1 && bh[depth].second==0)
						return go(gr+1, pot+mynode.potcontrib[a], 0);
					else
						go(gr+1, pot+mynode.potcontrib[a], 0);
				}
				continue;

			default://child node
				if(bh[depth].first==gr && bh[depth].second==mynode.result[a])
					return go(gr, pot, mynode.result[a]);
				else
					go(gr, pot, mynode.result[a]);
				continue;
		}
	}

	REPORT("should have found a suitable action by now.");
}

