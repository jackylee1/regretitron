#include "stdafx.h"
#include "treenolimit.h"
#include "constants.h"
#include "../utility.h"
#include <sstream>
#include <iomanip>
using namespace boost;

treesettings_t makelimittreesettings( int sblind, int bblind, int stacksize )
{
	// in this data struct, all I need to set (that is relevant for limit)
	//is smallblind, bigblind, and stacksize (all are ints)
	//for no-limit, I have all my old settings I used to use for no-limit below
	// ( this all used to be in solveparams.h )
	treesettings_t limitsettings = 
	{
		sblind, bblind, //smallblind, bigblind
		{0,0,0,0,0,0},   //B1 - B6
		{{0,0,0,0,0,0},  //R11 - R16
		 {0,0,0,0,0,0},  //R21 - R26
		 {0,0,0,0,0,0},  //R31 - R36
		 {0,0,0,0,0,0},  //R41 - R46
		 {0,0,0,0,0,0},  //R51 - R56
		 {0,0,0,0,0,0}}, //R61 - R66
		stacksize, false, true //stacksize, pushfold, limit
	};
	return limitsettings;

	//I keep the following settings here for posterity, as they used to be in solveparams.h
	// They all apply to no-limit, and represent a historical record 
	// of the settings that I was using at one time for that game
#if 0
#if PUSHFOLD

	SB, BB, //smallblind, bigblind
	{99,99,99,99,99,99},   //B1 - B6
	{{99,99,99,99,99,99},  //R11 - R16
	 {99,99,99,99,99,99},  //R21 - R26
	 {99,99,99,99,99,99},  //R31 - R36
	 {99,99,99,99,99,99},  //R41 - R46
	 {99,99,99,99,99,99},  //R51 - R56
	 {99,99,99,99,99,99}}, //R61 - R66
	SS, true, false //stacksize, pushfold, limit

#elif SS<=8 //all possible options are represented

	SB, BB,
	{2, 4, 6, 8, 10,12}, //Bn
	{{4, 6, 8, 10,12,14}, //R1n
	 {8, 12,16,20,24,28}, //R2n
	 {12,18,24,30,36,42}, //R3n
	 {16,24,32,40,48,56}, //R4n
	 {20,30,40,50,60,70}, //R5n
	 {24,36,48,60,72,84}}, //R6n
	SS, false, false

#elif SS==13
	
#if 1 //most options possible
	SB, BB,
	{2, 4, 8, 12,16,20},
	{{4, 6, 10,14,18,22},
	 {8, 16,20,24,28,32},
	 {16,24,32,40,48,99},
	 {24,36,48,99,99,99},
	 {32,48,99,99,99,99},
	 {40,99,99,99,99,99}},
	SS, false, false
#else //smaller tree (same as used for prototype?)
	SB, BB,
	{2, 6, 12,20,99,99}, //B
	{6, 12,20,99,99,99}, //R1
	{12,18,99,99,99,99}, //R2
	{24,99,99,99,99,99}, //R3
	{99,99,99,99,99,99}, //R4
	{99,99,99,99,99,99}, //R5
	{99,99,99,99,99,99}, //R6
	SS, false, false
#endif

#elif SS>=25 && SS<35

	SB, BB,
	{2, 6, 12,20,34,48}, //B
	{6, 12,20,30,42,56}, //R1
	{12,20,32,44,58,99}, //R2
	{20,34,46,60,99,99}, //R3
	{34,48,60,99,99,99}, //R4
	{48,60,99,99,99,99}, //R5
	{60,99,99,99,99,99}, //R6
	SS, false, false

#elif SS>=35

	SB, BB,
	4, 12,24,36,50,66,
	12,24,36,50,66,99,
	24,36,50,66,99,99,
	36,50,66,99,99,99,
	50,66,99,99,99,99,
	66,99,99,99,99,99,
	99,99,99,99,99,99,
	SS, false, false

#endif
#endif

}

