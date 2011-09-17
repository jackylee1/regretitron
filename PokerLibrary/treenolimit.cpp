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

//
//  functions to create a bitchin limit tree.
//

void killtree(Vertex node, BettingTree &tree)
{
	switch(tree[node].type)
	{
		case TerminalP0Wins:
		case TerminalP1Wins:
		case TerminalShowDown:
			return;
		case NodeError:
			REPORT("NodeError");
		case P0Plays:
		case P1Plays:
			break;
	}

	EIter e, elast;
	for(tie(e,elast) = out_edges(node, tree); e!=elast; )
	{
		Vertex tokill = target(*e, tree);
		remove_edge(e++, tree); // must have e++ here. must remove an edge that e does not point to. 
		killtree(tokill, tree);
	}
	remove_vertex(node, tree);
}

void prunelimit(int gr, int pot, int prev_potcontrib, BettingTree &tree, const Vertex &node, 
		const Vertex &T0, const Vertex &T1, const Vertex &TSD, const int maxacts)
{
	//make sure this is a player node

	switch(tree[node].type)
	{
		case P0Plays:
		case P1Plays:
			break;
		default:
			REPORT("tree broke");
	}

	if(get_property(tree, settings_tag()).stacksize <= get_property(tree, settings_tag()).sblind)
	{
		REPORT("I don't think a game is possible with that stacksize. No decisions are possible.");
	}

	//remove edges that put us all in

	EIter e, elast;
	const unsigned total = out_degree(node, tree);
	unsigned removed = 0;
	for(tie(e,elast) = out_edges(node, tree); e!=elast; )
	{
		if(pot + tree[*e].potcontrib >= get_property(tree, settings_tag()).stacksize)
		{
			switch(tree[*e].type)
			{
				case Fold:
				case Call:
					REPORT("these should be afordable or have been removed from the tree.");
				case Bet:
					killtree(target(*e, tree), tree); //comment out if vertex removal voids iterators
					remove_edge(e++, tree); // must have e++ here. must remove an edge that e does not point to. 
					removed++;
					break;
				case BetAllin: //leave these as is
				case CallAllin:
					e++;
					break;
			}
		}
		else
		{
			e++;
		}
	}

	if(removed != total - out_degree(node, tree)) REPORT("baaaad");

	//replace them with a single all-in sub-tree

	if(removed > 0)
	{
		if(get_property(tree, settings_tag()).stacksize > 24 * get_property(tree, settings_tag()).bblind)
			REPORT("stacksize is big enough that we should be playing deep-stacked limit");

		Vertex x;

		if(get_property(tree, settings_tag()).stacksize <= get_property(tree, settings_tag()).bblind)
		{ 
			//we are instantly all in in this, and only this, case. 
			add_edge(node, TSD, EdgeProp(CallAllin, get_property(tree, settings_tag()).stacksize - pot), tree);
		}
		else
		{
			//here, our opponent has the ability to respond.
			if(tree[node].type == P0Plays)
			{
				x = add_vertex(NodeProp(P1Plays), tree);
				add_edge(x, T0, EdgeProp(Fold, prev_potcontrib), tree);
			}
			else 
			{
				x = add_vertex(NodeProp(P0Plays), tree);
				add_edge(x, T1, EdgeProp(Fold, prev_potcontrib), tree);
			}

			add_edge(node, x, EdgeProp(BetAllin, get_property(tree, settings_tag()).stacksize - pot), tree);
			add_edge(x, TSD, EdgeProp(CallAllin, get_property(tree, settings_tag()).stacksize - pot), tree);
		}
	}

	//okay, now iterate through them like a normal person and get on goin'!

	if(maxacts > MAX_ACTIONS)
		REPORT("You are using more actions than the rest of the code!");
	if(out_degree(node, tree) < 2 || (int)out_degree(node, tree) > maxacts)
		REPORT("you have a "+tostring(out_degree(node, tree))+" membered node while max_actions is "+tostring(maxacts)+"!");
	tree[node].actioni = get_property(tree, maxorderindex_tag())[gr][out_degree(node, tree)]++;

	for(tie(e,elast) = out_edges(node, tree); e!=elast; e++)
	{
		switch(tree[*e].type)
		{
			case Bet:
			case BetAllin:
				prunelimit(gr, pot, tree[*e].potcontrib, tree, target(*e, tree), T0, T1, TSD, maxacts);
				break;
			case Call:
				if(gr < 3)
					prunelimit(gr+1, pot+tree[*e].potcontrib, 0, tree, target(*e, tree), T0, T1, TSD, maxacts);
			default: //this is river Call's, CallAllin's and Fold's
				break; //they all lead to terminal nodes.
		}
	}
}


