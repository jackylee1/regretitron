#include "stdafx.h"
#include "botapi.h"
#include "strategy.h"
#include "../PokerLibrary/constants.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/cardmachine.h"
#include "DiagnosticsPage.h" //to access radios for answer
#include "../utility.h"
#include <iomanip>
#include <poker_defs.h>
#include <inlines/eval.h>
#ifndef TIXML_USE_TICPP
#define TIXML_USE_TICPP
#endif
#include "../TinyXML++/ticpp.h"

const bool debug = true;

NullLogger BotAPI::botapinulllogger;

inline bool playerequal(Player player, NodeType nodetype)
{
	return ((player == P0 && nodetype == P0Plays) || (player == P1 && nodetype == P1Plays));
}

BotAPI::BotAPI(string xmlfile, bool preload, MTRand::uint32 randseed, 
		LoggerClass & botapilogger,
		LoggerClass & botapireclog )
   : myxmlfile(xmlfile),
	 MyWindow(NULL),
     isdiagnosticson(false),
	 m_logger( botapilogger ),
	 m_reclog( botapireclog ),
#if PLAYOFFDEBUG > 0
	 actionchooser(playoff_rand), //alias to globally defined object from utility.cpp
#else
     actionchooser(randseed), //per BotAPI object, seeded with time and clock
#endif
	 mystrats(0), //initialize empty
	 currstrat(NULL),
	 actualinv(2, -1), //size is 2, initial values are -1
	 perceivedinv(2, -1), //size is 2, initial values are -1
	 cards(), //size is 0, will be resized
	 bot_status(INVALID),
	 lastchosenaction( (Action)-1 ),
	 lastchosenamount( -1 )
{ 

	//log the randomizer state for all to see

	m_logger( "\nMTRand actionchooser is using seed: " + tostr( randseed ) + '\n' );

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
			mystrats.push_back(new Strategy(xmlfile, preload, m_logger));
			m_logger( "Loaded singular bot from " + xmlfile.substr( xmlfile.find_last_of( "/\\" ) + 1 ) + '\n' );
		}
		else //we actually have a portfolio
		{
			//check version
			if( ! root->HasAttribute( "version" ) )
				REPORT( "Unsupported portfolio XML file (version tag appears to be absent)" );
			const int portfoliofileversion = root->GetAttribute< int >( "version" );
			if( portfoliofileversion != PORTFOLIO_VERSION )
				REPORT( "Unsupported portfolio XML file (file's version: " + tostr( portfoliofileversion ) + ", this executable's version: " + tostr( PORTFOLIO_VERSION ) + ").", KNOWN);

			//set working directory to portfolio directory
			string newdirectory = xmlfile.substr(0,xmlfile.find_last_of("/\\"));
			if(newdirectory != xmlfile) //has directory info
				setdirectory(xmlfile.substr(0,xmlfile.find_last_of("/\\")));

			//loop, read each filename
			Iterator<Element> child("bot");
			for(child = child.begin(root); child != child.end(); child++)
			{
				string botfile = child->GetText( );
				mystrats.push_back(new Strategy(botfile, preload, m_logger));
				m_logger( "Loaded bot from " + botfile.substr( botfile.find_last_of( "/\\" ) + 1 ) );
				if( get_property( mystrats.front( )->gettree( ), settings_tag( ) ).limit != 
						get_property( mystrats.back( )->gettree( ), settings_tag( ) ).limit )
					REPORT( "This portfolio file contains both limit and no-limit strategies.", KNOWN );
			}

			if(root->GetAttribute<unsigned>("size") != mystrats.size())
				REPORT("Portfolio only had "+tostring(mystrats.size())+" bots!",KNOWN);

			m_logger( "Loaded portfolio bot with " + tostr( mystrats.size( ) ) + " bots from " + xmlfile.substr( xmlfile.find_last_of( "/\\" ) + 1 ) + '\n' );
		}
	}
	catch(ticpp::Exception &ex) //error parsing XML
	{
		REPORT(ex.what(),KNOWN);
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
	if( & m_reclog != & botapinulllogger )
		m_reclog( "NEW GAME P" + tostr( playernum ) + " of " + tostr( sblind ) + "/" + tostr( bblind ) + " at " + tostr( stacksize ) + " with " + tostr( myhand ) );

	if(playernum != P0 && playernum != P1)
		REPORT("invalid myplayer number in BotAPI::setnewgame()");

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

		// Ways to compare two ratios A(ctual) and B(ot):
		//    error = abs( A - B ) (doesn't seem too good) 
		//    error = abs( A / B - 1 ) (looks okay)
		//    error = abs( log( A ) - log( B ) ) (looks good)
		// none of these has any effect in limit, where we don't have to compare bots larger than A to bots smaller than A
		double error = myabs( std::log( stacksizemult_i ) - std::log( stacksize / bblind ) );

		if(error < besterror)
		{
			besterror = error;
			currstrat = mystrats[i]; //pointer
		}
	}
	if(besterror == numeric_limits<double>::infinity())
	{
		REPORT("Could find no suitable strategy to play with a stacksize of "+tostring(stacksize/bblind)+" bblinds in "+(islimit()?"limit.":"no-limit."));
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

	if(fpgreatereq(sblind, stacksize))
		bot_status = WAITING_ROUND;
	else
		bot_status = WAITING_ACTION;

	lastchosenaction = (Action)-1;
	lastchosenamount = -1;

	//finally log things

	if( & m_logger != & botapinulllogger )
	{
		m_logger( "\n\n\n************" );
		m_logger( "*** New Game: Player " + tostr( myplayer ) + " (" + ( myplayer ? "SB" : "BB" )
				+ "), with SB = $" + tostr2( multiplier * actualinv[ 1 ] )
				+ " and BB = $" + tostr2( multiplier * actualinv[ 0 ] ) );
		m_logger( "**  The smaller pre-betting stacksize is: $" + tostr2( multiplier * truestacksize ) );
		m_logger( "*   Using Strategy: " + currstrat->getfilename() + '\n' );

		m_logger( ">>> Bot has hole cards: " + tostr( cards[ PREFLOP ] ) + '\n' );
	}

	processmyturn();
}