void branchtree( int gr, int pot, int betagreed, int bettomeet, bool isfirstinround, const Vertex & thisnode, BettingTree & tree, const Vertex & T0, const Vertex & T1, const Vertex & TSD )
{
	const int & SB = get_property( tree, settings_tag( ) ).sblind;
	const int & BB = get_property( tree, settings_tag( ) ).bblind;
	const int & SS = get_property( tree, settings_tag( ) ).stacksize;
	const bool & islimit = get_property( tree, settings_tag( ) ).limit;

	if( gr < PREFLOP || gr > RIVER )
		REPORT( "invalid gr value in branchtree" );
	if( SS <= SB )
		REPORT( "No game possible with SS <= SB" );
	if( pot < 0 || betagreed < 0 || bettomeet < 0 || pot + bettomeet > SS )
		REPORT( "invalid numbers passed to branchnolimit" );
	if( bettomeet < betagreed )
		REPORT( "Can't bet less than contributed already" );
	if( isfirstinround && (
			( gr == 0 && ( ( betagreed != SB && bettomeet != mymin( SS, BB ) ) || tree[ thisnode ].type != P1Plays ) ) ||
			( gr > 0 && ( ( betagreed != 0 && bettomeet != 0 ) || tree[ thisnode ].type != P0Plays ) ) ) )
		REPORT( "Betting amounts or player types are incorrect for the first node in the round" );
	if( tree[ thisnode ].type != P0Plays && tree[ thisnode ].type != P1Plays )
		REPORT( "Invalid node type in branchtree" );

	const NodeType otherpl = ( tree[ thisnode ].type == P0Plays ? P1Plays : P0Plays );

	if( bettomeet > betagreed )
	{
		// add Fold edge
		add_edge( thisnode, ( otherpl == P0Plays ? T0 : T1 ), EdgeProp( Fold, betagreed ), tree );
	}

	if( pot + bettomeet == SS )
	{
		// add CallAllin edge
		add_edge( thisnode, TSD, EdgeProp( CallAllin, bettomeet ), tree );
	}
	else if( isfirstinround )
	{
		// add Bet edge with bettomeet potcontrib
		Vertex v = add_vertex( NodeProp( otherpl ), tree );
		add_edge( thisnode, v, EdgeProp( Bet, bettomeet ), tree );
		branchtree( gr, pot, bettomeet, bettomeet, false, v, tree, T0, T1, TSD );
	}
	else
	{
		// add Call edge
		if( gr == RIVER )
			add_edge( thisnode, TSD, EdgeProp( Call, bettomeet ), tree );
		else
		{
			Vertex v = add_vertex( NodeProp( P0Plays ), tree );
			add_edge( thisnode, v, EdgeProp( Call, bettomeet ), tree );
			branchtree( gr + 1, pot + bettomeet, 0, 0, true, v, tree, T0, T1, TSD );
		}
	}


	// if more betting is possible, add bets and a BetAllin node

	const int betincrement = ( islimit ? ( gr >= TURN ? 2 * BB : BB ) : mymax( BB, bettomeet - betagreed ) );

	if( pot + bettomeet < SS && ( ! islimit || bettomeet < 4 * betincrement ) )
	{
		for( int aboveamount = betincrement; pot + bettomeet + aboveamount < SS; aboveamount += betincrement )
		// for( int aboveamount = betincrement; pot + bettomeet + aboveamount < SS; aboveamount *= 2 )
		{
			// add Betting node
			Vertex v = add_vertex( NodeProp( otherpl ), tree );
			add_edge( thisnode, v, EdgeProp( Bet, bettomeet + aboveamount ), tree );
			branchtree( gr, pot, bettomeet, bettomeet + aboveamount, false, v, tree, T0, T1, TSD );

			if( islimit )
				return; //if it's limit and we added a betting node here, return. otherwise, allow the BetAllin to be added.
		}

		// add BetAllin node
		Vertex v = add_vertex( NodeProp( otherpl ), tree );
		add_edge( thisnode, v, EdgeProp( BetAllin, SS - pot ), tree );
		branchtree( gr, pot, bettomeet, SS - pot, false, v, tree, T0, T1, TSD );
	}
}

