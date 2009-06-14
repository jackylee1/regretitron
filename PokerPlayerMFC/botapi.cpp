#include "stdafx.h"
#include "botapi.h"
#include "strategy.h"
#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/cardmachine.h"
#include "DiagnosticsPage.h" //to access radios for answer
#include "../utility.h"

bool fpequal(double a, double b)
{
	bool isequal = (a > b-0.00001 && a < b+0.00001);
	if(isequal && a!=b)
	{
		CString text;
		text.Format("fpequal did something! a-b=%f", a-b);
		WARN(text);
	}
	return isequal;
}

BotAPI::BotAPI(bool diagon)
   : isdiagnosticson(diagon), answer(-1), MyWindow(NULL),
     mystrats(), //initialize to be empty
	 currstrat(NULL),
	 actualinv(2, -1), //size is 2, initial values are -1
	 perceivedinv(2, -1), //size is 2, initial values are -1
	 cards(4), //size is 4 (one for each gr), initial values blah
	 bot_status(INVALID),
	 actionchooser(), //seeds rand with time and clock
	 historyindexer(this)
{
	CFileDialog filechooser(TRUE, "xml", NULL, OFN_NOCHANGEDIR);
	if(filechooser.DoModal() == IDCANCEL) exit(0);
	mystrats.push_back(new Strategy(string((LPCTSTR)filechooser.GetPathName())));
}

BotAPI::~BotAPI()
{
	//kill the diagnostics window, if any
	destroywindow();
}

void BotAPI::setnewgame(Player playernum, CardMask myhand, 
						double sblind, double bblind, double stacksize)
{
	if(playernum != P0 && playernum != P1)
		REPORT("invalid myplayer number in BotAPI::setnewgame()");
	if(!fpequal(stacksize/bblind, (int)(stacksize/bblind+0.5)))
		REPORT("stacksize/bblind is not an integer?");

	//find the strategy with the nearest stacksize
	
	double besterror = numeric_limits<double>::infinity();
	for(unsigned int i=0; i<mystrats.size(); i++)
	{
		double error = abs( mystrats[i]->gettree().getparams().stacksize 
			/ mystrats[i]->gettree().getparams().bblind - stacksize/bblind);
		if(error < besterror)
		{
			besterror = error;
			currstrat = mystrats[i]; //pointer
		}
	}

	//We go through the list of our private data and update each.

	multiplier = stacksize / currstrat->gettree().getparams().stacksize;
	actualinv[P0] = bblind/multiplier;
	actualinv[P1] = sblind/multiplier;
	perceivedinv[P0] = currstrat->gettree().getparams().bblind;
	perceivedinv[P1] = currstrat->gettree().getparams().sblind;
	actualpot = 0.0;
	perceivedpot = 0;

	myplayer = playernum;
	cards[PREFLOP] = myhand;

	currentgr = PREFLOP;
	currentbeti = 0;
	currstrat->gettree().getnode(PREFLOP,perceivedpot,currentbeti,mynode);

	offtreebetallins = false;
	answer = -1;
	bot_status = WAITING_ACTION;

	historyindexer.reset(); //primes with PREFLOP info
	processmyturn();
}

void BotAPI::setnextround(int gr, CardMask newboard, double newpot)
{
	//check input parameters

	if (currentgr != gr-1 || gr < FLOP || gr > RIVER) REPORT("you set the next round at the wrong time");
	if (bot_status != WAITING_ROUND) REPORT("you must advancetree before you setflop");
	if (actualinv[0] != actualinv[1]) REPORT("you both best be betting the same.");
	if (perceivedinv[0] != perceivedinv[1]) REPORT("perceived state is messed up");
	if (!fpequal(actualpot + actualinv[0],newpot/multiplier)) REPORT("your new pot is unexpected.");

	//Update the state of our private data members

	perceivedpot += perceivedinv[0]; //both investeds must be the same
	actualinv[P0] = 0;
	actualinv[P1] = 0;
	perceivedinv[P0] = 0;
	perceivedinv[P1] = 0;
	actualpot = newpot/multiplier;
	cards[gr] = newboard;
	currentgr = gr;
	currentbeti = 0; //ready for P0 to act
	currstrat->gettree().getnode(gr,perceivedpot,currentbeti,mynode);
	bot_status = WAITING_ACTION;
	historyindexer.push(currentgr, perceivedpot, currentbeti);
	processmyturn();
}

