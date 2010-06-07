#include "stdafx.h"
#include "botapi.h"
#include "strategy.h"
#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/cardmachine.h"
#include "DiagnosticsPage.h" //to access radios for answer
#include "../utility.h"
#include <iomanip>
#ifndef TIXML_USE_TICPP
#define TIXML_USE_TICPP
#endif
#include "../TinyXML++/ticpp.h"

inline bool playerequal(Player player, NodeType nodetype)
{
	return ((player == P0 && nodetype == P0Plays) || (player == P1 && nodetype == P1Plays));
}

BotAPI::BotAPI(string xmlfile, string botname, bool preload)
   : myname(botname),
	 MyWindow(NULL),
     isdiagnosticson(false),
#if PLAYOFFDEBUG > 0
	 actionchooser(playoff_rand), //alias to globally defined object from utility.cpp
#else
     actionchooser(), //per BotAPI object, seeded with time and clock
#endif
	 mystrats(0), //initialize empty
	 currstrat(NULL),
	 actualinv(2, -1), //size is 2, initial values are -1
	 perceivedinv(2, -1), //size is 2, initial values are -1
	 cards(), //size is 0, will be resized
	 bot_status(INVALID)
{ 
	//check the xml file to see if it's a bot or a collection of bots

	using namespace ticpp;
	const string olddirectory = getdirectory();
	try
	{
		//load the xml
		Document doc(xmlfile);
		doc.LoadFile();

		//check if it is a portfolio (false means do not throw if node not found
		Element* root = doc.FirstChildElement("portfolio", false);
		if(root == NULL) //this is not a portfolio, it is just a bot. 
		{
			mystrats.push_back(new Strategy(xmlfile, preload));
		}
		else //we actually have a portfolio
		{

			//check version
			if(!root->HasAttribute("version") || root->GetAttribute<int>("version") != 0)
				REPORT("Unsupported portfolio XML file (older/newer version).");

			//set working directory to portfolio directory
			string newdirectory = xmlfile.substr(0,xmlfile.find_last_of("/\\"));
			if(newdirectory != xmlfile) //has directory info
				setdirectory(xmlfile.substr(0,xmlfile.find_last_of("/\\")));

			//loop, read each filename
			Iterator<Element> child("bot");
			for(child = child.begin(root); child != child.end(); child++)
				mystrats.push_back(new Strategy(child->GetText(), preload));

			if(mystrats.size() < 2)
				REPORT("Portfolio only had "+tostring(mystrats.size())+" bots!");
		}
	}
	catch(Exception &ex) //error parsing XML
	{
		REPORT(ex.what());
		exit(-1);
	}

	//restore directory

	setdirectory(olddirectory);
}

BotAPI::~BotAPI()
{
	//kill the diagnostics window, if any
#ifdef _MFC_VER
	destroywindow();
#endif
	for(unsigned i=0; i<mystrats.size(); i++)
		delete mystrats[i];
}

