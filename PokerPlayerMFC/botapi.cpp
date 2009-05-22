#include "stdafx.h"
#include "botapi.h"
#include "lookupanswer.h"
#include "../PokerLibrary/determinebins.h"
#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/flopalyzer.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/rephands.h" // for isallin and roundtobb
#include "DiagnosticsPage.h" //to access radios for answer

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// ----------------- Class Constructors and Destructors ------------------

BotAPI::BotAPI(bool diagon)
   : isdiagnosticson(diagon), mynode(NULL), answer(-1), MyWindow(NULL)
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
void BotAPI::setdiagnostics(bool onoff, CWnd* parentwin)
{
	isdiagnosticson = onoff;

	if(isdiagnosticson)
		populatewindow(parentwin); //will create window if not there
	else
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
	currentsceni = getscenarioi(FLOP, myplayer, roundtobb(newpot/multiplier), bethist);
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
	currentsceni = getscenarioi(TURN, myplayer, roundtobb(newpot/multiplier), bethist);
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
	currentsceni = getscenarioi(RIVER, myplayer, roundtobb(newpot/multiplier), bethist);
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
	//in this case there would be no all-in node to find
	if(a == ALLIN && pl == myplayer && offtreebetallins) 
		return;

	switch(a)
	{
	case FOLD:  REPORT("The bot does not need to know about folds");
	case CALL:  docall (pl, amount/multiplier); break;
	case BET:   dobet  (pl, amount/multiplier); break;
	case ALLIN: doallin(pl);   break;
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
		if(pl == myplayer) REPORT("bot should be unable to bet from a 3-membered node");

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

	if (isallin(mynode,currentgr,nextact))
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
	if(!isallin(mynode,currentgr,nextact))
		REPORT("we did not find a BET all-in");

	//ok, let's accept the answer and do it.
	invested[pl] = STACKSIZE-currentpot;
	perceived[pl] = STACKSIZE-currentpot;
	currentbetinode = mynode->result[nextact];
	mynode = (currentgr == PREFLOP ? pfn : n) + currentbetinode;
}

// ----------------------------------------------------------------------------
// ---- Private helper functions for advancing the tree on betting actions ----
		
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
			if(isallin(mynode,currentgr,a))
				continue;

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
		if(isallin(mynode,currentgr,a))
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

	//get answer from window if available

	if(MyWindow!=NULL)
	{
		switch(MyWindow->GetCheckedRadioButton(IDC_RADIO1, IDC_RADIO9))
		{
		case IDC_RADIO1: answer=0; break;
		case IDC_RADIO2: answer=1; break;
		case IDC_RADIO3: answer=2; break;
		case IDC_RADIO4: answer=3; break;
		case IDC_RADIO5: answer=4; break;
		case IDC_RADIO6: answer=5; break;
		case IDC_RADIO7: answer=6; break;
		case IDC_RADIO8: answer=7; break;
		case IDC_RADIO9: answer=8; break;
		default: REPORT("Failure of buttons.");
		}
	}

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
		if(mynode->potcontrib[answer] != perceived[1-myplayer])
			REPORT("tree value does not match reality");
		amount = multiplier * invested[1-myplayer];
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
		if (isallin(mynode,currentgr,answer))
		{
			amount = 0;
			myact = ALLIN;
		}
		else
		{
			amount = multiplier * (mynode->potcontrib[answer] + invested[1-myplayer]-perceived[1-myplayer]);
			if(amount < multiplier * mintotalwager())
				amount = multiplier * mintotalwager();
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

	if(mynode == NULL) return; //does not signify an error

	//choose an action if we are next to act

	if(mynode->playertoact == myplayer)
	{
		double cumulativeprob, randomprob, actionprobs[9];

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
	}

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
	
	//we're going first and nothing's been bet yet
	if(currentgr != PREFLOP && acting==P0 && invested[1-acting]==0)
		return 0;

	//otherwise, this is the standard formula that FullTilt seems to follow
	double prevwager = invested[1-acting]-invested[acting];
	return invested[1-acting] + max((double)BB, prevwager);
}