void verifytree( int gr, int pot, int bettomeet, const Vertex & node, const BettingTree & tree, const Vertex & T0, const Vertex & T1, const Vertex & TSD, const int maxacts )
{
	try
	{

		const int & SB = get_property( tree, settings_tag( ) ).sblind;
		//const int & BB = get_property( tree, settings_tag( ) ).bblind;
		const int & SS = get_property( tree, settings_tag( ) ).stacksize;

		// simple error checks

		if( tree[ T0 ].type != TerminalP0Wins
				|| tree[ T1 ].type != TerminalP1Wins
				|| tree[ TSD ].type != TerminalShowDown )
			REPORT( "T0, T1, or TSD are incorrect" );
		if( SS <= SB )
			REPORT( "No game possible with SS <= SB" );
		if( gr < PREFLOP || gr > RIVER )
			REPORT( "invalid gr value in verifytree" );
		if( pot + bettomeet > SS )
			REPORT( "too big pot + bettomeet" );
		if( pot < 0 || bettomeet < 0 )
			REPORT( "invalid numbers passed to branchnolimit" );
		if( tree[ node ].type != P0Plays && tree[ node ].type != P1Plays )
			REPORT( "Invalid node type in branchnolimit" );
		if( maxacts > MAX_ACTIONS )
			REPORT( "You are using more actions than the rest of the code!" );
		if( out_degree( node, tree ) < 2 || (int)out_degree( node, tree ) > maxacts )
			REPORT( "you have a " + tostring( out_degree( node, tree ) ) + " membered node while max_actions is " + tostring( maxacts ) + "!" );

		// do more complicated checks and call this function again recursively

		int numcalls = 0;
		int numfolds = 0;
		int numbetallins = 0;
		int lastedgepotcontrib = -1;
		EIter e, elast;
		for(tie(e,elast) = out_edges(node, tree); e!=elast; e++)
		{
			if( tree[ *e ].potcontrib <= lastedgepotcontrib )
				REPORT( "potcontribs are not strictly increasing" );
			lastedgepotcontrib = tree[ *e ].potcontrib;

			switch(tree[*e].type)
			{
				case Bet:
					if( tree[ *e ].potcontrib < bettomeet || pot + tree[ *e ].potcontrib >= SS )
						REPORT( "Potcontrib not correct for a bet" );
					if( tree[ node ].type == tree[ target( *e, tree ) ].type )
						REPORT( "verifytree found same player after a bet" );
					verifytree( gr, pot, tree[ *e ].potcontrib, target( *e, tree ), tree, T0, T1, TSD, maxacts );
					break;

				case BetAllin:
					numbetallins++;
					if( pot + tree[ *e ].potcontrib != SS )
						REPORT( "Potcontrib not correct for a BetAllin" );
					if( tree[ node ].type == tree[ target( *e, tree ) ].type )
						REPORT( "verifytree found same player after a bet" );
					verifytree( gr, pot, tree[ *e ].potcontrib, target( *e, tree ), tree, T0, T1, TSD, maxacts );
					break;

				case Call:
					numcalls++;
					if( bettomeet != tree[ *e ].potcontrib )
						REPORT( "Potcontrib not correct for a Call" );
					if( gr != RIVER )
					{
						if( tree[ target( *e, tree ) ].type != P0Plays )
							REPORT( "verifytree found incorrect player at start of next round" );
						verifytree( gr + 1, pot + tree[*e].potcontrib, 0, target(*e, tree), tree, T0, T1, TSD, maxacts);
					}
					else if( target( *e, tree ) != TSD || tree[ target( *e, tree ) ].type != TerminalShowDown )
						REPORT( "Call edge at the river does not point to TerminalShowDown node" );
					break;

				case CallAllin:
					numcalls++;
					if( bettomeet != tree[ *e ].potcontrib || pot + bettomeet != SS )
						REPORT( "Potcontrib not correct for a CallAllin" );
					if( target( *e, tree ) != TSD || tree[ target( *e, tree ) ].type != TerminalShowDown )
						REPORT( "CallAllin edge does not point to TerminalShowDown node" );
					break;

				case Fold:
					numfolds++;
					if( tree[ *e ].potcontrib >= bettomeet )
						REPORT( "Potcontrib not correct for a Fold" );
					if( ! ( ( tree[ node ].type == P1Plays && target( *e, tree ) == T0 && tree[ target( *e, tree ) ].type == TerminalP0Wins )
								|| ( tree[ node ].type == P0Plays && target( *e, tree ) == T1 && tree[ target( *e, tree ) ].type == TerminalP1Wins ) ) )
						REPORT( "Fold edge does not point to correct type of TerminalPxWins node." );
					break;

				default:
					REPORT( "unknown edge type in tree" );
			}

			// ensure at most one call or callallin, at most one fold, at most one betallin

			if( numcalls != 0 && numcalls != 1 )
				REPORT( "Invalid number of calling edges from this node" );
			if( numfolds != 0 && numfolds != 1 )
				REPORT( "Invalid number of folding edges from this node" );
			if( numbetallins != 0 && numbetallins != 1 )
				REPORT( "Invalid number of betallin edges from this node" );

		}
	}
	catch( std::exception & e )
	{
		throw Exception( "gr=" + tostr( gr ) + " pot=" + tostr( pot ) + " bettomeet=" + tostr( bettomeet ) + " acting=" + tostr( tree[ node ].type ) + "\n" + e.what( ) );
	}
}