//sblind, bblind - self explanatory
//stacksize - smallest for ONE player
void BotAPI::setnewgame(Player playernum, CardMask myhand, 
						double sblind, double bblind, double stacksize)
{
	if(playernum != P0 && playernum != P1)
		REPORT("invalid myplayer number in BotAPI::setnewgame()");

	//these stacksizes do not leave the bot with any choices in the game
	//we set the status to INVALID, allowing this call, but any subsequent calls for this
	//"game" will fail
	if(fpgreatereq(sblind, stacksize) || (fpgreatereq(bblind, stacksize) && playernum == P0))
	{
		bot_status = INVALID;
		return;
	}

	//find the strategy with the nearest stacksize
	
	double besterror = numeric_limits<double>::infinity();
	for(unsigned int i=0; i<mystrats.size(); i++)
	{
		const double stacksizemult_i = 
				(double)get_property(mystrats[i]->gettree(), settings_tag()).stacksize 
				/ (double)get_property(mystrats[i]->gettree(), settings_tag()).bblind;

		//in LIMIT, the bot MUST have at least as much chips as it has in real life.
		//in NO LIMIT, it doesn't matter as much: if the bot thinks it has only a couple chips left
		//  and pushes, it can still push in real life with its larger real-life stack and everything 
		//  is approximately cool. But a limit bot can't do that - it can't bet all-in unless it can't
		//  meet the current bet amount. So when it thinks that its only option is pushing and chooses
		//  to do so, it will then be DONE and have NO more information for you to play the game with.
		//so if it's limit, we ignore all bots with less stacksize than reality.
		if(islimit() && fpgreater( mymin(stacksize/bblind, 24.0), stacksizemult_i )) // 24bbs is the largest limit game
			continue;

		double error = myabs(stacksizemult_i - stacksize / bblind);

		if(error < besterror)
		{
			besterror = error;
			currstrat = mystrats[i]; //pointer
		}
	}
	if(besterror > 1e20)
	{
		REPORT("Could find no suitable strategy to play with a stacksize of "+tostring(stacksize/bblind)+" bblinds in "+(islimit()?"limit.":"no-limit."));
	}


	if(isloggingon())
	{
		getlog() << endl << endl << myname << " is in BotAPI::setnewgame(), starting game as P" << playernum << "." << endl;
		getlog() << myname << " sees " << sblind << '/' << bblind << " with stacksize " << stacksize << endl;
		getlog() << myname << " chose strategy " << currstrat->getfilename() << endl;
		getlog() << myname << "'s hole cards: " << tostring(myhand) << endl << endl;
	}

	//We go through the list of our private data and update each.

	if(islimit())
		multiplier = bblind / get_property(currstrat->gettree(), settings_tag()).bblind;
	else
		multiplier = stacksize / get_property(currstrat->gettree(), settings_tag()).stacksize;
	//in no limit, we don't need to use this because we normalize the bet amounts to match stacksize
	truestacksize = stacksize/multiplier;
	actualinv[P0] = bblind/multiplier;
	actualinv[P1] = sblind/multiplier;
	perceivedinv[P0] = get_property(currstrat->gettree(), settings_tag()).bblind;
	perceivedinv[P1] = get_property(currstrat->gettree(), settings_tag()).sblind;
	actualpot = 0.0;
	perceivedpot = 0;

	myplayer = playernum;
	cards.resize(1);
	cards[PREFLOP] = myhand;

	currentgr = PREFLOP;
	currentnode = currstrat->gettreeroot();

	offtreebetallins = false;
	bot_status = WAITING_ACTION;

	processmyturn();
}

//gr - zero offset gameround
//newboard - only the NEW card(s) from this round
//newpot - this is the pot for just ONE player
void BotAPI::setnextround(int gr, CardMask newboard, double newpot/*really just needed for error checking*/)
{
	//check input parameters

	if ((int)cards.size() != gr || currentgr != gr-1 || gr < FLOP || gr > RIVER) REPORT("you set the next round at the wrong time");
	if (bot_status != WAITING_ROUND) REPORT("you must advancetree before you setflop");
	if (!fpequal(actualinv[0], actualinv[1])) REPORT("you both best be betting the same.");
	if (perceivedinv[0] != perceivedinv[1]) REPORT("perceived state is messed up");
	if (!fpequal(actualpot+actualinv[0], newpot/multiplier)) REPORT("your new pot is unexpected.");
	if(fpgreatereq(actualpot+actualinv[P0],truestacksize) && fpgreatereq(actualpot+actualinv[P1],truestacksize))
		REPORT("You're sending me a round when we've already all-in'd.");

	if(isloggingon())
		getlog() << endl << "In BotAPI::setnextround() for " << gameroundstring(gr) << ": " << tostring(newboard) << " (pot = " << newpot << ")" << endl << endl;

	//Update the state of our private data members

	perceivedpot += perceivedinv[0]; //both investeds must be the same
	actualinv[P0] = 0;
	actualinv[P1] = 0;
	perceivedinv[P0] = 0;
	perceivedinv[P1] = 0;
	actualpot = newpot/multiplier;
	cards.push_back(newboard);
	currentgr = gr;

	EIter e, elast;
	for(tie(e, elast) = out_edges(currentnode, currstrat->gettree()); e!=elast; e++)
	{
		if((currstrat->gettree())[*e].type == Call)
		{
			currentnode = target(*e, currstrat->gettree());
			break;
		}
	}

	bot_status = WAITING_ACTION;
	processmyturn();
}

