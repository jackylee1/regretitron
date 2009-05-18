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
void printpreflophand(ostream &out, int index)
{
	char ret[4];
	char * mask;
	CardMask hand = findpreflophand(index);

	mask = GenericDeck_maskString(&StdDeck, &hand);

	ret[0] = mask[0]; //first rank
	ret[1] = mask[3]; //second rank
	if(ret[0] < ret[1]) std::swap(ret[0], ret[1]);
	ret[2] = (mask[1] == mask[4]) ? 's' : ' '; //suited or not.
	ret[3] = '\0';

	out << "  " << ret << endl;
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

//this function takes a scenario index.
//it outputs the gameround, the pot index, the board index (boardalyzer 
//score), the hand index (the bin number), and the bet history.
//if any of these values are not used, as they may be depending on the
//gameround, it sets them to -1.
//used by the diagnostics window (in PokerPlayer) and by printsceniinfo (this file)
void getindices(int sceni, int &gr, int &poti, int &boardi, int &handi, int bethist[3])
{
	//default value if not used.
	bethist[0]=bethist[1]=bethist[2]=-1;

	if(sceni<0)
	{
		REPORT("Invalid sceni given to getindices");
	}
	if(sceni<SCENI_PREFLOP_MAX)
	{
		gr = PREFLOP;
		poti=-1;
		boardi=-1;
		handi=sceni;
	}
	else if(sceni<SCENI_FLOP_MAX)
	{
		gr = FLOP;
		sceni-=SCENI_PREFLOP_MAX;
		bethist[0] = sceni % BETHIST_MAX;
		poti       =(sceni/BETHIST_MAX) % POTI_FLOP_MAX;
		boardi     =(sceni/BETHIST_MAX/POTI_FLOP_MAX) % FLOPALYZER_MAX;
		handi      = sceni/BETHIST_MAX/POTI_FLOP_MAX/FLOPALYZER_MAX;
	}
	else if(sceni<SCENI_TURN_MAX)
	{
		gr = TURN;
		sceni-=SCENI_FLOP_MAX;
		bethist[0] = sceni % BETHIST_MAX;
		bethist[1] =(sceni/BETHIST_MAX) % BETHIST_MAX;
		poti       =(sceni/BETHIST_MAX/BETHIST_MAX) % POTI_TURN_MAX;
		boardi     =(sceni/BETHIST_MAX/BETHIST_MAX/POTI_TURN_MAX) % TURNALYZER_MAX;
		handi      = sceni/BETHIST_MAX/BETHIST_MAX/POTI_TURN_MAX/TURNALYZER_MAX;
	}
	else if(sceni<SCENI_RIVER_MAX)
	{
		gr = RIVER;
		sceni-=SCENI_TURN_MAX;
		bethist[0] = sceni % BETHIST_MAX;
		bethist[1] =(sceni/BETHIST_MAX) % BETHIST_MAX;
		bethist[2] =(sceni/BETHIST_MAX/BETHIST_MAX) % BETHIST_MAX;
		poti       =(sceni/BETHIST_MAX/BETHIST_MAX/BETHIST_MAX) % POTI_RIVER_MAX;
		boardi     =(sceni/BETHIST_MAX/BETHIST_MAX/BETHIST_MAX/POTI_RIVER_MAX) % RIVALYZER_MAX;
		handi      = sceni/BETHIST_MAX/BETHIST_MAX/BETHIST_MAX/POTI_RIVER_MAX/RIVALYZER_MAX;
	}
	else 
	{
		REPORT("Invalid sceni given to getindices");
	}
}

//this function takes the current gameround and the pot index for that round,
//and returns the lowest and the highest pots that the engine
//would match to that pot index.
//it does that by assuming that a contiguous region of BB-multiple 
//pot values are assigned to a single pot index, and calling 
//the pot indexing function repeatedly to find that range. 
//both return values will compute to the same pot index
//used by the diagnostics window in PokerPlayer and printsceniinfo (below).
void resolvepoti(int gr, int poti, int &lower, int &upper)
{
	lower = upper = -1;

	for (int pot=BB; pot<STACKSIZE; pot+=BB) //increment pot in multiples of the BB
	{
		if (getpoti(gr, pot) == poti)
		{
			lower = pot;
			break;
		}
	}

	for (int pot=lower; pot<STACKSIZE; pot+=BB)
	{
		if (getpoti(gr, pot) == poti)
			upper = pot;
	}

	if(lower==-1 || upper==-1 || lower > upper) 
		REPORT("could not find pots to match pot index.")
}

//simple table of strings for bethistories.
//used by diagnostics window (in PokerPlayer) and resolvebethist(below)
string bethiststr(int bethist)
{
	switch(bethist)
	{
	case GO1-GO_BASE:
		return "check-check";
	case GO2-GO_BASE:
		return "bet-call";
	case GO3-GO_BASE:
		return "check-bet-call";
	case GO4-GO_BASE:
		return "bet-raise-call";
	case GO5-GO_BASE:
		return "check-bet-raise-call";
	default:
		REPORT("invalid betting history passed to bethiststr");
	}
}

//this function calls the ones above to print all information
//that the engine would know about a scenario index. to the 
//given stream
//used only by printfirstnodestrat in savestrategy.cpp (PokerNoLimit)
void printsceniinfo(ostream &out, int sceni, int n_hands)
{
	int gr, poti, boardi, handi, bethist[3];
	getindices(sceni, gr, poti, boardi, handi, bethist);
	int potlower, potupper;
	resolvepoti(gr, poti, potlower, potupper);

	out << "Known pot range: " << potlower << " - " << potupper << endl;

	if(gr==PREFLOP)
	{
		printpreflophand(out, handi);
	}
	else if(gr==FLOP)
	{
		out << "Betting log: " << bethiststr(bethist[0]);
		printflophands(out,handi,boardi,n_hands);
	}
	else if(gr==TURN)
	{
		out << "Betting log: " << bethiststr(bethist[0]) 
			<< " : " << bethiststr(bethist[1]) << endl;
		printturnhands(out,handi,boardi,n_hands);
	}
	else if(gr==RIVER)
	{
		out << "Betting log: " << bethiststr(bethist[0]) 
			<< " : " << bethiststr(bethist[1])
			<< " : " << bethiststr(bethist[2]) << endl;
		printriverhands(out,handi,boardi,n_hands);
	}
	else 
		REPORT("getindices returned an invalid gameround");
}


//takes an action index, the gameround, a pointer to the relevant
//betting tree node, and a multiplier.
//returns a string that reports info on what that action represents.
//it's units are the native ones for the poker engine (SB = 1) times
//the multiplier.
//used by diagnostics window (in PokerPlayer) 
//and by printfirstnodestrat (savestrategy.cpp of PokerNoLimit)
string actionstring(int action, int gr, betnode const * tree, double multiplier)
{
	stringstream str;
	str << fixed << setprecision(2);

	switch(tree->result[action])
	{
	case FD:
		str << "Fold";
		break;
	case GO1:
	case GO2:
	case GO3:
	case GO4:
	case GO5:
		if(tree->potcontrib[action]==0 || (gr==PREFLOP && tree->potcontrib[action]==BB))
			str << "Check";
		else
			str << "Call $" << multiplier*(tree->potcontrib[action]);
		break;
	case AI:
		str << "Call All-In";
		break;
	default:
		if(isallin(tree,gr,action))
			str << "All-In";
		else if(tree->potcontrib[action]==0)
			str << "Check";
		else
			str << "Bet $" << multiplier*(tree->potcontrib[action]);
		break;
	}

	return str.str();
}

//since by betting tree can be ambiguous, more extensive tests
//are needed to check if a node is a BET allin. this function takes
//a pointer to a betting tree node, the gameround, and an action index
//and tells you if it is a BET allin.
//used by BotAPI (in PokerPlayer) and actionstring (this file)
bool isallin(betnode const * mynode, int gr, int action)
{
	switch(mynode->result[action])
	{
	case FD:
	case GO1: 
	case GO2: 
	case GO3: 
	case GO4: 
	case GO5: 
	case AI: 
	case NA:
		return false;
	}

	if(mynode->potcontrib[action] != 0)
		return false;

	//must check to see if child node has 2 actions
	betnode const * child;
	child = (gr==PREFLOP) ? pfn : n;
	if(child[mynode->result[action]].numacts == 2)
		return true;
	else
		return false;
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
	