//gr - zero offset gameround
//newboard - only the NEW card(s) from this round
//newpot - this is the pot for just ONE player
void BotAPI::setnextround(int gr, CardMask newboard, double newpot/*really just needed for error checking*/)
{
	if( & m_reclog != & botapinulllogger )
	{
		switch( gr )
		{
#define NEWROUNDSTRING( gr ) case gr: m_reclog( #gr" " + tostr( newboard ) + " with pot " + tostr( newpot ) ); break;
			NEWROUNDSTRING( FLOP )
			NEWROUNDSTRING( TURN )
			NEWROUNDSTRING( RIVER )
		}
	}

	//check input parameters

	if ((int)cards.size() != gr || currentgr != gr-1 || gr < FLOP || gr > RIVER) REPORT("you set the next round at the wrong time");
	if (bot_status != WAITING_ROUND) REPORT("you must doaction before you setnextround");
	if (!fpequal(actualinv[0], actualinv[1])) REPORT("you both best be betting the same.");
	if (perceivedinv[0] != perceivedinv[1]) REPORT("perceived state is messed up");
	if (!fpequal(actualpot+actualinv[0], newpot/multiplier)) REPORT("your new pot is unexpected.");

	if( & m_logger != & botapinulllogger )
		m_logger( ">>> " + gameroundstring( gr ) + " comes: " + tostring( newboard ) 
				+ " (pot is $" + tostr2( 2 * newpot ) + ")\n" ); //newpot is actual (post multiplier), but for 1 person

	//Update the state of our private data members

	perceivedpot += perceivedinv[0]; //both investeds must be the same
	actualinv[P0] = 0;
	actualinv[P1] = 0;
	perceivedinv[P0] = 0;
	perceivedinv[P1] = 0;
	actualpot = newpot/multiplier;
	cards.push_back(newboard);
	currentgr = gr;

	if( fpequal( actualpot, truestacksize ) ) //all-in
		if( currentgr == RIVER )
			bot_status = WAITING_ENDOFGAME;
		else
			bot_status = WAITING_ROUND;
	else
		bot_status = WAITING_ACTION;

	processmyturn();
}