//wrapper function to simplify logic
//amount is only used during a call or a bet
//amount called - the total amount from ONE player to complete all bets from this round
//amount bet - the total amount wagered from ONE player, beyond what the other has put out so far
void BotAPI::doaction(Player pl, Action a, double amount)
{
	if(bot_status != WAITING_ACTION) REPORT("doaction called at wrong time");
	if(!playerequal(pl, currstrat->gettree()[currentnode].type)) REPORT("doaction thought the other player should be acting");
	if(fpgreatereq(actualpot+actualinv[1-pl],truestacksize) && fpgreatereq(actualpot+amount,truestacksize))
		REPORT("You're sending me an action when you've already bet all-in'd.");

	if(a == BETALLIN && pl == myplayer && offtreebetallins) 
	//if true, there would be no all-in node to find
	//we just bet an allin, instead of call, due to offtreebetallins
		return;

	switch(a)
	{
	case FOLD:  REPORT("The bot does not need to know about folds");
	case CALL:  docall (pl, amount/multiplier); break;
	case BET:   dobet  (pl, amount/multiplier); break;
	case BETALLIN: if(islimit()) REPORT("NO ALLIN 4 u!"); doallin(pl); break;
	default:    REPORT("You advanced tree with an invalid action.");
	}

	processmyturn();
}

//amount is the total amount of a wager when betting/raising (relative to the beginning of the round)
//          the amount aggreed upon when calling (same as above) or folding (not same as above)
//amount is equal to a POTCONTRIB style of amount. see how you defined potcontrib in the betting tree for details.
//amount is scaled and computed to be correct in the real game, regardless of what the bot sees.
//we've already chosen our action already, this is just the function that the outside world
//calls to get that answer.
Action BotAPI::getbotaction(double &amount)
{
	//error checking

	if(bot_status != WAITING_ACTION) REPORT("bot is waiting for a round or is inconsistent state");
	if(!playerequal(myplayer, currstrat->gettree()[currentnode].type)) REPORT("You asked for an answer when the bot thought action was on opp.");
	if(fpgreatereq(actualpot+actualinv[P0],truestacksize) && fpgreatereq(actualpot+actualinv[P1],truestacksize))
		REPORT("You're asking me what to do when we've already all-in'd.");

	//get the probabilities

	vector<double> probabilities(out_degree(currentnode, currstrat->gettree()));
	currstrat->getprobs(currentgr, (currstrat->gettree())[currentnode].actioni,
			out_degree(currentnode, currstrat->gettree()), cards, probabilities);

	if(isloggingon())
	{
		getlog() << myname << "At node: gameround = " << currentgr << ", cardsi = " << currstrat->getcardmach().getcardsi(currentgr, cards) << ", actioni = " << (currstrat->gettree())[currentnode].actioni << endl;
		getlog() << myname << "My options: ((";
		EIter e, elast;
		for(tie(e, elast) = out_edges(currentnode, currstrat->gettree()); e!=elast; e++)
			getlog() << actionstring(currstrat->gettree(), *e, multiplier) << "  ";
		getlog() << ")) = < ";
		for(unsigned i=0; i<out_degree(currentnode, currstrat->gettree()); i++)
			getlog() << setprecision(3) << fixed << 100*probabilities[i] << "% ";
		getlog() << ">" << endl;
	}

	//choose an action

	//in limit, we choose either the random one, or the most expensive affordable action, whichever is less
	//uses fact that the edges are inserted in tree in order of cost (or potcontrib that is)

	double randomprob = actionchooser.randExc(), cumulativeprob = 0;
	EIter eanswer, efirst, elast;
	tie(efirst, elast) = out_edges(currentnode, currstrat->gettree());
	int i=0;
	for(tie(eanswer, elast) = out_edges(currentnode, currstrat->gettree()); eanswer!=elast; eanswer++, i++)
	{
		cumulativeprob += probabilities[i];
		if( (cumulativeprob > randomprob || (fpequal(cumulativeprob, 1) && fpequal(randomprob, 1)))
				|| ( islimit() && fpgreatereq(perceivedpot + (currstrat->gettree())[*eanswer].potcontrib, truestacksize) ) )
			break;
	}

	if(eanswer == elast) //can't be rounding error
		REPORT("broken answer choosing mechanism!");

	if(isloggingon())
		getlog() << myname << "Chose " << actionstring(currstrat->gettree(), *eanswer, multiplier) << endl << endl;

	//potentially over-ride choice with diagnostics window

#ifdef _MFC_VER
	if(MyWindow!=NULL)
	{
		switch(MyWindow->GetCheckedRadioButton(IDC_RADIO1, IDC_RADIO9))
		{
			//if you chose the n'th button, this will increment efirst n-1 times, and set it to eanswer
			case IDC_RADIO9: efirst++;
			case IDC_RADIO8: efirst++;
			case IDC_RADIO7: efirst++;
			case IDC_RADIO6: efirst++;
			case IDC_RADIO5: efirst++;
			case IDC_RADIO4: efirst++;
			case IDC_RADIO3: efirst++;
			case IDC_RADIO2: efirst++;
			case IDC_RADIO1:
				eanswer = efirst;
			case 0: 
				break; //this is what we get if no radio button is chosen
			default: 
				REPORT("Failure of buttons.");
		}
	}
#endif

	//translate to Action type and set amount

	Action myact=BETALLIN; //compiler warnings. could put exit() after each REPORT to fix warnings.
	switch((currstrat->gettree())[*eanswer].type)
	{
		case Fold:
			if((currstrat->gettree())[*eanswer].potcontrib != perceivedinv[myplayer])
				REPORT("tree value does not match folding reality");
			amount = multiplier * actualinv[myplayer];
			myact = FOLD;
			break;

		case Call:
			if((currstrat->gettree())[*eanswer].potcontrib != perceivedinv[1-myplayer])
				REPORT("tree value does not match calling reality");
			amount = multiplier * actualinv[1-myplayer];
			myact = CALL;
			break;

		case CallAllin:
			amount = multiplier * (truestacksize - actualpot);
			if(!offtreebetallins) //as it should be
				myact = CALL;
			else if(islimit() || out_degree(currentnode, currstrat->gettree()) != 2)
				REPORT("Limit, or I thought we should be at a 2-membered node now! (fold or call all-in)");
			else //but sometimes, we treat these nodes as BET all in instead of call.
				myact = BETALLIN;
			break;

		case BetAllin:
			amount = multiplier * (truestacksize - actualpot);
			if(islimit()) //in limit, we acknowledge that a "bet allin" is actually a bet that we can't fully cover...
				myact = BET;
			else
				myact = BETALLIN;
			break;

		case Bet:
			if(islimit()) //this could be a covert all-in, as the bot has generally more chips than reality
				amount = multiplier * mymin(truestacksize - actualpot, (double)(currstrat->gettree()[*eanswer].potcontrib));
			else //all-ins would be handled above
			{
				amount = multiplier * ((currstrat->gettree())[*eanswer].potcontrib + actualinv[1-myplayer]-perceivedinv[1-myplayer]);
				if(fpgreater(multiplier * mintotalwager(), amount))
				{
					if(isloggingon())
						getlog() << myname << "...changing bet amount from " << amount << " to " << multiplier * mintotalwager() << " ... " << endl;
					REPORT("bot bet amount was less than multiplier * mintotalwager()", WARN);
					amount = multiplier * mintotalwager();
				}
			}
			myact = BET;
			break;
	}

	return myact;
}

