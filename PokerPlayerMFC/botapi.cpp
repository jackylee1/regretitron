#include "stdafx.h"
#include "botapi.h"
#include "lookupanswer.h"
#include "../PokerLibrary/determinebins.h"
#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/flopalyzer.h"
#include "../PokerLibrary/treenolimit.h"
#include "dodiagnostics.h"

BotAPI::BotAPI(bool diagon)
   : isdiagnosticson(diagon)
{
	initpfn();
	initbins();
}

BotAPI::~BotAPI()
{
	closebins();
	destroywindow();
}

void BotAPI::setnewgame(int playernum, CardMask hand, float sblind, float bblind)
{
	//check the inputs
	if(playernum != 0 && playernum != 1)
		REPORT("invalid myplayer number in BotAPI::setmyhand()");

	//set the multiplier
	multiplier = bblind / BB; //so that our values * mult gives real values
	//rescale inputs first
	sblind /= multiplier;
	bblind /= multiplier;
	//reset the invested amount
	invested[1] = sblind;
	perceived[1] = SB;
	invested[0] = bblind;
	perceived[0] = BB;

	//save the player number
	myplayer = playernum;
	//save the hand
	priv0 = hand; //easiest to always use priv0 in this case
	//determine the bin
	binnumber[myplayer][PREFLOP] = getpreflopbin(priv0);
	//set the gameround
	currentgr = PREFLOP;
	//reset bethist
	bethist[0] = bethist[1] = bethist[2] = -1;
	//reset the newpot value
	currentpot = 0;
	//set the sceni
	currentsceni = getscenarioi(PREFLOP, myplayer, 0, NULL);
	//reset the betinode
	currentbetinode = 0;
	//reset the offtreestuff
	offtreebetallins = false;
}

void BotAPI::setflop(CardMask theflop, float newpot)
{
	//adjust multiplier first
	newpot/=multiplier;

	//check usage: make sure we're at right gameround
	if (++currentgr != FLOP) REPORT("you set the flop at the wrong time");
	//check usage: make sure advancetree has moved us to a new round
	if (currentbetinode != -1) REPORT("you must advancetree before you setflop");
	currentbetinode = 0; //ready for P0 to act
	//check usage: make sure each have invested the same, and it adds up.
	if (invested[0] != invested[1]) REPORT("you both best be betting the same.");
	if (currentpot + invested[0] != newpot) REPORT("your new pot is unexpected.");
	invested[0] = invested[1] = perceived[0] = perceived[1] = 0;
	//check usage: make sure the newpot didn't shrink
	if (newpot < currentpot || newpot < BB) REPORT("newpot is too small.");
	//save the flop
	flop = theflop;
	//set the flopscore
	flopscore = flopalyzer(flop);
	//set the binnumber
	binnumber[myplayer][FLOP] = getflopbin(priv0,flop);
	//save the newpot total
	currentpot = newpot;
	//finally, set the sceni
	currentsceni = getscenarioi(FLOP, myplayer, (int)(newpot+0.5), bethist);
}

void BotAPI::setturn(CardMask theturn, float newpot)
{
	//adjust multiplier first
	newpot/=multiplier;

	//check usage: make sure we're at right gameround
	if (++currentgr != TURN) REPORT("you set the turn at the wrong time");
	//check usage: make sure advancetree has moved us to a new round
	if (currentbetinode != -1) REPORT("you must advancetree before you setturn");
	currentbetinode = 0; //ready for P0 to act
	//check usage: make sure each have invested the same
	if (invested[0] != invested[1]) REPORT("you both best be betting the same.");
	if (currentpot + invested[0] != newpot) REPORT("your new pot is unexpected.");
	invested[0] = invested[1] = perceived[0] = perceived[1] = 0;
	//check usage: make sure the pot didn't shrink
	if (newpot < currentpot || newpot < BB) REPORT("pot is too small.");
	//save the turn
	turn = theturn;
	//set the turnscore
	turnscore = turnalyzer(flop, turn);
	//set the binnumber
	binnumber[myplayer][TURN] = getturnbin(priv0,flop,turn);
	//save the new pot total
	currentpot = newpot;
	//finally, set the sceni
	currentsceni = getscenarioi(TURN, myplayer, (int)(newpot+0.5), bethist);
}

