#include "stdafx.h"
#include "rephands.h"
#include "determinebins.h"
#include "flopalyzer.h"
#include "treenolimit.h"
#include "constants.h"
using namespace std;

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

void printpreflophands(ostream &out, int index)
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

//this function assumes that a contiguous region of BB-multiple pot values are assigned
//to a single pot index. The lowest matching pot will be set to lower, and the highest
//matching pot will be set to upper. Each are guaranteed to produce the given pot index 
//for this gr.
void resolvepoti(int gr, int poti, int &lower, int &upper)
{
	//in case of error, we won't REPORT, but rather just return -1.
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
}

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
		return "[invalid bethist]";
	}
}

//in a convoluted way, produces "preflopbets : flopbets : turnbets"
// or "preflopbets : flopbets"
// or "preflopbets"
string resolvebethist(int gr, int bethist)
{
	string ret;
	for(int i=gr; i>=TURN; i--)
	{
		ret.insert(0, bethiststr(bethist%BETHIST_MAX));
		ret.insert(0, " : ");
		bethist /= BETHIST_MAX;
	}
	if(gr!=PREFLOP)
		ret.insert(0, bethiststr(bethist%BETHIST_MAX));
	return ret;
}

//tells you about a sceni
void printrepinfo(ostream &out, int sceni, int n_hands)
{
	if(sceni<SCENI_PREFLOP_MAX)
	{
		printpreflophands(out, sceni);
	}
	else if(0<=sceni && sceni<SCENI_FLOP_MAX)
	{
		int potlower, potupper, handbin, boardscore;
		sceni-=SCENI_PREFLOP_MAX;

		//resolve the betting
		out << "Betting log: " << resolvebethist(FLOP, sceni%BETHIST_MAX) << endl;
		sceni/=BETHIST_MAX;

		//resolve the pot
		resolvepoti(FLOP, sceni%POTI_FLOP_MAX, potlower, potupper);
		out << "Known pot range: " << potlower << " - " << potupper << endl;
		sceni/=POTI_FLOP_MAX;

		//resolve some example hands
		boardscore = sceni%FLOPALYZER_MAX;
		handbin = sceni/FLOPALYZER_MAX;
		printflophands(out,handbin,boardscore,n_hands);
	}
	else if(sceni<SCENI_TURN_MAX)
	{
		int potlower, potupper, handbin, boardscore;
		sceni-=SCENI_PREFLOP_MAX+SCENI_FLOP_MAX;

		//resolve the betting
		out << "Betting log: " << resolvebethist(TURN, sceni%(BETHIST_MAX*BETHIST_MAX)) << endl;
		sceni/=BETHIST_MAX*BETHIST_MAX;

		//resolve the pot
		resolvepoti(TURN, sceni%POTI_TURN_MAX, potlower, potupper);
		out << "Known pot range: " << potlower << " - " << potupper << endl;
		sceni/=POTI_TURN_MAX;

		//resolve some example hands
		boardscore = sceni%TURNALYZER_MAX;
		handbin = sceni/TURNALYZER_MAX;
		printturnhands(out,handbin,boardscore,n_hands);
	}
	else if(sceni<SCENI_RIVER_MAX)
	{
		int potlower, potupper, handbin, boardscore;
		sceni-=SCENI_PREFLOP_MAX+SCENI_FLOP_MAX+SCENI_TURN_MAX;

		//resolve the betting
		out << "Betting log: " << resolvebethist(RIVER, sceni%(BETHIST_MAX*BETHIST_MAX*BETHIST_MAX)) << endl;
		sceni/=BETHIST_MAX*BETHIST_MAX*BETHIST_MAX;

		//resolve the pot
		resolvepoti(RIVER, sceni%POTI_RIVER_MAX, potlower, potupper);
		out << "Known pot range: " << potlower << " - " << potupper << endl;
		sceni/=POTI_RIVER_MAX;

		//resolve some example hands
		boardscore = sceni%RIVALYZER_MAX;
		handbin = sceni/RIVALYZER_MAX;
		printriverhands(out,handbin,boardscore,n_hands);
	}
	else 
		out << "INVALID SCENI: " << sceni << endl << endl;
}


string actionstring(int action, int gr, int beti)
{
	const betnode * tree = (gr == PREFLOP) ? pfn+beti : n+beti;
	stringstream str;
	string ret;

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
		str << "Call " << (int)(tree->potcontrib[action]) << "sb:";
		break;
	case AI:
		str << "Call All-In:";
		break;
	default:
		if(tree->potcontrib[action]==0)
			str << "Bet All-In:";
		else
			str << "Bet to " << (int)(tree->potcontrib[action]) << "sb:";
		break;
	}

	ret = str.str();
	ret.resize(15,' ');
	return ret;
}