#ifdef _MFC_VER
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
#endif


//a "call" by definition ends the betting round
void BotAPI::docall(Player pl, double amount)
{
	if(!fpequal(amount,actualinv[1-pl])) REPORT("you called the wrong amount");

	if(isloggingon())
		getlog() << (pl == myplayer ? myname : "Human") << " in BotAPI::docall has called $" << amount << endl;

	actualinv[pl] = actualinv[1-pl];
	perceivedinv[pl] = perceivedinv[1-pl];
	bot_status = WAITING_ROUND;
}

//a "bet" by definition continues the betting round
void BotAPI::dobet(Player pl, double amount)
{

	//error checking 

	if(fpgreater(mintotalwager(), amount) || fpgreater(actualpot + amount, truestacksize))
		REPORT("Invalid bet amount");

	if(isloggingon())
		getlog() << (pl == myplayer ? myname : "Human") << " in BotAPI::dobet has bet/raised $" << amount << endl;

	//NO LIMIT: try to find the bet action that will set the new perceived pot closest to the actual.
	// if no bet actions, then offtreebetallins is invoked to handle this human's behavior that my tree does not have.
	//LIMIT: we handle true bets and covert all-in's aka bets someone can't fully cover here. we want the 'Bet'
	// or 'BetAllin' from the tree (doesn't matter) that has potcontrib equal-to or greater-than the given real-world
	// amount. The only time it will be greater-than is if the real world is short-stacked

	//find the 'Bet' node with amount closest to reality
	double besterror = numeric_limits<double>::infinity();
	EIter e, elast, ebest;
	for(tie(e, elast) = out_edges(currentnode, currstrat->gettree()); e!=elast; e++)
	{
		if((currstrat->gettree())[*e].type == Bet || (islimit() && (currstrat->gettree())[*e].type == BetAllin))
		{
			double error = (double) (perceivedpot + (currstrat->gettree())[*e].potcontrib) - (actualpot + amount);
			if(islimit() && fpgreater(0, error))
				continue;
			error = myabs(error);
			if(error < besterror)
			{
				ebest = e;
				besterror = error;
			}
		}
	}

	if(besterror > 1e20) //probably still infinity -> found no node
	{
		if(islimit()) REPORT("in limit, we could not find a 'Bet' node to match some player's Bet. Fatal.");
		if(pl == myplayer) REPORT("could not find Bot's own bet action in tree. the bot HAD to have bet from the tree. we should find a bet action.");
		else REPORT("the oppenent bet when the tree had no betting actions. off tree -> treating as all-in", INFO);
		offtreebetallins = true;
		doallin(pl);
	}
	else
	{
		if(islimit() && !( fpequal(besterror, 0) ||
					   ( fpequal(actualpot + amount, truestacksize) && !fpequal(get_property(currstrat->gettree(), settings_tag()).stacksize, truestacksize) )))
			REPORT("in limit, we expect bets to either exactly match the tree's bets or this to be a covert all-in");
		if(!fpequal(besterror, 0) && isloggingon())
			getlog() << myname << " in BotAPI::dobet found best betting action of $" << currstrat->gettree()[*ebest].potcontrib*multiplier << " with error " << besterror << endl;
		actualinv[pl] = amount;
		perceivedinv[pl] = currstrat->gettree()[*ebest].potcontrib;
		currentnode = target(*ebest, currstrat->gettree());
	}
}