void BotAPI::setriver(CardMask theriver, float newpot)
{
	//adjust multiplier first
	newpot/=multiplier;

	//check usage: make sure we're at right gameround
	if (++currentgr != RIVER) REPORT("you set the river at the wrong time");
	//check usage: make sure advancetree has moved us to a new round
	if (currentbetinode != -1) REPORT("you must advancetree before you setriver");
	currentbetinode = 0; //ready for P0 to act
	//check usage: make sure each have invested the same
	if (invested[0] != invested[1]) REPORT("you both best be betting the same.");
	if (currentpot + invested[0] != newpot) REPORT("your new pot is unexpected.");
	invested[0] = invested[1] = perceived[0] = perceived[1] = 0;
	//check usage: make sure the pot didn't shrink
	if (newpot < currentpot || newpot < BB) REPORT("pot is too small.");
	//save the river
	river = theriver;
	//set the riverscore
	riverscore = rivalyzer(flop, turn, river);
	//set the binnumber
	binnumber[myplayer][RIVER] = getriverbin(priv0,flop,turn,river);
	//save the new pot total
	currentpot = newpot;
	//finally, set the sceni
	currentsceni = getscenarioi(RIVER, myplayer, (int)(newpot+0.5), bethist);
}


//wrapper function to handle diagnostics
void BotAPI::advancetree(int player, Action a, int amount)
{
	betnode const * mynode; //needed to check if i am next to act
	_advancetree(player,a,amount);
	mynode = (currentgr == PREFLOP ? pfn : n) + currentbetinode;
	//if it's my turn and diagnostics are on:
	if(isdiagnosticson && mynode->playertoact==myplayer)
	{
		populatewindow();
	}
}
//turns diagnostics window on or off;
void BotAPI::setdiagnostics(bool onoff)
{
	//need mynode to see if we are to act
	betnode const * mynode = (currentgr == PREFLOP ? pfn : n) + currentbetinode;
	//if state hasn't changed, don't care
	if(isdiagnosticson == onoff) return;
	//update state
	isdiagnosticson = onoff;
	//handle new state
	if(isdiagnosticson && mynode->playertoact==myplayer)
		populatewindow(); //will create window if not there
	else if(!isdiagnosticson)
		destroywindow(); //will do nothing if window not there
}

void BotAPI::_advancetree(int player, Action a, int amount)
{
	//adjust multiplier first
	amount/=multiplier;

	//the index of the other player
	int otherplayer = 1-player;
	//the index of the next action in the betting tree
	int nextact;
	//get a pointer to the current betting tree node.
	betnode const * mynode = (currentgr == PREFLOP ? pfn : n) + currentbetinode;

	//check usage: make sure that we are advanced to the correct player
	if(player != mynode->playertoact) REPORT("advancetree thought the other player should be acting");

	//we are at a betnode, we are given the action and amount of
	//the player at that betnode. we must determine the available
	//action that best matches that decision, and then advance the
	//currentbetnode to that decision.

	switch(a)
	{
	case FOLD:
		REPORT("do not tell me they folded, just setnewgame()");

	case CALL:
		if (amount != invested[otherplayer])
			REPORT("you called the wrong amount");
		invested[player] = amount;

		//there should be just one call option from the current player's betnode
		//he must have used that option, and we can get the bethist from it.
		bethist[currentgr] = getbethist(mynode);

		//this brings us to the end of a betting round, so we don't need
		//to worry about currentbetinode anymore. set it to error-checking value.
		currentbetinode = -1;
		return;

	case BET: //represents a new BET amount, could be raise or whatever

		if (amount < invested[otherplayer]) //when SB calls, could be equal.
			REPORT("you did not bet enough for that to be a bet");
		invested[player] = amount;

		//so now i need to figure out which of my bot's available actions
		//most closely matches the current one

		//the ones with 3 actions are fold/call/go allin,
		// but those options ignore the possibility of betting even more
		// without going all in. if player == me, then i know that i am
		// not going to choose the option to bet more w/o going all in, but
		// if it's my opponent, he could. if so, this is where i go off the
		// betting tree.
		if (mynode->numacts == 3)
		{
			if(player == myplayer)
				REPORT("i thought we couldn't bet from a 3-membered node!")
			else
			{
				//the opponent has left the betting tree!
				//we will call again advancetree, pretending that opponent
				//has bet all in.
				advancetree(player, ALLIN, -1);
				//we will do nothing here but set a flag to tell us that we should
				//treat "call all-in" actions as "bet all-in" actions.
				//this situation won't last long, as i am next to act, and
				//my only two options will each end the game (fold or all-in)
				offtreebetallins = true;
				//that's it.
				return;
			}
		}
		//otherwise, this should work.
		else
		{
			//should match bet amounts or so.
			nextact = getbestbetact(mynode, amount);

			//check what it returned
			switch(mynode->result[nextact])
			{
			case FD: case GO1: case GO2: case GO3: case GO4: case GO5: case AI: case NA:
				REPORT("no it's not! that action is not even a bet");
			default:
				if (mynode->potcontrib[nextact] == 0) //this would be betting all-in
					REPORT("no, taht's all-in, we didn't do that!");

				//ok, let's accept the answer and do it.
				currentbetinode = mynode->result[nextact];
				perceived[player] = mynode->potcontrib[nextact];
				return;
			}
		}
		REPORT("can't get here");

	case ALLIN: 
	//this means the player has BET allin, 
	//if he called all-in, i don't wanna hear about it, as the game is over.
		nextact = getallinact(mynode);
		if(nextact < 0 || nextact >=9)
			REPORT("could not find all-in node.");
		//check what it returned
		switch(mynode->result[nextact])
		{
		case FD: case GO1: case GO2: case GO3: case GO4: case GO5: case AI: case NA:
			REPORT("that is not a BET all-in");
		default:
			if (mynode->potcontrib[nextact] != 0)
				REPORT("that is still not a BET all-in");

			//ok, let's accept the answer and do it.
			currentbetinode = mynode->result[nextact];
			return;
		}
		REPORT("can't get here");

	default:
		REPORT("don't know what happened...");
	}
	REPORT("can't get here");
}

		
int BotAPI::getbestbetact(betnode const * mynode, int betsize)
{
	int possiblea[9];
	int possibleerror[9];
	int possi=0;

	for(int a=0; a<mynode->numacts; a++)
	{
		switch(mynode->result[a])
		{
		case FD: case GO1: case GO2: case GO3: case GO4: case GO5: case AI: case NA:
			continue;
		default:
			//save the action index
			possiblea[possi] = a;
			//save the error
			possibleerror[possi] = abs(mynode->potcontrib[a] - betsize);
			++possi;
		}
	}
	if (possi == 0)
		REPORT("no betting actions found!");

	//min_element returns the pointer to the minimum element of what its passed
	//this simply chooses the option with the nearest bet value.
	return possiblea[(std::min_element(possibleerror, possibleerror+possi) - possibleerror)];
}