void assignactioni( int gr, const Vertex & node, BettingTree & tree )
{
	tree[ node ].actioni = get_property( tree, maxorderindex_tag( ) )[ gr ][ out_degree( node, tree ) ]++;

	EIter e, elast;
	for( tie( e, elast ) = out_edges( node, tree ); e != elast; e++ )
	{
		switch( tree[ *e ].type )
		{
			case Bet:
			case BetAllin:
				assignactioni( gr, target( *e, tree ), tree );
				break;

			case Call:
				if( gr != RIVER )
					assignactioni( gr + 1, target( *e, tree ), tree );
				break;
			default:
				break;
		}
	}
}

Vertex createtree( BettingTree & tree, const int max_actions, LoggerClass & treelogger )
{
	// erase whatevers there

	tree.clear( ); //shouldn't erase properties
	get_property( tree, maxorderindex_tag( ) ).resize( extents[ 4 ][ multi_array_types::extent_range( 2, MAX_ACTIONS + 1 ) ] );
	for( int i = 0; i < 4; i++ ) 
		for( int j = 2; j <= MAX_ACTIONS; j++ ) 
			get_property( tree, maxorderindex_tag( ) )[ i ][ j ] = 0;

	const int & SB = get_property( tree, settings_tag( ) ).sblind;
	const int & BB = get_property( tree, settings_tag( ) ).bblind;
	const int & SS = get_property( tree, settings_tag( ) ).stacksize;
	const bool & islimit = get_property( tree, settings_tag( ) ).limit;

	//create terminal nodes
	Vertex T0 = add_vertex( NodeProp( TerminalP0Wins ), tree );
	Vertex T1 = add_vertex( NodeProp( TerminalP1Wins ), tree );
	Vertex TSD = add_vertex( NodeProp( TerminalShowDown ), tree );

	//create the root
	Vertex root = add_vertex( NodeProp( P1Plays ), tree );

	//recursively add the rest of the tree
	branchtree( PREFLOP, 0, SB, mymin( BB, SS ), true, root, tree, T0, T1, TSD );

	//extensively error check once again the tree
	verifytree( PREFLOP, 0, mymin( BB, SS ), root, tree, T0, T1, TSD, max_actions );

	//assign maxorderindex values and actioni's recursively
	assignactioni( PREFLOP, root, tree );

	treelogger( string( islimit ? "" : "No-" ) + "Limit tree has " + tostr( num_vertices( tree ) - 3 ) + " nodes, and 3 terminal nodes also. " );

	//return the root
	return root;
}

//takes an action index, the gameround, a pointer to the relevant
//betting tree node, and a multiplier.
//returns a string that reports info on what that action represents.
//it's units are the native ones for the poker engine (sblind = 1) times
//the multiplier.
string actionstring(const BettingTree &tree, const Edge &edge, double multiplier)
{
	ostringstream str;
	str << fixed << setprecision(2);

	switch(tree[edge].type)
	{
	case Fold:
		str << "fold";
		break;
	case Call:
		if( fpequal( 0, tree[ edge ].potcontrib ) )
			str << "check";
		else
			str << "call $" << multiplier*(tree[edge].potcontrib);
		break;
	case Bet:
		if( fpequal( 0, tree[ edge ].potcontrib ) )
			str << "check";
		else
			str << "bet $" << multiplier*(tree[edge].potcontrib);
		break;
	case CallAllin:
		str << "call all-in";
		break;
	case BetAllin:
		str << "bet all-in";
		break;
	}

	return str.str();
}