//add on a limit tree for the flop/turn/river rounds
Vertex addlimit(int gr, BettingTree &tree, const Vertex &T0, const Vertex &T1, const Vertex &TSD)
{
	if(gr > 3) return TSD;
	if(gr < 1) REPORT("bad tree creation!!");

	Vertex n0 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n1 = add_vertex(NodeProp(P1Plays), tree);
	Vertex n2 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n3 = add_vertex(NodeProp(P1Plays), tree);
	Vertex n4 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n5 = add_vertex(NodeProp(P1Plays), tree);
	Vertex n6 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n7 = add_vertex(NodeProp(P1Plays), tree);
	Vertex n8 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n9 = add_vertex(NodeProp(P1Plays), tree);

	const int b = get_property(tree, settings_tag()).bblind * (gr == 1 ? 1 : 2); //use big-bet for turn and river

	add_edge(n0,n5,EdgeProp(Bet,0), tree);
	add_edge(n0,n1,EdgeProp(Bet,1*b), tree);

	add_edge(n1,T0,EdgeProp(Fold,0), tree);
	add_edge(n1, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,1*b), tree);
	add_edge(n1,n2,EdgeProp(Bet,2*b), tree);

	add_edge(n2,T1,EdgeProp(Fold,1*b), tree);
	add_edge(n2, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,2*b), tree);
	add_edge(n2,n3,EdgeProp(Bet,3*b),tree);

	add_edge(n3,T0,EdgeProp(Fold,2*b),tree);
	add_edge(n3, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,3*b), tree);
	add_edge(n3,n4,EdgeProp(Bet,4*b),tree);

	add_edge(n4,T1,EdgeProp(Fold,3*b),tree);
	add_edge(n4, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,4*b), tree);

	add_edge(n5, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,0*b), tree);
	add_edge(n5,n6,EdgeProp(Bet,1*b),tree);

	add_edge(n6,T1,EdgeProp(Fold,0*b),tree);
	add_edge(n6, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,1*b), tree);
	add_edge(n6,n7,EdgeProp(Bet,2*b),tree);

	add_edge(n7,T0,EdgeProp(Fold,1*b),tree);
	add_edge(n7, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,2*b), tree);
	add_edge(n7,n8,EdgeProp(Bet,3*b),tree);

	add_edge(n8,T1,EdgeProp(Fold,2*b),tree);
	add_edge(n8, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,3*b), tree);
	add_edge(n8,n9,EdgeProp(Bet,4*b),tree);

	add_edge(n9,T0,EdgeProp(Fold,3*b),tree);
	add_edge(n9, addlimit(gr+1,tree,T0,T1,TSD), EdgeProp(Call,4*b), tree);

	return n0;
}


//tests to make sure edge ordering is preserved
void testtree()
{
	const int N = 7; //oughta do it
	BettingTree t;
	Vertex root = add_vertex(t);
	Vertex n[N];
	for(int i=0; i<N; i++)
	{
		n[i] = add_vertex(t);
		add_edge(root, n[i], EdgeProp(Fold, i), t);
	}
	for(int i=0; i<N; i++)
		for(int j=0; j<N; j++)
			add_edge(n[i], add_vertex(t), EdgeProp(Fold, j), t);

	// now check it

	EIter e1, e1last;
	EIter e2, e2last;
	int i=0;
	for(tie(e1,e1last) = out_edges(root, t); e1!=e1last; e1++, i++)
	{
		if(t[*e1].potcontrib != i)
			REPORT("edge ordering not preserved...");
		int j=0;
		for(tie(e2,e2last) = out_edges(target(*e1, t), t); e2!=e2last; e2++, j++)
			if(t[*e2].potcontrib != j)
				REPORT("edge ordering not preserved...");
	}
}

