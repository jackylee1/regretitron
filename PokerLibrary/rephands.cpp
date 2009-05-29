#include "stdafx.h"
#include "rephands.h"
#include "determinebins.h"
#include "flopalyzer.h"
#include "treenolimit.h"
#include "constants.h"
using namespace std;

//this function takes the hand index (which is the bin number) of a
//preflop hand (preflop bins are same as 2-card index: 0-168) and 
//returns a single random hand that fits that.
//this and the 3 below are used by the diagnostic window in 
//PokerPlayer and printpreflophand (below).
CardMask findpreflophand(int index)
{
	CardMask priv, usedcards;
	CardMask_RESET(usedcards);

	while(1)
	{
		//deal out the cards randomly.
		MONTECARLO_N_CARDS_D(priv, usedcards, 2, 1, );
		if (getpreflopbin(priv)==index)
			return priv;
	}
}

//these 3 functions take a boardi and a handi (boardalyzer and bin number, 
//that is) and returns a single random hole cards and board that fit it
//these and the one above are used by the diagnostic window in 
//PokerPlayer and printxxxxhands (below).
void findflophand(int bin, int flopscore, CardMask &priv, CardMask &flop)
{
	CardMask usedcards;
	CardMask_RESET(usedcards);
	
	while(1)
	{
		//first find a flop
		MONTECARLO_N_CARDS_D(flop, usedcards, 3, 1, );
		if(flopalyzer(flop) == flopscore)
			break;
	}

	while(1)
	{
		//now find our private cards.
		MONTECARLO_N_CARDS_D(priv, flop, 2, 1, );
		if(getflopbin(priv, flop) == bin)
			break;
	}
}

void findturnhand(int bin, int turnscore, CardMask &priv, CardMask &flop, CardMask &turn)
{
	CardMask usedcards;
	CardMask_RESET(usedcards);
	
	while(1)
	{
		//first find a flop and turn
		MONTECARLO_N_CARDS_D(flop, usedcards, 3, 1, );
		MONTECARLO_N_CARDS_D(turn, flop, 1, 1, );
		if(turnalyzer(flop,turn) == turnscore)
			break;
	}

	CardMask_OR(usedcards, flop, turn);

	while(1)
	{
		//now find our private cards.
		MONTECARLO_N_CARDS_D(priv, usedcards, 2, 1, );
		if(getturnbin(priv,flop,turn) == bin)
			break;
	}
}

void findriverhand(int bin, int riverscore, CardMask &priv, CardMask &flop, CardMask &turn, CardMask &river)
{
	CardMask usedcards;
	CardMask_RESET(usedcards);
	
	while(1)
	{
		//first find a flop and turn and river
		CardMask_RESET(usedcards);
		MONTECARLO_N_CARDS_D(flop, usedcards, 3, 1, );
		MONTECARLO_N_CARDS_D(turn, flop, 1, 1, );
		CardMask_OR(usedcards, flop, turn);
		MONTECARLO_N_CARDS_D(river, usedcards, 1, 1, );
		if(rivalyzer(flop,turn,river) == riverscore)
			break;
	}

	CardMask_OR(usedcards, usedcards, river);

	while(1)
	{
		//now find our private cards.
		MONTECARLO_N_CARDS_D(priv, usedcards, 2, 1, );
		if(getriverbin(priv,flop,turn,river) == bin)
			break;
	}
}

//this function takes a handi for preflop (so that's the raw 2-card-index)
//calls findpreflophand (above) to get a random matching hand,
//and prints the hand in common 2 character plus 's' for suited
//format, to the given stream
//this and the next three are only used by printsceniinfo in this file.
string preflophandstring(int index)
{
	string ret("");
	char * mask;
	CardMask hand = findpreflophand(index);

	mask = GenericDeck_maskString(&StdDeck, &hand);

	ret += mask[0]; //first rank
	ret += mask[3]; //second rank
	if(ret[0] < ret[1]) std::swap(ret[0], ret[1]);
	if(mask[1] == mask[4]) ret += 's'; //suited

	return ret;
}