//wrapper function to simplify logic
//amount is only used during a call or a bet
//amount called - the total amount from ONE player to complete all bets from this round
//amount bet - the total amount wagered from ONE player, beyond what the other has put out so far
//amount fold - the total amount that player is forfeiting for this round
void BotAPI::doaction(Player pl, Action a, double amount)
{
	if( & m_reclog != & botapinulllogger )
	{
		switch( a )
		{
#define ACTIONSTRING( a ) case a: m_reclog( #a" " + tostr( amount ) + " by P" + tostr( pl ) ); break;
			ACTIONSTRING( FOLD )
			ACTIONSTRING( CALL )
			ACTIONSTRING( BET )
			ACTIONSTRING( BETALLIN )
		}
	}

	if( debug && ( & m_logger != & botapinulllogger ) )
		m_logger( "BotAPI::doaction( player=" + tostr( pl ) + ", a=" + tostr( a ) + ", amount=" + tostr( amount ) + " )"
				+ "\nmultiplier = " + tostr( multiplier )
				+ "\nmultiplied values:"
				+ "\n truestacksize = " + tostr( multiplier * truestacksize )
				+ "\n actualinv[0] = " + tostr( multiplier * actualinv[0] )
				+ "\n actualinv[1] = " + tostr( multiplier * actualinv[1] )
				+ "\n perceivedinv[0] = " + tostr( multiplier * perceivedinv[0] )
				+ "\n perceivedinv[1] = " + tostr( multiplier * perceivedinv[1] )
				+ "\n actualpot = " + tostr( multiplier * actualpot )
				+ "\n perceivedpot = " + tostr( multiplier * perceivedpot )
				+ "\n myplayer = " + tostr( myplayer )
				+ "\n currentgr = " + tostr( currentgr )
				+ "\n lastchosenaction = " + tostr( lastchosenaction )
				+ "\n lastchosenamount = " + tostr( multiplier * lastchosenamount ) 
				+ "\n");

	if(bot_status != WAITING_ACTION) REPORT("doaction called at wrong time " + tostr( (int)bot_status ) );
	if(!playerequal(pl, currstrat->gettree()[currentnode].type)) REPORT("doaction thought the other player should be acting");
	if( fpgreatereq( actualpot + actualinv[ pl ], truestacksize ) )
		REPORT( "A player who was already all-in is now doing an action." );
	if( pl == myplayer && ( a != lastchosenaction || ! fpequal( amount/multiplier, lastchosenamount ) ) )
		REPORT( "Bot failed to get filled on its orders exactly." );

	if(a == BETALLIN && pl == myplayer && offtreebetallins) 
	//if true, there would be no all-in node to find
	//we just bet an allin, instead of call, due to offtreebetallins
		return;

	switch(a)
	{
	case FOLD:  dofold (pl, amount/multiplier); break;
	case CALL:  docall (pl, amount/multiplier); break;
	case BET:   dobet  (pl, amount/multiplier); break;
	case BETALLIN: if(islimit()) REPORT("For limit, BotAPI does not use BETALLIN externally."); doallin(pl); break;
	default:    REPORT("You advanced tree with an invalid action.");
	}

	processmyturn();
}

namespace 
{
	//used for formatting in the next function
	string dollarstring( double value ){ return "$" + tostr2( value ); }
	string numberstring( double value ){ return tostr( value ); }
}

//this version of the function is just a wrapper around the other one 
// this one returns more info
void BotAPI::getactionstring( Action act, double amount, string & actionstr, int & level )
{
	// ********************************************************************
	// settings that control the behavior of the strings eminated from here
	// chipfmt: a function that formats money printed
	string (* chipfmt)( double ) = & numberstring;
	// raiseamtabove:
	// if true: output amounts to raise are ( amt I will invest - amt opp has invested )
	// if false: output amounts are total amount i am raising it to this round
	const bool raiseamtabove = false; // false for fulltilt, true for pokeracademy
	// *********************************************************************

	const double adjamount = amount / multiplier;
	const double amtaboveme = amount - multiplier * actualinv[ myplayer ];
	const double amtabovehim = amount - multiplier * actualinv[ 1 - myplayer ];

	// we need to generate a string that describes the action
	// and set the level to something that indicates the color
	// for the level,
	//   0 = fold
	//   1 = check
	//   2 = call
	//   3 = bet
	//   4 = raise

	// the folding case is easy
	if( act == FOLD )
	{
		actionstr = "Fold";
		level = 0;
	}
	// this is when no ones bet at the flop, turn, river
	// and also when i'm the big blind and the small blind only called
	else if( fpequal( actualinv[ P0 ], actualinv[ P1 ] ) )
	{
		// checking.. either from the big blind, or from zero
		if( fpequal( adjamount, actualinv[ P0 ] ) )
		{
			actionstr = "Check";
			level = 1;
		}
		// betting where i could have checked
		else
		{
			if( fpequal( 0, actualinv[ P0 ] ) )
				actionstr = "Bet " + chipfmt( amount );
			else if( raiseamtabove )
				actionstr = "Bet additional " + chipfmt( amtaboveme );
			else
				actionstr = "Bet to " + chipfmt( amount );
			level = 3;
		}
	}
	// only case left is when I already have a bet to me
	// this includes preflop when I'm the small blind facing the big blind
	else
	{
		// I am raising above the bet to me
		if( fpgreater( adjamount, actualinv[ 1 - myplayer ] ) )
		{
			if( raiseamtabove )
				actionstr = "Raise " + chipfmt( amtabovehim );
			else
				actionstr = "Raise to " + chipfmt( amount );
			level = 4;
		}
		// I am calling the bet to me
		else
		{
			actionstr = "Call " + chipfmt( amtaboveme );
			level = 2;
		}
	}

	//lastly, if we are all-in, might want to mention that
	if( fpequal( truestacksize, actualpot + adjamount ) )
		actionstr += " (all-in)";
}