Vertex createlimittree(BettingTree &tree, const int max_actions, LoggerClass & treelogger)
{
	//erase whatevers there
	tree.clear(); //shouldn't erase properties
	get_property(tree, maxorderindex_tag()).resize(extents[4][multi_array_types::extent_range(2,10)]);
	for(int i=0; i<4; i++) for(int j=2; j<10; j++) get_property(tree, maxorderindex_tag())[i][j] = 0;

	const int &SB = get_property(tree, settings_tag()).sblind;
	const int &BB = get_property(tree, settings_tag()).bblind;

	//create terminal nodes
	Vertex T0 = add_vertex(NodeProp(TerminalP0Wins), tree);
	Vertex T1 = add_vertex(NodeProp(TerminalP1Wins), tree);
	Vertex TSD = add_vertex(NodeProp(TerminalShowDown), tree);

	//put in the preflop, recursively adding in the rest
	Vertex n0 = add_vertex(NodeProp(P1Plays), tree);
	Vertex n1 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n2 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n3 = add_vertex(NodeProp(P1Plays), tree);
	Vertex n4 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n5 = add_vertex(NodeProp(P1Plays), tree);
	Vertex n6 = add_vertex(NodeProp(P0Plays), tree);
	Vertex n7 = add_vertex(NodeProp(P1Plays), tree);

	add_edge(n0,T0,EdgeProp(Fold,SB),tree);
	add_edge(n0,n1,EdgeProp(Bet,1*BB),tree);
	add_edge(n0,n2,EdgeProp(Bet,2*BB),tree);

	add_edge(n1, addlimit(1,tree,T0,T1,TSD), EdgeProp(Call,1*BB), tree);
	add_edge(n1,n5,EdgeProp(Bet,2*BB),tree);

	add_edge(n5,T0,EdgeProp(Fold,1*BB),tree);
	add_edge(n5, addlimit(1,tree,T1,T1,TSD), EdgeProp(Call,2*BB),tree);
	add_edge(n5,n6,EdgeProp(Bet,3*BB),tree);

	add_edge(n6,T1,EdgeProp(Fold,2*BB),tree);
	add_edge(n6, addlimit(1,tree,T1,T1,TSD), EdgeProp(Call,3*BB),tree);
	add_edge(n6,n7,EdgeProp(Bet,4*BB),tree);
	
	add_edge(n7,T0,EdgeProp(Fold,3*BB),tree);
	add_edge(n7, addlimit(1,tree,T1,T1,TSD), EdgeProp(Call,4*BB),tree);

	add_edge(n2,T1,EdgeProp(Fold,1*BB),tree);
	add_edge(n2, addlimit(1,tree,T1,T1,TSD), EdgeProp(Call,2*BB),tree);
	add_edge(n2,n3,EdgeProp(Bet,3*BB),tree);

	add_edge(n3,T0,EdgeProp(Fold,2*BB),tree);
	add_edge(n3, addlimit(1,tree,T1,T1,TSD), EdgeProp(Call,3*BB),tree);
	add_edge(n3,n4,EdgeProp(Bet,4*BB),tree);

	add_edge(n4,T1,EdgeProp(Fold,3*BB),tree);
	add_edge(n4, addlimit(1,tree,T1,T1,TSD), EdgeProp(Call,4*BB),tree);

	if(num_vertices(tree) != 6378 + 3)
		REPORT("Tree is not verified. Repeat. Tree is not good.");

	//prune it down to size according to stacksize
	prunelimit(0, 0, BB, tree, n0, T0, T1, TSD, max_actions);

	//at most (4 + 4 + 8 + 8) small bets = 24 bblinds can be spent in a game
	treelogger( "Tree has " + tostr( num_vertices(tree)-3 ) + " nodes, and 3 terminal nodes also. "
			+ "Lost " + tostr(  6378+3-num_vertices(tree) ) + " nodes compared to a full tree." );

	if(get_property(tree, settings_tag()).stacksize >= 24 * get_property(tree, settings_tag()).bblind && num_vertices(tree) != 6378 + 3)
		REPORT("Tree is not good. NO GOOD! DO NOT USE!!");

	//return the root
	return n0;
}

