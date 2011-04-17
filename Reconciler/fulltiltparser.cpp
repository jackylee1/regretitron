#include "fulltiltparser.h"
#include <iostream>
#include <string>
#include <boost/regex.hpp>
using namespace std;







using namespace boost;

void stripcomma( string & str )
{
	for( string::iterator i = str.begin( ); i != str.end( ); )
		if( *i == ',' )
			i = str.erase( i );
		else
			i++;
}

#define do_regex( s, m, r ) if( ! regex_match( s, m, r ) ) \
	throw Exception( "Failed to match string '" + s + "' with regex '" + tostr( r ) + "'." );

#define myassert( s, exp ) if( ! ( exp ) ) \
	throw Exception( "Failed to '" #exp "' for string '" + s + "'." );

void FullTiltParser::Parse( istream & input )
{
	cout << endl << endl << endl;

	string s;
	string myname, oppname;
	Player myplayernum = (Player)-1;
	double myinvested = 0, oppinvested = 0;
	double onemanpot = 0;
	bool sawfirstcheck = false;
	int gameround = -1;
	double sblind = 0, bblind = 0;
	double stacksize = 0;

	getline( input, s );

	while( input.good( ) )
	{
		static smatch results;

		static regex beginstring( "Full Tilt Poker Game #([0-9]+): .*" );
		static regex flopregex( "\\*\\*\\* FLOP \\*\\*\\* \\[(..) (..) (..)\\]" );
		static regex turnregex( "\\*\\*\\* TURN \\*\\*\\* \\[.. .. ..\\] \\[(..)\\]" );
		static regex riverregex( "\\*\\*\\* RIVER \\*\\*\\* \\[.. .. .. ..\\] \\[(..)\\]" );
		static regex checkregex( "(.*) checks" );
		static regex callregex( "(.*) calls ([0-9,]+)(, and is all in)?" );
		static regex betregex( "(.*) (bets|raises to) ([0-9,]+)(, and is all in)?" );
		static regex foldregex( "(.*) folds" );
		static regex potwinregex( "(.*) (wins|ties for) the pot \\([0-9,]+\\).*" );
		static regex slowregex( "(.*) has 15 seconds left to act" );
		static regex allincadenceregex( "(.*) shows \\[(..) (..)\\]" ); //notice the lack of any trailing text

		try
		{
			if( s.empty( ) )
			{
				//	newlines between games....
				myname = oppname = "";
				myplayernum = (Player)-1;
				myinvested = oppinvested = 0;
				onemanpot = 0;
				sawfirstcheck = false;
				gameround = -1;
				sblind = bblind = 0;
				stacksize = 0;
			}
			else if( regex_match( s, beginstring ) )
			{
				//	Full Tilt Poker Game #28120278132: $1 + $0.10 Heads Up Sit & Go (218650489), Table 1 - 40/80 - Limit Hold'em - 22:29:50 ET - 2011/02/11
				do_regex( s, results, beginstring );
				uint64 gamenumber = fromstr< uint64 >( results[ 1 ] );

				//	Seat 1: Scottopoly (2,300)
				//	Seat 2: wibsy lembo (700)
				static regex seatregex( "Seat ([0-9]+): (.+) \\(([0-9,]+)\\)" );

				getline( input, s ); 
				do_regex( s, results, seatregex );
				int seata = fromstr< int >( results[ 1 ] );
				string mana = results[ 2 ], chipsa = results[ 3 ];

				getline( input, s ); 
				do_regex( s, results, seatregex );
				int seatb = fromstr< int >( results[ 1 ] );
				string manb = results[ 2 ], chipsb = results[ 3 ];
				//	wibsy lembo posts the small blind of 20
				//	Scottopoly posts the big blind of 40
				static regex sblindregex( "(.+) posts the small blind of (.*)" );
				static regex bblindregex( "(.+) posts the big blind of (.*)" );

				getline( input, s ); 
				do_regex( s, results, sblindregex );
				string mansmal = results[ 1 ];
				sblind = fromstr< double >( results[ 2 ] );

				getline( input, s ); 
				do_regex( s, results, bblindregex );
				string manbig = results[ 1 ];
				bblind = fromstr< double >( results[ 2 ] );

				//	The button is in seat #2
				static regex buttonregex( "The button is in seat #([0-9])" );
				getline( input, s ); 
				do_regex( s, results, buttonregex );
				int buttonseat = fromstr< int >( results[ 1 ] );

				//	*** HOLE CARDS ***
				//	Dealt to Scottopoly [Qc As]
				getline( input, s );
				myassert( s, s == "*** HOLE CARDS ***" );

				static regex holecardregex( "Dealt to (.*) \\[(..) (..)\\]" );
				getline( input, s );
				do_regex( s, results, holecardregex );
				string myself = results[ 1 ];
				MyCardMask cards;
				cards.Add( results[ 2 ] );
				cards.Add( results[ 3 ] );

				//plaernum, blinds, stacksize
				myname = myself;
				int myseat, oppseat;
				if( myself == mana )
				{
					myseat = seata;
					oppseat = seatb;
					oppname = manb;
				}
				else if( myself == manb )
				{
					myseat = seatb;
					oppseat = seata;
					oppname = mana;
				}
				else throw Exception( "myname not found in seats" );

				if( buttonseat == myseat )
				{
					myplayernum = P1;
					myassert( s, myname == mansmal );
					myassert( s, oppname == manbig );
					myinvested = sblind;
					oppinvested = bblind;
				}
				else if( buttonseat == oppseat )
				{
					myplayernum = P0;
					myassert( s, myname == manbig );
					myassert( s, oppname == mansmal );
					myinvested = bblind;
					oppinvested = sblind;
				}
				else throw Exception( "buttonseat not found in seats" );

				onemanpot = 0;
				sawfirstcheck = true;
				gameround = PREFLOP;

				stripcomma( chipsa );
				stripcomma( chipsb );
				stacksize = mymin( fromstr< double >( chipsa ), fromstr< double >( chipsb ) );

				m_reconciler.setnewgame( gamenumber, myplayernum, cards, sblind, bblind, stacksize );
			}
			else if( regex_match( s, flopregex ) )
			{
				//flop
				//	*** FLOP *** [2s 3h 8c]
				do_regex( s, results, flopregex );
				MyCardMask cards;
				cards.Add( results[ 1 ] );
				cards.Add( results[ 2 ] );
				cards.Add( results[ 3 ] );
				myassert( s, fpequal( myinvested, oppinvested ) );
				onemanpot += myinvested;
				myinvested = oppinvested = 0;
				m_reconciler.setnextround( FLOP, cards, onemanpot );
				sawfirstcheck = false;
				gameround = FLOP;
			}
			else if( regex_match( s, turnregex ) )
			{
				//turn
				//	*** TURN *** [2s 3h 8c] [Jc]
				do_regex( s, results, turnregex );
				MyCardMask cards;
				cards.Add( results[ 1 ] );
				myassert( s, fpequal( myinvested, oppinvested ) );
				onemanpot += myinvested;
				myinvested = oppinvested = 0;
				m_reconciler.setnextround( TURN, cards, onemanpot );
				sawfirstcheck = false;
				gameround = TURN;
			}
			else if( regex_match( s, riverregex ) )
			{
				//river
				//	*** RIVER *** [2s 3h 8c Jc] [8d]
				do_regex( s, results, riverregex );
				MyCardMask cards;
				cards.Add( results[ 1 ] );
				myassert( s, fpequal( myinvested, oppinvested ) );
				onemanpot += myinvested;
				myinvested = oppinvested = 0;
				m_reconciler.setnextround( RIVER, cards, onemanpot );
				sawfirstcheck = false;
				gameround = RIVER;
			}
			else if( regex_match( s, checkregex ) )
			{
				//check
				//	Scottopoly checks
				do_regex( s, results, checkregex );
				if( results[ 1 ] == myname )
					m_reconciler.doaction( myplayernum, sawfirstcheck ? CALL : BET, myinvested );
				else if( results[ 1 ] == oppname )
					m_reconciler.doaction( (Player)( 1 - myplayernum ), sawfirstcheck ? CALL : BET, oppinvested );
				else
					throw Exception( "Player not found for check '" + results[ 1 ] + "'" );
				sawfirstcheck = true;
			}
			else if( regex_match( s, callregex ) )
			{
				//call
				//	Scottopoly calls 30
				do_regex( s, results, callregex );
				double amount = fromstr< double >( results[ 2 ] );
				if( results[ 1 ] == myname )
				{
					const bool isbet = ( gameround == PREFLOP && fpequal( myinvested, sblind ) && fpgreater( stacksize, oppinvested ) );
					myinvested += amount;
					myassert( s, fpequal( myinvested, oppinvested ) );
					m_reconciler.doaction( myplayernum, isbet ? BET : CALL, myinvested );
				}
				else if( results[ 1 ] == oppname )
				{
					const bool isbet = ( gameround == PREFLOP && fpequal( oppinvested, sblind ) && fpgreater( stacksize, myinvested ) );
					oppinvested += amount;
					myassert( s, fpequal( myinvested, oppinvested ) );
					m_reconciler.doaction( (Player)( 1 - myplayernum ), isbet ? BET : CALL, oppinvested );
				}
				else
					throw Exception( "Player not found for call '" + results[ 1 ] + "'" );
			}
			else if( regex_match( s, betregex ) )
			{
				//bet
				//	wibsy lembo bets 30
				//raise
				//	wibsy lembo raises to 60
				//	wibsy lembo raises to 50, and is all in
				do_regex( s, results, betregex );
				double amount = mymin( fromstr< double >( results[ 3 ] ), stacksize - onemanpot );
				if( results[ 1 ] == myname )
				{
					myinvested = amount;
					myassert( s, fpgreater( myinvested, oppinvested ) );
					m_reconciler.doaction( myplayernum, BET, myinvested );
				}
				else if( results[ 1 ] == oppname )
				{
					oppinvested = amount;
					myassert( s, fpgreater( oppinvested, myinvested ) );
					m_reconciler.doaction( (Player)( 1 - myplayernum ), BET, oppinvested );
				}
				else
					throw Exception( "Player not found for call '" + results[ 1 ] + "'" );
			}
			else if( regex_match( s, foldregex ) )
			{
				//fold
				//	wibsy lembo folds
				do_regex( s, results, foldregex );

				if( results[ 1 ] == myname )
				{
					myassert( s, fpgreater( oppinvested, myinvested ) );

					//	Uncalled bet of 80 returned to Scottopoly
					regex uncalledregex( "Uncalled bet of ([0-9,]+) returned to " + oppname );
					getline( input, s );
					do_regex( s, results, uncalledregex );

					//	Scottopoly mucks
					regex muckedregex( oppname + " mucks" );
					getline( input, s );
					do_regex( s, results, muckedregex );

					m_reconciler.doaction( myplayernum, FOLD, myinvested );
				}
				else if( results[ 1 ] == oppname )
				{
					myassert( s, fpgreater( myinvested, oppinvested ) );

					regex uncalledregex( "Uncalled bet of ([0-9,]+) returned to " + myname );
					getline( input, s );
					do_regex( s, results, uncalledregex );

					regex muckedregex( myname + " mucks" );
					getline( input, s );
					do_regex( s, results, muckedregex );

					m_reconciler.doaction( (Player)( 1 - myplayernum ), FOLD, oppinvested );
				}
				m_reconciler.endofgame( );
			}
			else if( s == "*** SHOW DOWN ***" )
			{
				//showdown
				//	*** SHOW DOWN ***
				//	Scottopoly shows [3s Ad] a pair of Threes
				//	taksa0a shows [3h As] a pair of Threes

				//	*** SHOW DOWN ***
				//	wibsy lembo shows [3c Js] two pair, Jacks and Eights
				//	Scottopoly shows [Jh 7d] two pair, Jacks and Eights

				//	*** SHOW DOWN ***
				//	wibsy lembo shows [6s 2d] a full house, Sixes full of Twos
				//	Scottopoly mucks
				string s1, s2;
				getline( input, s1 );
				getline( input, s2 );

				//find the opponent's line of the next two lines...
				if( s1.substr( 0, oppname.length( ) ) == oppname )
					s = s1;
				else if( s2.substr( 0, oppname.length( ) ) == oppname )
					s = s2;
				else
					throw Exception( "Opponent not found at showdown!" );

				//see if he has cards or not
				if( s == oppname + " mucks" )
					m_reconciler.endofgame( );
				else
				{
					regex oppcardregex( oppname + " shows \\[(..) (..)\\] .*" );
					do_regex( s, results, oppcardregex );
					MyCardMask cards;
					cards.Add( results[ 1 ] );
					cards.Add( results[ 2 ] );
					m_reconciler.endofgame( cards );
				}
			}
			else if( regex_match( s, allincadenceregex ) )
			{
				//	wibsy lembo shows [Ts 7d]
				//	Scottopoly shows [7s 6s]
				//	*** FLOP *** [3c 9s 8h]
				//	*** TURN *** [3c 9s 8h] [Qh]
				//	*** RIVER *** [3c 9s 8h Qh] [Qc]

				myassert( s, myinvested == oppinvested );
				onemanpot += myinvested;
				myinvested = oppinvested = 0;

				MyCardMask oppcards;

				do_regex( s, results, allincadenceregex );
				if( results[ 1 ] == oppname )
				{
					oppcards.Add( results[ 2 ] );
					oppcards.Add( results[ 3 ] );
				}

				getline( input, s );

				do_regex( s, results, allincadenceregex );
				if( results[ 1 ] == oppname )
				{
					oppcards.Add( results[ 2 ] );
					oppcards.Add( results[ 3 ] );
				}

				myassert( s, oppcards.NumCards( ) == 2 );

				getline( input, s );

				if( regex_match( s, regex( "Uncalled bet of ([0-9,]+) returned to (.*)" ) ) )
					getline( input, s );

				if( regex_match( s, results, flopregex ) )
				{
					MyCardMask cards;
					cards.Add( results[ 1 ] );
					cards.Add( results[ 2 ] );
					cards.Add( results[ 3 ] );
					m_reconciler.setnextround( FLOP, cards, onemanpot );
					getline( input, s );
				}

				if( regex_match( s, results, turnregex ) )
				{
					MyCardMask cards;
					cards.Add( results[ 1 ] );
					m_reconciler.setnextround( TURN, cards, onemanpot );
					getline( input, s );
				}

				if( regex_match( s, results, riverregex ) )
				{
					MyCardMask cards;
					cards.Add( results[ 1 ] );
					m_reconciler.setnextround( RIVER, cards, onemanpot );
					getline( input, s );
				}

				//	wibsy lembo shows a pair of Queens
				//	Scottopoly shows a pair of Queens
				myassert( s, regex_match( s, regex( "(.*) shows .*" ) ) );
				getline( input, s );
				myassert( s, regex_match( s, regex( "(.*) shows .*" ) ) );

				m_reconciler.endofgame( oppcards );
			}
			else if( regex_match( s, potwinregex ) )
			{
				//potwin
				//	Scottopoly wins the pot (420) with two pair, Jacks and Eights

				//	wibsy lembo wins the pot (640) with a full house, Sixes full of Twos

				//	Scottopoly wins the pot (180)

				//	Scottopoly ties for the pot (560) with a pair of Threes
				//	taksa0a ties for the pot (560) with a pair of Threes
			}
			else if( s == "*** SUMMARY ***" )
			{
				//summary

				//	*** SUMMARY ***
				//	Total pot 420 | Rake 0
				//	Board: [2s 3h 8c Jc 8d]
				//	Seat 1: Scottopoly (big blind) showed [Jh 7d] and won (420) with two pair, Jacks and Eights
				//	Seat 2: wibsy lembo (small blind) showed [3c Js] and lost with two pair, Jacks and Eights

				//	*** SUMMARY ***
				//	Total pot 180 | Rake 0
				//	Board: [Jd 9h 2h]
				//	Seat 1: Scottopoly (big blind) collected (180), mucked
				//	Seat 2: wibsy lembo (small blind) folded on the Flop
				getline( input, s );
				myassert( s, regex_match( s, regex( "Total pot .*" ) ) );
				getline( input, s );
				if( regex_match( s, regex( "Board: .*" ) ) ) //sometimes there is no board..
					getline( input, s );
				myassert( s, regex_match( s, regex( "Seat 1: .*" ) ) );
				getline( input, s );
				myassert( s, regex_match( s, regex( "Seat 2: .*" ) ) );
			}
			else if( regex_match( s, slowregex ) )
			{
				//slow
				//	Scottopoly has 15 seconds left to act
				do_regex( s, results, slowregex );
				cout << results[ 1 ] << " is slow..." << endl;
			}
			else
				throw Exception( "Failed to parse: '" + s + "'." );


			getline( input, s );
		}
		catch( std::exception & e )
		{
			logger( string( "Error: " ) + e.what( ) );
			logger( "Game state reset, waiting for new game to start..." );
			while( input.good( ) && ! regex_match( s, beginstring ) )
				getline( input, s );
		}

	} //this ends the while loop

	cout << "Succeeded in parsing the whole file." << endl;
}