//amount is the total amount of a wager when betting/raising (relative to the beginning of the round)
//          the amount aggreed upon when calling (same as above) or folding (not same as above)
//amount is equal to a POTCONTRIB style of amount. see how you defined potcontrib in the betting tree for details.
//amount is scaled and computed to be correct in the real game, regardless of what the bot sees.
//we've already chosen our action already, this is just the function that the outside world
//calls to get that answer.
Action BotAPI::getbotaction_internal( double & amount, bool doforceaction, Action forceaction, double forceamount )
{
	//error checking

	if(bot_status != WAITING_ACTION) REPORT("bot is waiting for a round or is inconsistent state");
	if(!playerequal(myplayer, currstrat->gettree()[currentnode].type)) REPORT("You asked for an answer when the bot thought action was on opp.");
	if( fpgreatereq( actualpot + actualinv[ myplayer ], truestacksize ) )
		REPORT( "You're asking me what to do when we've already all-in'd." );

	//get the probabilities

	vector<double> probabilities(out_degree(currentnode, currstrat->gettree()));
	currstrat->getprobs(currentgr, (currstrat->gettree())[currentnode].actioni,
			out_degree(currentnode, currstrat->gettree()), cards, probabilities);

	//do the logging

	if( & m_logger != & botapinulllogger )
	{
		m_logger( "Bot considers action (gameround = " + tostr( currentgr ) + ", cardsi = " + tostr( currstrat->getcardmach( ).getcardsi( currentgr, cards ) ) + ", actioni = " + tostr( ( currstrat->gettree( ) )[ currentnode ].actioni ) + ")" );
		ostringstream o;
		o << "Bot has options: ((";
		EIter e, elast;
		for(tie(e, elast) = out_edges(currentnode, currstrat->gettree()); e!=elast; e++)
			o << ' ' << actionstring(currstrat->gettree(), *e, multiplier) << ' ';
		o << ")) = < ";
		for(unsigned i=0; i<out_degree(currentnode, currstrat->gettree()); i++)
			o << setprecision( 3 ) << fixed << 100*probabilities[i] << "% ";
		m_logger( o.str( ) + ">\n" );
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
				m_logger( "       Bot was overridden by Diagnostic Window." );
			case 0: 
				break; //this is what we get if no radio button is chosen
			default: 
				REPORT("Failure of buttons.");
		}
	}