void branchnolimit(int gr, int pot, int potcontrib, int bettomeet, bool cancheck, bool amsblind, const Vertex & thisnode, BettingTree &tree, const Vertex &T0, const Vertex &T1, const Vertex &TSD)
{
	const int &SB = get_property(tree, settings_tag()).sblind;
	const int &BB = get_property(tree, settings_tag()).bblind;
	const int &SS = get_property(tree, settings_tag()).stacksize;

	if( SS <= SB )
		REPORT( "No game possible with SS <= SB" );
	if( pot < 0 || potcontrib < 0 || bettomeet < 0 || pot + bettomeet > SS )
		REPORT( "invalid numbers passed to branchnolimit" );
	if( bettomeet < potcontrib )
		REPORT( "Can't bet less than contributed already" );
	if( cancheck && ( bettomeet != 0 || potcontrib != 0 ) )
		REPORT( "Can only check when no bets and no potcontrib" );
	if( tree[ thisnode ].type != P0Plays && tree[ thisnode ].type != P1Plays )
		REPORT( "Invalid node type in branchnolimit" );

	const NodeType otherpl = ( tree[ thisnode ].type == P0Plays ? P1Plays : P0Plays );

	if( bettomeet > potcontrib )
	{
		// add Fold edge
		add_edge( thisnode, ( otherpl == P0Plays ? T0 : T1 ), EdgeProp( Fold, potcontrib ), tree );
	}

	if( cancheck )
	{
		// add Bet edge with 0 bet
		Vertex v = add_vertex( NodeProp( otherpl ), tree );
		add_edge( thisnode, v, EdgeProp( Bet, 0 ), tree );
		branchnolimit( gr, pot, 0, 0, false, false, v, tree, T0, T1, TSD );
	}
	else if( amsblind )
	{
		// add Bet edge with bettomeet potcontrib
		Vertex v = add_vertex( NodeProp( otherpl ), tree );
		add_edge( thisnode, v, EdgeProp( Bet, bettomeet ), tree );
		branchnolimit( gr, pot, bettomeet, bettomeet, false, false, v, tree, T0, T1, TSD );
	}
	else if( pot + bettomeet == SS )
	{
		// add CallAllin edge
		add_edge( thisnode, TSD, EdgeProp( CallAllin, bettomeet ), tree );
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
			branchnolimit( gr + 1, pot + bettomeet, 0, 0, true, false, v, tree, T0, T1, TSD );
		}
	}


	// For Reference:
	//   void branchnolimit(int gr, int pot, int potcontrib, int bettomeet, bool cancheck, bool amsblind, 
	//		Vertex & thisnode, BettingTree &tree, const Vertex &T0, const Vertex &T1, const Vertex &TSD)


	// if more betting is possible, add bets and a BetAllin node

	if( pot + bettomeet < SS ) 
	{
		const int betincrement = mymax( BB, bettomeet - potcontrib );
		for( int betamount = bettomeet + betincrement; pot + betamount < SS; betamount += betincrement )
		{
			// add Betting node
			Vertex v = add_vertex( NodeProp( otherpl ), tree );
			add_edge( thisnode, v, EdgeProp( Bet, betamount ), tree );
			branchnolimit( gr, pot, bettomeet, betamount, false, false, v, tree, T0, T1, TSD );
		}

		// add BetAllin node
		Vertex v = add_vertex( NodeProp( otherpl ), tree );
		add_edge( thisnode, v, EdgeProp( BetAllin, SS - pot ), tree );
		branchnolimit( gr, pot, bettomeet, SS - pot, false, false, v, tree, T0, T1, TSD );
	}
}