int BotAPI::getallinact(betnode const * mynode)
{
	int possiblea[9];
	int possi=0;

	for(int a=0; a<mynode->numacts; a++)
	{
		switch(mynode->result[a])
		{
		case FD: case GO1: case GO2: case GO3: case GO4: case GO5: case AI: case NA:
			continue;
		default:
			if(mynode->potcontrib[a]==0)//then it's all-in!
			{
				//save the action index
				possiblea[possi] = a;
				++possi;
			}
		}
	}
	if (possi != 1)
		REPORT("not exactly 1 all-in action found!");

	return possiblea[0];
}

int BotAPI::getbethist(betnode const * mynode)
{
	for(int a=0; a<mynode->numacts; a++)
		switch(mynode->result[a])
		{
		//we trust there is only one of these in the damn tree
		case GO1: case GO2: case GO3: case GO4: case GO5:
			return mynode->result[a] - GO_BASE;
		}

	REPORT("failure to find a call action");
}


Action BotAPI::getanswer(float &amount)
{
	//these are read from the file
	float actionprobs[9]={0,0,0,0,0,0,0,0,0};
	//index of our chosen answer in the betting tree
	int answer;

	//get a pointer to the current betting tree node.
	betnode const * mynode = (currentgr == PREFLOP ? pfn : n) + currentbetinode;

	//check usage: make sure we are to act.
	if(mynode->playertoact != myplayer)
		REPORT("You asked for an answer when the bot thought action was on opp.")

	//fill array with the current action probabilities
	readstrategy(currentsceni, currentbetinode, actionprobs, mynode->numacts);

	//go through actions and choose one randomly with correct likelyhood.
	do{
		float cumulativeprob = 0;
		float randomprob = (float) rand()/RAND_MAX; //generates the answer!
		for(int a=0; a<mynode->numacts; a++)
		{
			cumulativeprob += actionprobs[a];
			if (cumulativeprob > randomprob)
			{
				answer = a;
				break;
			}
		}
	//like if randomprob is 1 and there are some adding errors to keep the cumulative prob under 1
	}while(answer == mynode->numacts);

	//pass back the correct numbers to communicate our answer
	switch(mynode->result[answer])
	{
	case FD:
		amount = -1;
		return FOLD;
	case NA:
		REPORT("we chose an invalid action. no good guys.");
	case GO1:
	case GO2:
	case GO3:
	case GO4:
	case GO5:
		amount = multiplier * (mynode->potcontrib[answer] + invested[myplayer]-perceived[myplayer]);
		return CALL;
	case AI:
		if(!offtreebetallins) //as it should be
		{
			amount = 0;
			return CALL;
		}
		else //but sometimes, we treat these nodes as BET all in instead of call.
		{
			if(mynode->numacts != 2)
				REPORT("I thought we should be at a 2-membered node now! (fold or call all-in)");
			amount = 0;
			return ALLIN;
		}

	default:
		if (amount = mynode->potcontrib[answer] == 0)
			return ALLIN;
		else
		{
			//so we actually bet or call the increment above what we really invested
			amount = multiplier * (mynode->potcontrib[answer] + invested[myplayer]-perceived[myplayer]);
			return BET;
		}
	}
}