//this is a "bet" of amount all-in
//used by NO-LIMIT only.
void BotAPI::doallin(Player pl)
{
	if(islimit()) REPORT("!");
	EIter e, elast;
	for(tie(e, elast) = out_edges(currentnode, currstrat->gettree()); e!=elast; e++)
		if((currstrat->gettree())[*e].type == BetAllin)
			break;

	if(e==elast)
		REPORT("in no limit, we could not find a BetAllin. What gives?");

	currentnode = target(*e, currstrat->gettree());

	if(perceivedpot + (currstrat->gettree())[*e].potcontrib != get_property(currstrat->gettree(), settings_tag()).stacksize) 
		REPORT("all-in potcontrib set incorrectly",WARN);

	if(isloggingon())
		getlog() << (pl == myplayer ? myname : "Human") << " in BotAPI::doallin has bet/raised All-In" << endl;

	actualinv[pl] = get_property(currstrat->gettree(), settings_tag()).stacksize-actualpot;
	perceivedinv[pl] = (currstrat->gettree())[*e].potcontrib;
}

//this function gets called all the time
void BotAPI::processmyturn()
{
	//update diagnostics window

#ifdef _MFC_VER
	if(isdiagnosticson)
		populatewindow();
#endif
}

double BotAPI::mintotalwager()
{
	const NodeType &acting = currstrat->gettree()[currentnode].type;
	double minwager;

	//calling from the SBLIND is a special case of how much we can wager
	if(currentgr == PREFLOP && fpgreater(get_property(currstrat->gettree(), settings_tag()).bblind, actualinv[playerindex(acting)]))
		minwager = get_property(currstrat->gettree(), settings_tag()).bblind;
	
	//we're going first and nothing's been bet yet
	else if(currentgr != PREFLOP && acting==P0Plays && fpequal(actualinv[1-playerindex(acting)],0))
		minwager = 0;

	//otherwise, this is the standard formula that FullTilt seems to follow
	else
		minwager = actualinv[1-playerindex(acting)] 
			+ mymax((double)get_property(currstrat->gettree(), settings_tag()).bblind, 
			        actualinv[1-playerindex(acting)]-actualinv[playerindex(acting)] );

	//lastly check to see if the only available action is going all-in
	if (actualpot + minwager > truestacksize)
		return truestacksize - actualpot;
	else
		return minwager;
}

#ifdef _MFC_VER