#endif

	//translate to Action type and set amount

	Action myact;
	if( doforceaction ) //we are forcing, use the force parameters in this case
	{
		myact = forceaction;
		amount = forceamount;
	}
	else //do not force, find the act and the amount from the tree edge we chose above
	{
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
				if( out_degree( currentnode, currstrat->gettree( ) ) != 2 )
					REPORT( "CallAllIn node found and not at a 2-membered node" );
				amount = multiplier * (truestacksize - actualpot);
				if( offtreebetallins )
				{
					if( islimit( ) ) REPORT( "offtreebetallins = true in limit" );
					// an opponent bet when we did not have the tree to handle it. we treated it as a BetAllin.
					// the bot has chosen to respond by "calling all-in". In reality, we are not all-in.
					// But now we will be.
					myact = BETALLIN;
				}
				else
					myact = CALL;
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
				{
					amount = multiplier * mymin(truestacksize - actualpot, (double)(currstrat->gettree()[*eanswer].potcontrib));
					if(fpgreatereq(get_property(currstrat->gettree(), settings_tag()).bblind, truestacksize)) 
						//then the stacksize must be between the sblind and the bblind size, the game consists of one choice,
						//we must have just made it, and it will end the game. It is a call all-in from the small blind.
						myact = CALL;
					else
						myact = BET;
				}
				else //all-ins would be handled above
				{
					const int & potc = ( currstrat->gettree( ) )[ * eanswer ].potcontrib;

					amount = multiplier * ( potc + actualinv[ 1 - myplayer ] - perceivedinv[ 1 - myplayer ] );

					if(fpgreater(multiplier * mintotalwager(), amount))
					{
						if( & m_logger != & botapinulllogger )
							m_logger( "Warning: adjusting bet amount from " + tostr( amount ) + " to " + tostr( multiplier * mintotalwager() ) + "." );
						amount = multiplier * mintotalwager();
					}

					if( & m_logger != & botapinulllogger )
					{
						ostringstream o;
						o << "Translating bot's bet to outside world in no-limit:\n"
							<< "   Bot perceives it is at $" << tostr2( multiplier * perceivedinv[ myplayer ] )
							<< " vs $" << tostr2( multiplier * perceivedinv[ 1 - myplayer ] )
							<< " (at stacksize $" << tostr2( multiplier * get_property( currstrat->gettree( ), settings_tag( ) ).stacksize ) << ")"
							<< " and now wishes to bet $" << tostr2( multiplier * potc ) << '\n'
							<< "   In actuality it is at $" << tostr2( multiplier * actualinv[ myplayer ] )
							<< " vs $" << tostr2( multiplier * actualinv[ 1 - myplayer ] )
							<< " (at stacksize $" << tostr2( multiplier * truestacksize ) << ")"
							<< " and will actually be betting $" << tostr2( multiplier * amount ) << "\n\n";
						m_logger( o.str( ) );
					}

					myact = BET;
				}
				break;
			default:
				REPORT("bad action"); exit(0);
		}
	}

	if( & m_logger != & botapinulllogger )
	{
		// TODO when forcing, eanswer is still set by random decision, and this string
		// will not always match what is supposed to be forced
		// to fix this, I would need a way of choosing the edge of the tree that 
		// that corresponds to my forcing parameter.
		ostringstream o; 
		o << "       Bot chose >>>   " << actionstring(currstrat->gettree(), *eanswer, multiplier) << "   <<<";
		if( fpgreater( amount, multiplier * actualinv[ myplayer ] ) )
			o << " (adding $" << tostr2( amount - multiplier * actualinv[ myplayer ] ) << " to pot)\n";
		else
			o << '\n';
		m_logger( o.str( ) );
	}

	lastchosenaction = myact;
	lastchosenamount = amount/multiplier;
	return myact;
}

void BotAPI::logendgame( bool iwin, const string & story )
{
	const double loserinvested = iwin ? actualinv[ 1 - myplayer ] : actualinv[ myplayer ];

	// pot is for one person, i will print pot from 2 people

	m_logger( ">>> Game ended. " + story + "Bot " + ( iwin ? "won" : "lost" ) + " pot of $" 
			+ tostr2( multiplier * ( 2 * actualpot + actualinv[ 0 ] + actualinv[ 1 ] ) )
			+ " ($" + tostr2( multiplier * ( actualpot + loserinvested ) ) 
			+ " " + ( iwin ? "profit" : "loss" ) + ")\n" ); 
}

