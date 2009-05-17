#include "stdafx.h"
#include "botapi.h"
#include "lookupanswer.h"
#include "../PokerLibrary/determinebins.h"
#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/flopalyzer.h"
#include "../PokerLibrary/treenolimit.h"
#include "dodiagnostics.h"

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// ----------------- Class Constructors and Destructors ------------------

BotAPI::BotAPI(bool diagon)
   : isdiagnosticson(diagon), mynode(NULL), answer(-1)
{
	//init the preflop tree
	initpfn();
	//init the bins for determinebins
	initbins();
}

BotAPI::~BotAPI()
{
	//close the bins
	closebins();
	//kill the diagnostics window, if any
	destroywindow();
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// ------ Public function (the only one) to handle diagnostics window ----------

//turns diagnostics window on or off
//this is the only interaction users of BotAPI have with this window.
void BotAPI::setdiagnostics(bool onoff)
{
	//if state hasn't changed, don't care
	if(isdiagnosticson == onoff) return;
	//update state
	isdiagnosticson = onoff;
	//handle new state
	if(isdiagnosticson && mynode!=NULL && mynode->playertoact==myplayer)
		populatewindow(); //will create window if not there
	else if(!isdiagnosticson)
		destroywindow(); //will do nothing if window not there
}


// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// ----------- Public functions to handle Inputs: new games ----------------

void BotAPI::setnewgame(Player playernum, CardMask hand, 
						double sblind, double bblind, double stacksize)
{
	//first we check the inputs, necesarily setting the multiplier

	if(playernum != P0 && playernum != P1)
		REPORT("invalid myplayer number in BotAPI::setmyhand()");
	multiplier = bblind / BB;
	if(stacksize/multiplier != STACKSIZE)
		REPORT("invalid stacksize amount in BotAPI::setmyhand()");

	//Now, set the state for base class GameState

	priv0 = hand;
	myplayer = playernum;
	binnumber[myplayer][PREFLOP] = getpreflopbin(priv0);

	//We go through the list of our private data and update each.

	//myplayer set above
	currentgr = PREFLOP;
	currentpot = 0.0;
	invested[0] = bblind/multiplier;
	invested[1] = sblind/multiplier;
	perceived[0] = BB;
	perceived[1] = SB;
	//multiplier already set above
	currentsceni = getscenarioi(PREFLOP, myplayer, 0, NULL);
	currentbetinode = 0;
	mynode = pfn;
	bethist[0] = bethist[1] = bethist[2] = -1;

	offtreebetallins = false;
	answer = -1;
	processmyturn();
}


// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// -------- Public functions to handle Inputs: gameround changes -----------

void BotAPI::setflop(CardMask theflop, double newpot)
{
	//check input parameters

	if (currentgr != PREFLOP) REPORT("you set the flop at the wrong time");
	if (currentbetinode != -1) REPORT("you must advancetree before you setflop");
	if (invested[0] != invested[1]) REPORT("you both best be betting the same.");
	if (perceived[0] != perceived[1]) REPORT("perceived state is messed up");
	if (currentpot + invested[0] != newpot/multiplier) REPORT("your new pot is unexpected.");
	if (newpot/multiplier < currentpot || newpot/multiplier < BB) REPORT("newpot is too small.");

	//Set state of baseclass GameState

	flop = theflop;
	flopscore = flopalyzer(flop);
	binnumber[myplayer][FLOP] = getflopbin(priv0,flop);

	//Update the state of our private data members

	currentgr++;
	currentpot = newpot/multiplier;
	invested[0] = 0;
	invested[1] = 0;
	perceived[0] = 0;
	perceived[1] = 0;
	currentsceni = getscenarioi(FLOP, myplayer, (int)(newpot/multiplier+0.5), bethist);
	currentbetinode = 0; //ready for P0 to act
	mynode = n;
	processmyturn();
}

void BotAPI::setturn(CardMask theturn, double newpot)
{
	//check input parameters

	if (currentgr != FLOP) REPORT("you set the turn at the wrong time");
	if (currentbetinode != -1) REPORT("you must advancetree before you setturn");
	if (invested[0] != invested[1]) REPORT("you both best be betting the same.");
	if (perceived[0] != perceived[1]) REPORT("perceived state is messed up");
	if (currentpot + invested[0] != newpot/multiplier) REPORT("your new pot is unexpected.");
	if (newpot/multiplier < currentpot || newpot/multiplier < BB) REPORT("pot is too small.");

	//set state of GameState

	turn = theturn;
	turnscore = turnalyzer(flop, turn);
	binnumber[myplayer][TURN] = getturnbin(priv0,flop,turn);

	//set state of BotAPI

	currentgr++;
	currentpot = newpot/multiplier;
	invested[0] = 0;
	invested[1] = 0;
	perceived[0] = 0;
	perceived[1] = 0;
	currentsceni = getscenarioi(TURN, myplayer, (int)(newpot/multiplier+0.5), bethist);
	currentbetinode = 0; //ready for P0 to act
	mynode = n;
	processmyturn();
}

void BotAPI::setriver(CardMask theriver, double newpot)
{
	//check input parameters

	if (currentgr != TURN) REPORT("you set the river at the wrong time");
	if (currentbetinode != -1) REPORT("you must advancetree before you setriver");
	if (invested[0] != invested[1]) REPORT("you both best be betting the same.");
	if (perceived[0] != perceived[1]) REPORT("perceived state is messed up");
	if (currentpot + invested[0] != newpot/multiplier) REPORT("your new pot is unexpected.");
	if (newpot/multiplier < currentpot || newpot/multiplier < BB) REPORT("pot is too small.");

	//set state of GameState

	river = theriver;
	riverscore = rivalyzer(flop, turn, river);
	binnumber[myplayer][RIVER] = getriverbin(priv0,flop,turn,river);

	//set state of BotAPI

	currentgr++;
	currentpot = newpot/multiplier;
	invested[0] = 0;
	invested[1] = 0;
	perceived[0] = 0;
	perceived[1] = 0;
	currentsceni = getscenarioi(RIVER, myplayer, (int)(newpot/multiplier+0.5), bethist);
	currentbetinode = 0; //ready for P0 to act
	mynode = n;
	processmyturn();
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// -------------- Inputs: betting actions (including our own) -----------------

//wrapper function to simplify logic
void BotAPI::advancetree(Player pl, Action a, double amount)
{
	switch(a)
	{
	case FOLD:  REPORT("The bot does not need to know about folds");
	case CALL:  docall (pl, amount/multiplier); break;
	case BET:   dobet  (pl, amount/multiplier); break;
	case ALLIN: doallin(pl);         break;
	default:    REPORT("You advanced tree with an invalid action.");
	}

	processmyturn();
}

//private functions that actually handles advancing the tree
//we must determine the available action that best matches 
//the given decision, and then advance the currentbetinode to 
//that decision.

//a "call" by definition ends the betting round
void BotAPI::docall(Player pl, double amount)
{
	if(pl != mynode->playertoact) REPORT("advancetree thought the other player should be acting");
	if(amount != invested[1-pl]) REPORT("you called the wrong amount");

	//only neccesary line
	bethist[currentgr] = getbethist();

	invested[pl] = invested[1-pl];
	perceived[pl] = perceived[1-pl];
	currentbetinode = -1;
	mynode = NULL;
}

//a "bet" by definition continues the betting round
void BotAPI::dobet(Player pl, double amount)
{
	if(pl != mynode->playertoact) REPORT("advancetree thought the other player should be acting");
	if(amount < mintotalwager() || amount >= STACKSIZE)
		REPORT("Invalid bet amount");

	//these nodes do not have a betting action, so we use allin.
	if (mynode->numacts == 3)
	{
		if(pl == myplayer) REPORT("bot should be unable to bet from a 3-membered node")

		offtreebetallins = true;
		doallin(pl);
		return;
	}

	int nextact = getbestbetact(amount);
	if(nextact < 0 || nextact >=9) REPORT("could not find betting node.");

	switch(mynode->result[nextact])
	{
	case FD: case GO1: case GO2: case GO3: case GO4: case GO5: case AI: case NA:
		REPORT("we expected a betting action");
	}

	if (mynode->potcontrib[nextact] == 0)
		REPORT("we expected a betting action but got all-in");

	//ok, let's accept the answer and do it.
	invested[pl] = amount;
	perceived[pl] = mynode->potcontrib[nextact];
	currentbetinode = mynode->result[nextact];
	mynode = (currentgr == PREFLOP ? pfn : n) + currentbetinode;
}

//this is a "bet" of amount all-in
void BotAPI::doallin(Player pl)
{
	if(pl != mynode->playertoact) REPORT("advancetree thought the other player should be acting");

	int nextact = getallinact();

	if(nextact < 0 || nextact >=9) REPORT("could not find all-in node.");
	if(!isallin(nextact))
		REPORT("we did not find a BET all-in");

	//ok, let's accept the answer and do it.
	invested[pl] = STACKSIZE-currentpot;
	perceived[pl] = STACKSIZE-currentpot;
	currentbetinode = mynode->result[nextact];
	mynode = (currentgr == PREFLOP ? pfn : n) + currentbetinode;
}

// ----------------------------------------------------------------------------
// ---- Private helper functions for advancing the tree on betting actions ----
		
bool BotAPI::isallin(int a)
{
	switch(mynode->result[a])
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

	if(mynode->potcontrib[a] != 0)
		return false;

	//must check to see if child node has 2 actions
	betnode const * child;
	child = (currentgr==PREFLOP) ? pfn : n;
	if(child[mynode->result[a]].numacts == 2)
		return true;
	else
		return false;
}

int BotAPI::getbestbetact(double betsize)
{
	int bestaction=-1;
	double besterror = std::numeric_limits<double>::infinity();

	for(int a=0; a<mynode->numacts; a++)
	{
		switch(mynode->result[a])
		{
		case FD: case GO1: case GO2: case GO3: case GO4: case GO5: case AI: case NA:
			continue;
		default:
			if(mynode->potcontrib[a]==0)
				continue; //this is a bet-all-in then

			double error = abs( (double)(mynode->potcontrib[a]) - betsize );
			if(error < besterror)
			{
				bestaction = a;
				besterror = error;
			}
		}
	}

	if (bestaction == -1)
		REPORT("no betting actions found!");

	return bestaction;
}


int BotAPI::getallinact()
{
	int allinaction;
	int total=0;

	for(int a=0; a<mynode->numacts; a++)
	{
		if(isallin(a))
		{
			allinaction = a;
			total++;
		}
	}
	if (total != 1)
		REPORT("not exactly 1 all-in action found!");

	return allinaction;
}

//there should be just one call option from the current player's betnode
//he must have used that option, and we can get the bethist from it.
int BotAPI::getbethist()
{
	int bethist;
	int total=0;

	for(int a=0; a<mynode->numacts; a++)
	{
		switch(mynode->result[a])
		{
		case GO1: case GO2: case GO3: case GO4: case GO5:
			total++;
			bethist = mynode->result[a] - GO_BASE;
		}
	}

	if (total != 1)
		REPORT("not exactly 1 call action found for bethist.");

	return bethist;
}

// ------------------------------------------------------------------------------
// ------------------------------------------------------------------------------
// ------------------ Public function to access bot acction ---------------------

Action BotAPI::getanswer(double &amount)
{
	Action myact;

	if(myplayer != mynode->playertoact) REPORT("You asked for an answer when the bot thought action was on opp.");
	if(answer<0 || answer>=9) REPORT("Inconsistant BotAPI state.");


	//pass back the correct numbers to communicate our answer
	switch(mynode->result[answer])
	{
	case FD:
		amount = -1;
		myact = FOLD;
		break;

	case NA:
		REPORT("we chose an invalid action. no good guys.");

	case GO1:
	case GO2:
	case GO3:
	case GO4:
	case GO5:
		amount = multiplier * (mynode->potcontrib[answer] + invested[myplayer]-perceived[myplayer]);
		myact = CALL;
		break;

	case AI:
		if(!offtreebetallins) //as it should be
		{
			amount = 0;
			myact = CALL;
		}
		else //but sometimes, we treat these nodes as BET all in instead of call.
		{
			if(mynode->numacts != 2)
				REPORT("I thought we should be at a 2-membered node now! (fold or call all-in)");
			amount = 0;
			myact = ALLIN;
		}
		break;

	default:
		if (mynode->potcontrib[answer] == 0)
		{
			amount = 0;
			myact = ALLIN;
		}
		else
		{
			//so we actually bet or call the increment above what we really invested
			amount = multiplier * (mynode->potcontrib[answer] + invested[myplayer]-perceived[myplayer]);
			myact = BET;
		}
	}

	answer = -1;
	return myact;
}

// ------------------------------------------------------------------------------
// -------------- Private function to actually determine bot action -------------

//this would be the value used in production, but it may be overridden by the 
//diagnostics window
void BotAPI::processmyturn()
{
	double cumulativeprob, randomprob, actionprobs[9];

	if(mynode == NULL || mynode->playertoact != myplayer) return; //does not signify an error

	readstrategy(currentsceni, currentbetinode, actionprobs, mynode->numacts);

	//go through actions and choose one randomly with correct likelyhood.
	do{
		cumulativeprob = 0;
		randomprob = mersenne.randExc(); //generates the answer!
		answer = -1;
		for(int a=0; a<mynode->numacts; a++)
		{
			cumulativeprob += actionprobs[a];
			if (cumulativeprob > randomprob)
			{
				answer = a;
				break;
			}
		}
	}while(answer == -1);

	//lastly, update diagnostics window
	if(isdiagnosticson) populatewindow();
}


// ------------------------------------------------------------------------------
// ------------------------------------------------------------------------------
// -------------------- Any other private helper functions ----------------------

double BotAPI::mintotalwager()
{
	Player acting = (Player)mynode->playertoact;
	//calling from the SBLIND is a special case of how much we can wager
	if(currentgr == PREFLOP && invested[acting]<(double)BB)
		return (double)BB;
	
	//otherwise, this is the standard formula that FullTilt seems to follow
	double prevwager = invested[acting]-invested[1-acting];
	return invested[acting] + max((double)BB, prevwager);
}