void verifynolimit( int gr, int pot, int potcontrib, const Vertex & node, BettingTree &tree, const Vertex &T0, const Vertex &T1, const Vertex &TSD, const int maxacts )
{
	const int &SB = get_property(tree, settings_tag()).sblind;
	//const int &BB = get_property(tree, settings_tag()).bblind;
	const int &SS = get_property(tree, settings_tag()).stacksize;

	if( SS <= SB )
		REPORT( "No game possible with SS <= SB" );
	if( pot + potcontrib > SS )
		REPORT( "too big pot + potcontrib" );
	if( pot < 0 || potcontrib < 0 )
		REPORT( "invalid numbers passed to branchnolimit" );
	if( tree[ node ].type != P0Plays && tree[ node ].type != P1Plays )
		REPORT( "Invalid node type in branchnolimit" );

	//okay, now iterate through them like a normal person and get on goin'!

	if(maxacts > MAX_ACTIONS)
		REPORT("You are using more actions than the rest of the code!");
	if(out_degree(node, tree) < 2 || (int)out_degree(node, tree) > maxacts)
		REPORT("you have a "+tostring(out_degree(node, tree))+" membered node while max_actions is "+tostring(maxacts)+"!");
	tree[node].actioni = get_property(tree, maxorderindex_tag())[gr][out_degree(node, tree)]++;

	EIter e, elast;
	for(tie(e,elast) = out_edges(node, tree); e!=elast; e++)
	{
		switch(tree[*e].type)
		{
			case Bet:
				if( pot + tree[ *e ].potcontrib >= SS )
					REPORT( "Bet doesn't smell like bet" );
				verifynolimit( gr, pot, tree[*e].potcontrib, target(*e, tree), tree, T0, T1, TSD, maxacts);
				break;
			case BetAllin:
				if( pot + tree[ *e ].potcontrib != SS )
					REPORT( "all-in is not really all-in" );
				verifynolimit( gr, pot, tree[*e].potcontrib, target(*e, tree), tree, T0, T1, TSD, maxacts);
				break;
			case Call:
				if(gr < 3)
				{
					verifynolimit( gr+1, pot+tree[*e].potcontrib, 0, target(*e, tree), tree, T0, T1, TSD, maxacts);
					break;
				}
			default: 

				//this is river Call's, CallAllin's and Fold's

				switch( tree[ target( *e, tree ) ].type )
				{
					case TerminalP0Wins: 
					case TerminalP1Wins:
					case TerminalShowDown:
						break;
					default:
						REPORT( "call, callallin, or fold that doesn't point to terminal node" );
				}
				break; //they all lead to terminal nodes.
		}
	}
}

Vertex createnolimittree(BettingTree &tree, const int max_actions, LoggerClass & treelogger)
{
	//erase whatevers there
	tree.clear(); //shouldn't erase properties
	get_property(tree, maxorderindex_tag()).resize(extents[4][multi_array_types::extent_range(2,10)]);
	for(int i=0; i<4; i++) for(int j=2; j<10; j++) get_property(tree, maxorderindex_tag())[i][j] = 0;

	const int &SB = get_property(tree, settings_tag()).sblind;
	const int &BB = get_property(tree, settings_tag()).bblind;
	const int &SS = get_property(tree, settings_tag()).stacksize;

	//create terminal nodes
	Vertex T0 = add_vertex(NodeProp(TerminalP0Wins), tree);
	Vertex T1 = add_vertex(NodeProp(TerminalP1Wins), tree);
	Vertex TSD = add_vertex(NodeProp(TerminalShowDown), tree);

	//put in the preflop, recursively adding in the rest
	Vertex root = add_vertex(NodeProp(P1Plays), tree);

	branchnolimit( PREFLOP, 0, SB, mymin( BB, SS ), false, true, root, tree, T0, T1, TSD );

	verifynolimit( PREFLOP, 0, SB, root, tree, T0, T1, TSD, max_actions ); //makes sure no node has too many actions

	treelogger( "No-Limit tree has " + tostr( num_vertices(tree)-3 ) + " nodes, and 3 terminal nodes also. " );

	//return the root
	return root;
}

Vertex createtree(BettingTree &tree, const int max_actions, LoggerClass & treelogger)
{
	testtree();

	if( get_property(tree, settings_tag()).limit )
		return createlimittree( tree, max_actions, treelogger );
	else
		return createnolimittree( tree, max_actions, treelogger );
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