void BotAPI::endofgame( )
{
	// the game has ended, and we have not seen the opponent's cards
	// what could have happened?
	// we can figure it out from the currentgr, if the bot was waiting for an action, and the invested's
	// we know we have the last currentgr by the cards that were dealt, and 
	// have handled the case of allin's elsewhere.

	if( bot_status == WAITING_ACTION )
	{
		// we were waiting for an action and the game ended before the river ==> folded
		if( currentgr != RIVER )
		{
			doaction( (Player)(1 - myplayer), FOLD, multiplier * actualinv[ 1 - myplayer ] );
			logendgame( true, "Opponent folded. " );
		}
		// we were waiting for an action and the game ended at the river (without seeing opp's cards)...
		else
		{
			m_logger( ">>> Opponent has either mucked or folded.\n" );
			logendgame( true, "Approx results: " );
		}
	}
	// we have actually done doaction and everything enough that BotAPI knows the game was over...
	else if( bot_status == WAITING_ENDOFGAME )
	{
		if( fpequal( actualinv[0], actualinv[1] ) ) //someone called, to end the game...
			//...if it was me, then the oppenent must have bet, but we didn't see his cards....
			if( lastchosenaction == CALL )
				REPORT( "Opponent bet and then mucked." );
			//...if i folded, something is wrong.
			else if( lastchosenaction == FOLD )
				REPORT( "Invalid numbers." );
			//... but it looks like the opponent called, and then mucked. (I don't think I can detect/reach this case.)
			else
				logendgame( true, "Opponent mucked. " );
		// if I had more invested, then the opponent must have just folded (I don't think I can detect/reach this case / handled above.)
		else if( fpgreater( actualinv[ myplayer ], actualinv[ 1 - myplayer ] ) ) //opp folded
			if( lastchosenaction == CALL || lastchosenaction == FOLD )
				REPORT( "I can't have just called or folded...." );
			else
				logendgame( true, "Opponent folded. " );
		else
			if( lastchosenaction != FOLD )
				REPORT( "I have to have just folded." );
			else
				logendgame( false, "Bot folded. " );
	}
	// a game shouldn't end in any other state than WAITING_ACTION or WAITING_ENDOFGAME
	else
		REPORT( "Invalid logic at endofgame in BotAPI." );

	//this has to be at the end to be shown after any actions are called (ensure we don't "return;" anywhere before here)
	m_reclog( "GAMEOVER (opponent mucks)\n" );

	bot_status = INVALID;
}