//this function is a member of BotAPI so that it has access to all the
//current state of the bot. MyWindow is also a member of BotAPI, and has public
//controls and wrapper functions to allow me to update the page from outside of its class.
void BotAPI::populatewindow(CWnd* parentwin)
{
	CString text; //used throughout

	//create window if needed

	if(MyWindow == NULL && parentwin != NULL)
		MyWindow = new DiagnosticsPage(parentwin);

	if(bot_status == INVALID) return; //can't do nothin with that!

	//set bot name

	MyWindow->SetWindowText(toCString(currstrat->getfilename()));

	//set actions

	if(playerequal(myplayer, currstrat->gettree()[currentnode].type))
	{
		vector<double> probabilities(out_degree(currentnode, currstrat->gettree()));
		currstrat->getprobs(currentgr, (currstrat->gettree())[currentnode].actioni,
			out_degree(currentnode, currstrat->gettree()), cards, probabilities);
		EIter e, elast;
		int a=0;
		for(tie(e, elast) = out_edges(currentnode, currstrat->gettree()); e!=elast; e++, a++)
		{
			MyWindow->ActButton[a].SetWindowText(toCString(actionstring(currstrat->gettree(), *e, multiplier)));
			MyWindow->ActButton[a].EnableWindow(TRUE);
			MyWindow->ActionBars[a].ShowWindow(SW_SHOW);
			int position = (int)(probabilities[a]*101.0);
			if(position == 101)
				position = 100;
			if(position < 0 || position > 100)
				REPORT("failure of probabilities");
			MyWindow->ActionBars[a].SetPos(position);
			MyWindow->ActButton[a].SetCheck(BST_UNCHECKED);
		}
		for(int a=out_degree(currentnode, currstrat->gettree()); a<MAX_ACTIONS; a++)
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
			if(currentnode == currstrat->gettreeroot()) //start of game, human is first, so reset
			{
				MyWindow->ActButton[a].SetWindowText(TEXT(""));
				MyWindow->ActButton[a].SetCheck(BST_UNCHECKED);
				MyWindow->ActionBars[a].SetPos(0);
			}
			MyWindow->ActButton[a].EnableWindow(FALSE);
		}
	}

	//set beti history

	/****** NOTE: if I want this functionality back I will implement it another way
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
	*******/
	MyWindow->BetHistory.SetWindowText(TEXT(""));

	//set pot amount

	text.Format(TEXT("$%0.2f"), 2*multiplier*perceivedpot);
	MyWindow->PotSize.SetWindowText(text);

	//set perceived invested amounts

	text.Format(TEXT("Bot: $%0.2f\nHuman: $%0.2f"), 
		multiplier*perceivedinv[myplayer],
		multiplier*perceivedinv[1-myplayer]);
	MyWindow->PerceivedInvestHum.SetWindowText(text);

	//get bin number(s) & board score

	vector<int> handi; 
	int boardi;
	currstrat->getcardmach().getindices(currentgr, cards, handi, boardi);

	//set bin number

	if(currstrat->getcardmach().getparams().usehistory)
	{
		text.Format("%d", handi[PREFLOP]+1);
		for(int i=FLOP; i<=currentgr; i++)
			text.AppendFormat(" - %d", handi[i]+1);
	}
	else
		if(currentgr==PREFLOP)
			text = TEXT("(index)");
		else
			text.Format("%d", handi[currentgr]+1);
	MyWindow->BinNumber.SetWindowText(text);

	//set bin max

	if(currstrat->getcardmach().getparams().usehistory)
	{
		text.Format(TEXT("Bin num(max %d/%d/%d/%d):"), 
				currstrat->getcardmach().getparams().bin_max[PREFLOP],
				currstrat->getcardmach().getparams().bin_max[FLOP],
				currstrat->getcardmach().getparams().bin_max[TURN],
				currstrat->getcardmach().getparams().bin_max[RIVER]);
	}
	else
		if(currentgr==PREFLOP)
			text = TEXT("Bin number:");
		else
			text.Format(TEXT("Bin number(1-%d):"),currstrat->getcardmach().getparams().bin_max[currentgr]);

	MyWindow->BinMax.SetWindowText(text);

	//set board score

	if(currstrat->getcardmach().getparams().useflopalyzer)
		if(currentgr==PREFLOP)
			text = TEXT("-");
		else
			text.Format("%d", boardi);
	else
		text = TEXT("(N/A)");
	MyWindow->BoardScore.SetWindowText(text);

	//send cards and our strategy to MyWindow, so that it can draw the cards as it needs

	//will refresh if changed.
	MyWindow->setcardstoshow(currstrat, currentgr, cards);

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
#endif //_MFC_VER