//wrapper function to simplify logic
void BotAPI::doaction(Player pl, Action a, double amount)
{
	if(bot_status != WAITING_ACTION) REPORT("doaction called at wrong time");
	if(pl != mynode.playertoact) REPORT("doaction thought the other player should be acting");

	if(a == ALLIN && pl == myplayer && offtreebetallins) 
	//if true, there would be no all-in node to find
	//we just bet an allin, instead of call, due to offtreebetallins
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

Action BotAPI::getbotaction(double &amount)
{
	//error checking

	if(myplayer != mynode.playertoact) REPORT("You asked for an answer when the bot thought action was on opp.");
	if(answer<0 || answer>=MAX_ACTIONS) REPORT("Inconsistant BotAPI state.");

	//replace strategy-chosen answer with answer from window if available

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

	//translate to Action type and set amount

	Action myact;
	switch(mynode.result[answer])
	{
	case BetNode::NA:
		REPORT("we chose an invalid action. no good guys.");

	case BetNode::FD:
		amount = -1;
		myact = FOLD;
		break;

	case BetNode::GO:
		if(mynode.potcontrib[answer] != perceivedinv[1-myplayer])
			REPORT("tree value does not match reality");
		amount = multiplier * actualinv[1-myplayer];
		myact = CALL;
		break;

	case BetNode::AI: //called all-in
		if(!offtreebetallins) //as it should be
		{
			amount = 0;
			myact = CALL;
		}
		else //but sometimes, we treat these nodes as BET all in instead of call.
		{
			if(mynode.numacts != 2)
				REPORT("I thought we should be at a 2-membered node now! (fold or call all-in)");
			amount = 0;
			myact = ALLIN;
		}
		break;

	default:
		if(currstrat->gettree().isallin(mynode.result[answer],mynode.potcontrib[answer],currentgr))
		{
			amount = 0;
			myact = ALLIN;
		}
		else
		{
			amount = multiplier * (mynode.potcontrib[answer] + actualinv[1-myplayer]-perceivedinv[1-myplayer]);
			if(amount < multiplier * mintotalwager())
			{
				WARN("bot bet amount was less than multiplier * mintotalwager()");
				amount = multiplier * mintotalwager();
			}
			myact = BET;
		}
	}

	answer = -1;
	return myact;
}


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


//a "call" by definition ends the betting round
void BotAPI::docall(Player pl, double amount)
{
	if(!fpequal(amount,actualinv[1-pl])) REPORT("you called the wrong amount");

	actualinv[pl] = actualinv[1-pl];
	perceivedinv[pl] = perceivedinv[1-pl];
	bot_status = WAITING_ROUND;
}

//a "bet" by definition continues the betting round
void BotAPI::dobet(Player pl, double amount)
{

	//error checking 

	if(amount < mintotalwager() || amount >= currstrat->gettree().getparams().stacksize)
		REPORT("Invalid bet amount");

	//try to find the bet action that will set the new perceived pot closest to the actual

	int bestaction=-1;
	double besterror = numeric_limits<double>::infinity();
	for(int a=0; a<mynode.numacts; a++)
	{
		switch(mynode.result[a])
		{
		case BetNode::FD: 
		case BetNode::GO: 
		case BetNode::AI: 
		case BetNode::NA:
			continue;

		default:
			if(currstrat->gettree().isallin(mynode.result[a],mynode.potcontrib[a],currentgr))
				continue;

			double error = abs((double)(perceivedpot + mynode.potcontrib[a]) - (actualpot + amount));
			if(error < besterror)
			{
				bestaction = a;
				besterror = error;
			}
		}
	}

	// if there's no bet, doallin, otherwise, update state.

	if (bestaction == -1)
	{
		if(pl == myplayer) REPORT("bot the bot HAD to have bet from the tree. we should find a bet action.");
		else WARN("the oppenent bet when the tree had no betting actions. off tree -> treating as all-in");
		offtreebetallins = true;
		doallin(pl);
	}
	else
	{
		actualinv[pl] = amount;
		perceivedinv[pl] = mynode.potcontrib[bestaction];
		currentbeti = mynode.result[bestaction];
		historyindexer.push(currentgr, perceivedpot, currentbeti);
		currstrat->gettree().getnode(currentgr,perceivedpot,currentbeti,mynode);
	}
}

//this is a "bet" of amount all-in
void BotAPI::doallin(Player pl)
{
	int allinaction, total=0;
	for(int a=0; a<mynode.numacts; a++)
	{
		if(currstrat->gettree().isallin(mynode.result[a],mynode.potcontrib[a],currentgr))
		{
			allinaction = a;
			total++;
		}
	}
	if (total != 1) REPORT("not exactly 1 all-in action found!");

	actualinv[pl] = currstrat->gettree().getparams().stacksize-actualpot;
	perceivedinv[pl] = currstrat->gettree().getparams().stacksize-perceivedpot;
	currentbeti = mynode.result[allinaction];
	historyindexer.push(currentgr, perceivedpot, currentbeti);
	currstrat->gettree().getnode(currentgr,perceivedpot,currentbeti,mynode);
}

//this function gets called indiscriminately
void BotAPI::processmyturn()
{
	//choose an action if we are next to act

	if(bot_status == WAITING_ACTION && mynode.playertoact == myplayer)
	{
		double cumulativeprob = 0, randomprob;
		if(historyindexer.getnuma() != mynode.numacts)
			REPORT("Indexer has FAILED!"); //only reason for hist indexer getnuma
		vector<double> probabilities(mynode.numacts);
		currstrat->getprobs(currentgr, historyindexer.getactioni(),
			mynode.numacts, cards, probabilities);
		randomprob = actionchooser.randExc(); //generates the answer!
		answer = -1;
		do{
			for(int a=0; a<mynode.numacts; a++)
			{
				cumulativeprob += probabilities[a];
				if (cumulativeprob > randomprob)
				{
					answer = a;
					break;
				}
			}
		}while(answer==-1); //just in case due to rounding errors cumulative prob never reaches randomprob
	}

	//update diagnostics window

	if(isdiagnosticson)
		populatewindow();
}

double BotAPI::mintotalwager()
{
	Player acting = (Player)mynode.playertoact;
	//calling from the SBLIND is a special case of how much we can wager
	if(currentgr == PREFLOP && actualinv[acting]<(double)currstrat->gettree().getparams().bblind)
		return (double)currstrat->gettree().getparams().bblind;
	
	//we're going first and nothing's been bet yet
	if(currentgr != PREFLOP && acting==P0 && actualinv[1-acting]==0)
		return 0;

	//otherwise, this is the standard formula that FullTilt seems to follow
	double prevwager = actualinv[1-acting]-actualinv[acting];
	return actualinv[1-acting] + max((double)currstrat->gettree().getparams().bblind, prevwager);
}

#define CSTR(stdstr) CString((stdstr).c_str())

//this function is a member of BotAPI so that it has access to all the
//current state of the bot. MyWindow is also a member of BotAPI, and has public
//controls and wrapper functions to allow me to update the page from outside of its class.
void BotAPI::populatewindow(CWnd* parentwin)
{
	CString text; //used throughout

	//create window if needed

	if(MyWindow == NULL && parentwin != NULL)
		MyWindow = new DiagnosticsPage(parentwin);

	//set actions

	if(bot_status == INVALID) return; //can't do nothin with that!

	if(mynode.playertoact == myplayer)
	{
		vector<double> probabilities(mynode.numacts);
		currstrat->getprobs(currentgr, historyindexer.getactioni(),
			mynode.numacts, cards, probabilities);
		for(int a=0; a<mynode.numacts; a++)
		{
			MyWindow->ActButton[a].SetWindowText(CSTR(currstrat->gettree().actionstring(currentgr,a,mynode,multiplier)));
			MyWindow->ActButton[a].EnableWindow(TRUE);
			MyWindow->ActionBars[a].ShowWindow(SW_SHOW);
			int position = (int)(probabilities[a]*101.0);
			if(position == 101)
				position = 100;
			if(position < 0 || position > 100)
				REPORT("failure of probabilities");
			MyWindow->ActionBars[a].SetPos(position);
			if(answer==a)
				MyWindow->ActButton[a].SetCheck(BST_CHECKED);
			else
				MyWindow->ActButton[a].SetCheck(BST_UNCHECKED);
		}
		for(int a=mynode.numacts; a<MAX_ACTIONS; a++)
		{
			MyWindow->ActButton[a].SetWindowText(TEXT(""));
			MyWindow->ActButton[a].EnableWindow(FALSE);
			MyWindow->ActionBars[a].ShowWindow(SW_HIDE);
		}
	}
	else
	{
		for(int a=0; a<MAX_ACTIONS; a++)
		{
			if(currentgr == PREFLOP && currentbeti == 0) //start of game, human is first, so reset
			{
				MyWindow->ActButton[a].SetWindowText(TEXT(""));
				MyWindow->ActButton[a].SetCheck(BST_UNCHECKED);
				MyWindow->ActionBars[a].SetPos(0);
			}
			MyWindow->ActButton[a].EnableWindow(FALSE);
		}
	}

	//set beti history

	const vector<HistoryNode> &myhist = historyindexer.gethistory();
	text.Format("(bot = P%d, human = P%d)\n", myplayer, 1-myplayer);
	for(unsigned int i=0; i<myhist.size(); i++)
	{
		if(i==0 || myhist[i].gr != myhist[i-1].gr)
		{
			switch(myhist[i].gr)
			{
			case PREFLOP: text.Append("Preflop:  "); break;
			case FLOP: text.Append("\nFlop:  "); break;
			case TURN: text.Append("\nTurn:  "); break;
			case RIVER: text.Append("\nRiver:  "); break;
			default: REPORT("broken Bet History");
			}
			text.AppendFormat("%d", myhist[i].beti);
		}
		else
			text.AppendFormat(" - %d", myhist[i].beti);

	}
	MyWindow->BetHistory.SetWindowText(text);

	//set pot amount

	text.Format(TEXT("$%0.2f"), 2*multiplier*perceivedpot);
	MyWindow->PotSize.SetWindowText(text);

	//set perceived invested amounts

	text.Format(TEXT("Bot: $%0.2f\nHuman: $%0.2f"), 
		multiplier*perceivedinv[myplayer],
		multiplier*perceivedinv[1-myplayer]);
	MyWindow->PerceivedInvestHum.SetWindowText(text);

	//set bin number & board score

	int handi, boardi;
	currstrat->getcardmach().getindices(currentgr, cards, handi, boardi);
	if(currentgr==PREFLOP)
	{
		MyWindow->BinMax.SetWindowText(TEXT("Bin number:"));
		MyWindow->BinNumber.SetWindowText(TEXT("-"));
		MyWindow->BoardScore.SetWindowText(TEXT("-"));
	}
	else
	{
		text.Format(TEXT("Bin number(1-%d):"),currstrat->getcardmach().getparams().bin_max[currentgr]);
		MyWindow->BinMax.SetWindowText(text);
		text.Format(TEXT("%d"),handi);
		MyWindow->BinNumber.SetWindowText(text);
		text.Format(TEXT("%d"),boardi);
		MyWindow->BoardScore.SetWindowText(text);
	}

	//send cards and our strategy to MyWindow, so that it can draw the cards as it needs

	MyWindow->setcardstoshow(currstrat, currentgr, handi, boardi);
	MyWindow->RefreshCards(); //same function as is called when button is clicked

	//if preflop, may have stale images of board cards still up

	if(currentgr==PREFLOP)
		MyWindow->RedrawWindow();
}

void BotAPI::destroywindow()
{
	if(MyWindow != NULL)
	{
		MyWindow->DestroyWindow();
		delete MyWindow;
		MyWindow = NULL;
	}
}

void BotAPI::BetHistoryIndexer::push(int gr, int pot, int beti) //or could have go return pot
{
	history.push_back(HistoryNode(gr, pot, beti));
	HistoryNode &prev = history[history.size()-2];
	if(!go(prev.gr, prev.pot, prev.beti))
		REPORT("could not find matching node in the tree in BetHistoryIndexer::push()");
	for(int i=0; i<4; i++) 
		for(int j=0; j<MAX_NODETYPES; j++) 
			if(counters[i][j]<0 || counters[i][j]>parentbotapi->currstrat->getactionmax(i,j))
				REPORT("counters too high!");
	if(current_actioni < 0 || current_actioni >= parentbotapi->currstrat->getactionmax(gr, current_numa-2))
		REPORT("current_actioni is bad!");
	if(current_actioni != counters[gr][current_numa-2])
		REPORT("huh?");
}

void BotAPI::BetHistoryIndexer::reset() //clear and prime with preflop info
{ 
	current_actioni = -1;
	current_numa = -1;
	history.clear();
	for(int i=0; i<4; i++) 
		for(int j=0; j<MAX_NODETYPES; j++) 
			counters[i][j]=0;

	//now do same as push, but can't call push as don't have previous HistoryNode from history vect.
	history.push_back(HistoryNode(PREFLOP, 0, 0));
	if(!go(PREFLOP, 0, 0)) //will just find it right away and set what it needs to set
		REPORT("could not find matching node in the tree in BetHistoryIndexer::push()");

}

bool BotAPI::BetHistoryIndexer::go(int gr, int pot, int beti)
{
	BetNode mynode;
	parentbotapi->currstrat->gettree().getnode(gr, pot, beti, mynode);
	const int &numa = mynode.numacts; //for ease of typing

	if(history.back().beti==beti && history.back().gr==gr && history.back().pot==pot)
	{
		//need to save at least one of these. we save both for cleanliness and fun. 
		current_numa = numa;
		current_actioni = counters[gr][numa-2];
		return true; //found it!
	}

	counters[gr][numa-2]++;

	for(int a=0; a<numa; a++) //looking for actions that lead to more play
	{
		switch(mynode.result[a])
		{
		case BetNode::NA:
			REPORT("invalid tree");
		case BetNode::FD: //no next action
		case BetNode::AI: //no next action
			continue;
		case BetNode::GO:
			if(gr!=RIVER) //RIVER would mean no next action
				if(go(gr+1, pot+mynode.potcontrib[a], 0)) //run go and test value to see if we found it
					return true; //found it!
			continue; //didn't find it (or if RIVER not even a next action), keep trying

		default://child node
			if(go(gr, pot, mynode.result[a]))
				return true; //found it!
			continue; //keep tryin..
		}
	}

	//we went through all the actions, and didn't find it.
	return false;
}