void BotAPI::endofgame( CardMask opponentcards )
{
	// the game has ended, and we saw the opponent's cards
	// assume it is a showdown.

	if( currentgr != RIVER )
		REPORT( "We saw the opponent's cards but it was not at the river..." );
	if( cards.size( ) != 4 )
		REPORT( "currentgr == RIVER but cards.size( ) != 4" );
	if( bot_status == WAITING_ACTION ) //the opponent must have called to see this showdown (assume didn't fold and show cards)
		doaction( (Player)( 1 - myplayer ), CALL, multiplier * actualinv[ myplayer ] );
	if( bot_status != WAITING_ENDOFGAME )
		REPORT( "After [possibly] calling at showdown, bot_status not endofgame." );

	CardMask fullbot, fullopp;
	CardMask_OR( fullbot, cards[1], cards[2] );
	CardMask_OR( fullbot, fullbot, cards[3] );
	fullopp = fullbot; //now both variables have the full board in them

	CardMask_OR( fullbot, fullbot, cards[0] );
	CardMask_OR( fullopp, fullopp, opponentcards );

	HandVal valopp = Hand_EVAL_N( fullopp, 7 );
	HandVal valbot = Hand_EVAL_N( fullbot, 7 );

	//do this after the action might happen above
	m_logger( ">>> Opponent shows: " + tostr( opponentcards ) );

	if( valbot > valopp )
		logendgame( true /* iwin */, "Bot won the showdown. " );
	else if( valopp > valbot )
		logendgame( false /* i dont win */, "Bot lost the showdown. " );
	else
		m_logger( ">>> Game ended. TIE! Split pot of $" 
				+ tostr2( multiplier * ( 2 * actualpot + actualinv[ 0 ] + actualinv[ 1 ] ) )
				+ "\n" );

	//this has to be at the end to be shown after any actions are called (ensure we don't "return;" anywhere before here)
	m_reclog( "GAMEOVER (opponent has " + tostr( opponentcards ) + ")\n" );

	bot_status = INVALID;
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


//this amount has been DIVIDED by multiplier
void BotAPI::dofold(Player pl, double amount)
{
	if(!fpequal(amount,actualinv[pl])) REPORT("you folded the wrong amount");
	if( !fpgreater( actualinv[1-pl], actualinv[pl] ) ) REPORT( "You can't fold unless the other player's bet more than you." );

	if( pl != myplayer )
	{
		m_logger( "       Opponent has folded.\n" );
	}

	bot_status = WAITING_ENDOFGAME;
}

//a "call" by definition ends the betting round
//this amount has been DIVIDED by multiplier
void BotAPI::docall(Player pl, double amount)
{
	if(!fpequal(amount,actualinv[1-pl])) REPORT("you called the wrong amount");

	if( pl != myplayer && ( & m_logger != & botapinulllogger ) )
	{
		if( fpequal( amount, 0 ) )
			m_logger( "       Opponent has checked.\n" );
		else
			m_logger( "       Opponent has called $" + tostr2( multiplier * amount ) + " (added $" + tostr2( multiplier * ( amount - actualinv[ pl ] ) ) + " to pot)\n" );
	}


	// find call edge and advance the tree

	EIter e, elast;
	for(tie(e, elast) = out_edges(currentnode, currstrat->gettree()); e!=elast; e++)
	{
		if( (currstrat->gettree())[*e].type == Call )
		{
			currentnode = target(*e, currstrat->gettree());
			goto foundcalledge;
		}
		else if( (currstrat->gettree())[*e].type == CallAllin )
		{
			if( ! fpequal( actualpot + amount, truestacksize ) )
				REPORT( "On a call, a CallAllin edge was found but the amount was not consitant with Calling allin."
						  " amount=" + tostr( multiplier * amount )
						+ " truestacksize=" + tostr( multiplier * truestacksize )
						+ " actualpot=" + tostr( multiplier * actualpot ) );
			currentnode = target(*e, currstrat->gettree());
			goto foundcalledge;
		}
	}
	REPORT( "Could not find an edge of type Call in the tree." );
foundcalledge:


	actualinv[pl] = actualinv[1-pl];
	perceivedinv[pl] = perceivedinv[1-pl];
	if( currentgr == 3 )
		bot_status = WAITING_ENDOFGAME;
	else
		bot_status = WAITING_ROUND;
}

//a "bet" by definition continues the betting round
//this amount has been DIVIDED by multiplier
//in no-limit, this should NOT be passed in an amount equal to stacksize (this would be all-in, and the below function would service that)
//in limit, this COULD be passed in an amount equal to stacksize. This is to match the outside world notion of all bets being "bets" in limit, rather than allin's.
void BotAPI::dobet(Player pl, double amount)
{

	//error checking 

	if( ! ( 
				amount == 0 || // either we are checking, or...
				( fpgreatereq( amount, mintotalwager( ) ) && fpgreater( truestacksize, actualpot + amount ) ) || // from the min bet to less than stacksize
				( islimit( ) && fpequal( truestacksize, actualpot + amount ) ) // ... or this is limit and it is the stacksize
		  ) )
		REPORT( "Invalid bet amount. "
				  "(amount=" + tostr( multiplier * amount )
				+ " mintotalwager( )=" + tostr( multiplier * mintotalwager( ) )
				+ " truestacksize=" + tostr( multiplier * truestacksize )
				+ " actualpot=" + tostr( multiplier * actualpot )
				+ ")" );

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

	if(besterror == numeric_limits<double>::infinity()) // -> found no node
	{
		if(islimit()) REPORT("in limit, we could not find a 'Bet' node to match some player's Bet. Fatal.");
		if(pl == myplayer) REPORT("could not find Bot's own bet action in tree. the bot HAD to have bet from the tree. we should find a bet action.");
		else m_logger( "       Note: The oppenent bet when our tree had no betting actions. off tree -> treating as all-in." );
		offtreebetallins = true;
		doallin(pl);
	}
	else
	{
		if(islimit() && !( fpequal(besterror, 0) ||
					   ( fpequal(actualpot + amount, truestacksize) && !fpequal(get_property(currstrat->gettree(), settings_tag()).stacksize, truestacksize) )))
			REPORT("in limit, we expect bets to either exactly match the tree's bets or this to be a covert all-in");
		if( & m_logger != & botapinulllogger )
		{
			if( pl != myplayer )
			{
				if( fpequal( amount, 0 ) )
					m_logger( "       Opponent has checked." );
				else if( ! fpequal( 0, mymax( actualinv[ 0 ], actualinv[ 1 ] ) ) )
					m_logger( "       Opponent has raised to $" + tostr2( multiplier * amount ) + " (added $" + tostr2( multiplier * ( amount - actualinv[ pl ] ) ) + " to pot)" );
				else
					m_logger( "       Opponent has bet $" + tostr2( multiplier * amount ) + " (added $" + tostr2( multiplier * ( amount - actualinv[ pl ] ) ) + " to pot)" );
			}
			if(!fpequal(besterror, 0))
			{
				ostringstream o;
				o << "Translating outside world's bet to bot tree\n"
					<< "   Received a bet from " << ( pl == myplayer ? "myself the bot" : "opponent" ) << " of amount $" << tostr2( multiplier * amount )
					<< " with the bot at $" << tostr2( multiplier * actualinv[ myplayer ] )
					<< " and the opponent at $" << tostr2( multiplier * actualinv[ 1 - myplayer ] )
					<< " (at stacksize $" << tostr2( multiplier * truestacksize ) << ")\n"
					<< "   Bot translates this to a bet of $" << tostr2( currstrat->gettree()[*ebest].potcontrib * multiplier )
					<< " with the bot at $" << tostr2( multiplier * perceivedinv[ myplayer ] )
					<< " and the opponent at $" << tostr2( multiplier * perceivedinv[ 1 - myplayer ] )
					<< " (at stacksize $" << tostr2( multiplier * get_property( currstrat->gettree( ), settings_tag( ) ).stacksize ) << ")\n\n";
				m_logger( o.str( ) );
			}
			else if( pl != myplayer )
				m_logger( "" ); // ensure a newline in this case to keep log spacing nice
		}
		actualinv[pl] = amount;
		perceivedinv[pl] = currstrat->gettree()[*ebest].potcontrib;
		currentnode = target(*ebest, currstrat->gettree());
	}
}

//this is a "bet" of amount all-in
//used by NO-LIMIT only.
void BotAPI::doallin(Player pl)
{
	if(islimit()) REPORT("The doallin function is for no-limit only. Limit communicates these as 'bets'.");
	EIter e, elast;
	for(tie(e, elast) = out_edges(currentnode, currstrat->gettree()); e!=elast; e++)
		if((currstrat->gettree())[*e].type == BetAllin)
			break;

	if(e==elast)
		REPORT("in no limit, we could not find a BetAllin. What gives?");

	currentnode = target(*e, currstrat->gettree());

	if(perceivedpot + (currstrat->gettree())[*e].potcontrib != get_property(currstrat->gettree(), settings_tag()).stacksize) 
		REPORT("all-in potcontrib set incorrectly");

	if( pl != myplayer )
		if( & m_logger != & botapinulllogger )
			m_logger( "       Opponent has bet all-in. (added $" + tostr2( multiplier * ( truestacksize - actualpot - actualinv[ pl ] ) ) + " to pot)" );

	actualinv[pl] = truestacksize-actualpot;
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

// pre-multiplier (internal) number returned
// potcontrib style number returned
// zero is not counted as a wager if checking
double BotAPI::mintotalwager()
{
	const int & BB = get_property( currstrat->gettree( ), settings_tag( ) ).bblind;
	const NodeType & acting = currstrat->gettree( )[ currentnode ].type;
	
	double minwager;

	//calling from the SBLIND is a special case of how much we can wager
	if( currentgr == PREFLOP && fpgreater( BB, actualinv[ playerindex( acting ) ] ) )
		minwager = BB;
	
	// limit follows a standard formula
	else if( islimit( ) )
	{
		if( currentgr == PREFLOP || currentgr == FLOP )
			minwager = actualinv[ 1 - playerindex( acting ) ] + BB;
		else
			minwager = actualinv[ 1 - playerindex( acting ) ] + 2 * BB;
	}

	// no-limit, this is the standard formula that FullTilt seems to follow
	else
	{
		minwager = actualinv[ 1 - playerindex( acting ) ] 
			+ mymax( (double)BB, actualinv[ 1 - playerindex( acting ) ] - actualinv[ playerindex( acting ) ] );
	}

	//lastly check to see if the only available action is going all-in
	if( actualpot + minwager > truestacksize )
		minwager = truestacksize - actualpot;

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

	if(MyWindow == NULL)// && parentwin != NULL)
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
	vector<int> boardi;
	currstrat->getcardmach().getindices(currentgr, cards, handi, boardi);

	//set bin number

	if(currstrat->getcardmach().getparams().usehistory)
	{
		text.Format("%d", handi[0]+1);
		for(unsigned i=1; i<handi.size( ); i++)
			text.AppendFormat(" - %d", handi[i]+1);
	}
	else
		if(currentgr==PREFLOP)
			text = TEXT("(index)");
		else
			text.Format("%d", handi[0]+1);
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

	if( boardi.empty( ) )
		text = TEXT("--");
	else
	{
		text.Format("%d", boardi[0]+1);
		for(unsigned i=1; i<boardi.size( ); i++)
			text.AppendFormat(" - %d", boardi[i]+1);
	}
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