//the following functions take a handi and boardi (bin number and 
//boardalyzer score) calls findxxxxhand (above), and prints out the hand
//it does that the given n number of times to the given stream.
//they and the one above are only used by printsceniinfo in this file.
void printflophands(ostream &out, int bin, int flopscore, int n)
{
	CardMask priv, flop;
	for(int i=0; i<n; i++)
	{
		findflophand(bin, flopscore, priv, flop);
		out << "  " << PRINTMASK(priv) << " : ";
		out << PRINTMASK(flop) << endl;
	}
}

void printturnhands(ostream &out, int bin, int turnscore, int n)
{
	CardMask priv, flop, turn;
	for(int i=0; i<n; i++)
	{
		findturnhand(bin, turnscore, priv, flop, turn);
		out << "  " << PRINTMASK(priv) << " : ";
		out << PRINTMASK(flop) << " : ";
		out << PRINTMASK(turn) << endl;
	}
}

void printriverhands(ostream &out, int bin, int riverscore, int n)
{
	CardMask priv, flop, turn, river;
	for(int i=0; i<n; i++)
	{
		findriverhand(bin, riverscore, priv, flop, turn, river);
		out << "  " << PRINTMASK(priv) << " : ";
		out << PRINTMASK(flop) << " : ";
		out << PRINTMASK(turn) << " : ";
		out << PRINTMASK(river) << endl;
	}
}


//takes an action index, the gameround, a pointer to the relevant
//betting tree node, and a multiplier.
//returns a string that reports info on what that action represents.
//it's units are the native ones for the poker engine (SB = 1) times
//the multiplier.
//used by diagnostics window (in PokerPlayer) 
//and by printfirstnodestrat (savestrategy.cpp of PokerNoLimit)
string actionstring(int gr, int action, const betnode &bn, double multiplier)
{
	stringstream str;
	str << fixed << setprecision(2);

	switch(bn.result[action])
	{
	case FD:
		str << "Fold";
		break;
	case GO:
		if(bn.potcontrib[action]==0 || (gr==PREFLOP && bn.potcontrib[action]==BB))
			str << "Check";
		else
			str << "Call $" << multiplier*(bn.potcontrib[action]);
		break;
	case AI:
		str << "Call All-In";
		break;
	default:
		if(isallin(bn.result[action], bn.potcontrib[action], gr))
			str << "All-In";
		else if(bn.potcontrib[action]==0)
			str << "Check";
		else if(gr==PREFLOP && bn.potcontrib[action]==BB)
			str << "Call $" << multiplier*(bn.potcontrib[action]);
		else
			str << "Bet $" << multiplier*(bn.potcontrib[action]);
		break;
	}

	return str.str();
}

//takes a floating point pot value and returns the nearest integer
//which is also a multiple of big blinds.
//used by BotAPI and diagnostics window (both in PokerPlayer)
int roundtobb(double pot)
{
	return (int)(pot/(double)(BB)+0.5) * BB;
}

//takes a cardmask with 2 or 3 cards in it, and returns
//a randomly ordered array of 2 or 3 cardmasks with 1 card in each. 
//used by diagnostics window only (in PokerPlayer)
void decomposecm(CardMask in, CardMask out[])
{
	int j=0;
	for(int i=0; i<52; i++)
		if (StdDeck_CardMask_CARD_IS_SET(in, i))
			out[j++]=StdDeck_MASK(i);

	if(j!=2 && j!=3)
		REPORT("not 2 or 3 cards in cm in decompose cm");

	//this is so stupid
	//choose one to be first
	std::swap(out[0], out[mersenne.randInt(j-1)]);
	//choose one to be second
	std::swap(out[1], out[mersenne.randInt(j-2)+1]);
}

//takes a cardmask assumed to have 1 card in it and returns
// a string that corresponds to the filename of the card image
// for that card.
//Bonus! If the cardmask is empty, this returns 53.png, which 
//is the joker!!!! (sweet)
//used by the GUI's: diagnostics window (in PokerPlayer) and by 
//the game window (in SimpleGame).
string cardfilename(CardMask m)
{
	int i;
	ostringstream ret;
	for(i=0; i<52; i++)
		if (StdDeck_CardMask_CARD_IS_SET(m, i)) break;
	i = 1 + (12-StdDeck_RANK(i))*4 + StdDeck_SUIT(i);
	ret << "cards/" << i << ".png";
	return ret.str();
}
	
