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
     num_used_strats(0),
	 currstrat(NULL),
	 actualinv(2, -1), //size is 2, initial values are -1
	 perceivedinv(2, -1), //size is 2, initial values are -1
	 cards(4), //size is 4 (one for each gr), initial values blah
	 bot_status(INVALID),
	 actionchooser() //seeds rand with time and clock
{

	CFileDialog filechooser(TRUE, "xml", NULL, OFN_NOCHANGEDIR);
	if(filechooser.DoModal() == IDCANCEL) exit(0);
	mystrats[num_used_strats++] = new Strategy(string((LPCTSTR)filechooser.GetPathName()));

	if(num_used_strats >= MAX_STRATEGIES) REPORT("not enough room for strategies!");
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
	for(int i=0; i<num_used_strats; i++)
	{
		double error = abs( mystrats[i]->gettree().getparams().stacksize 
			/ mystrats[i]->gettree().getparams().bblind - stacksize/bblind);
		if(error < besterror)
		{
			besterror = error;
			currstrat = mystrats[i]; //iterator
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
	betihistory.clear(); //destroy whatever it had
	betihistory.push_back(pair<int,int>(PREFLOP,currentbeti));
	currstrat->gettree().getnode(PREFLOP,perceivedpot,currentbeti,mynode);

	offtreebetallins = false;
	answer = -1;
	bot_status = WAITING_ACTION;
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

	actualinv[P0] = 0;
	actualinv[P1] = 0;
	perceivedinv[P0] = 0;
	perceivedinv[P1] = 0;
	actualpot = newpot/multiplier;
	perceivedpot += perceivedinv[0]; //both investeds must be the same
	cards[gr] = newboard;
	currentgr = gr;
	currentbeti = 0; //ready for P0 to act
	betihistory.push_back(pair<int,int>(currentgr,currentbeti));
	currstrat->gettree().getnode(gr,perceivedpot,currentbeti,mynode);
	bot_status = WAITING_ACTION;
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

	//try to find the best bet action

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

				double error = abs( (double)(mynode.potcontrib[a]) - amount );
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
		else WARN("the oppenent bet when the tree had to betting actions. off tree -> treating as all-in");
		offtreebetallins = true;
		doallin(pl);
	}
	else
	{
		actualinv[pl] = amount;
		perceivedinv[pl] = mynode.potcontrib[bestaction];
		currentbeti = mynode.result[bestaction];
		betihistory.push_back(pair<int,int>(currentgr,currentbeti));
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
	betihistory.push_back(pair<int,int>(currentgr,currentbeti));
	currstrat->gettree().getnode(currentgr,perceivedpot,currentbeti,mynode);
}

//this function gets called indiscriminately
void BotAPI::processmyturn()
{
	//choose an action if we are next to act

	if(bot_status == WAITING_ACTION && mynode.playertoact == myplayer)
	{
		double cumulativeprob = 0, randomprob;
		vector<double> actionprobability;
		currstrat->getprobs(betihistory, cards, actionprobability);
		randomprob = actionchooser.randExc(); //generates the answer!
		answer = -1;
		do{
			for(int a=0; a<mynode.numacts; a++)
			{
				cumulativeprob += actionprobability[a];
				if (cumulativeprob > randomprob)
				{
					answer = a;
					break;
				}
			}
		}while(answer==-1); //just in case due to rounding errors cumulative prob never reaches randomprob
	}

	//update diagnostics window

	if(isdiagnosticson && bot_status != INVALID) 
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

	if(mynode.playertoact == myplayer)
	{
		vector<double> probabilities;
		currstrat->getprobs(betihistory, cards, probabilities);
		for(int a=0; a<mynode.numacts; a++)
		{
			MyWindow->ActButton[a].SetWindowText(CSTR(currstrat->gettree().actionstring(currentgr,a,mynode,multiplier)));
			MyWindow->ActButton[a].EnableWindow(TRUE);
			MyWindow->ActionBars[a].ShowWindow(SW_SHOW);
			MyWindow->ActionBars[a].SetPos((int)(probabilities[a]*100.0)); //maps to 0-99, sometimes 100, should have 0-100
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

	//set sceni beti

	text.Format(TEXT("no sceni"));
	MyWindow->SceniText.SetWindowText(text);
	text.Format(TEXT("%d"),currentbeti);
	MyWindow->BetiText.SetWindowText(text);

	//set bethist

	text = TEXT("no bethist");
	for(int g=PREFLOP; g<RIVER; g++)
		MyWindow->Bethist[g].SetWindowText(text);

	//set pot amount

	text.Format(TEXT("$%0.2f"), 2*multiplier*perceivedpot);
	MyWindow->PotSize.SetWindowText(text);

	//set perceived invested amounts

	text.Format(TEXT("Bot: $%0.2f\nHuman: $%0.2f"), 
		multiplier*perceivedinv[myplayer],
		multiplier*perceivedinv[1-myplayer]);
	MyWindow->PerceivedInvestHum.SetWindowText(text);

	//set bin number

	int handi, boardi;
	currstrat->getcardmach().getindices(currentgr, cards, handi, boardi);
	if(currentgr==PREFLOP)
	{
		MyWindow->BinMax.SetWindowText(TEXT("Bin number:"));
		MyWindow->BinNumber.SetWindowText(TEXT("-"));
	}
	else
	{
		text.Format(TEXT("Bin number(1-%d):"),currstrat->getcardmach().getparams().bin_max[currentgr]);
		MyWindow->BinMax.SetWindowText(text);
		text.Format(TEXT("%d"),handi);
		MyWindow->BinNumber.SetWindowText(text);
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

