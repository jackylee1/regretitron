#include "stdafx.h"
#include "treenolimit.h"
#include "constants.h"
#include "../utility.h"
#include <sstream>
#include <iomanip>


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

Vertex createtree(BettingTree &tree, const int max_actions)
{
	testtree();

	if(!get_property(tree, settings_tag()).limit)
		REPORT("no limit tree is not implemented. come back later. shouldn't be too hard. all code is in place. "
			   "basically, you just need to copy the huge comment below into boost-graph language. That is all. ");

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
	if(isloggingon())
		getlog() << "Tree has " << num_vertices(tree)-3 << " nodes, and 3 terminal nodes also. "
			<< "Lost " << 6378+3-num_vertices(tree) << " nodes compared to a full tree." << endl;

	if(get_property(tree, settings_tag()).stacksize >= 24 * get_property(tree, settings_tag()).bblind && num_vertices(tree) != 6378 + 3)
		REPORT("Tree is not good. NO GOOD! DO NOT USE!!");

	//return the root
	return n0;
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
		str << "Fold";
		break;
	case Call:
		str << "Call " << multiplier*(tree[edge].potcontrib);
		break;
	case Bet:
		str << "Bet " << multiplier*(tree[edge].potcontrib);
		break;
	case CallAllin:
		str << "Call All-in";
		break;
	case BetAllin:
		str << "Bet All-in";
		break;
	}

	return str.str();
}


	/*
	//used for both trees...
	
	if(myparams.limit && myparams.pushfold)
		REPORT("I can't make a limit pushfold tree...");

	const char P0 = 0;
	const char P1 = 1;

	const unsigned char &NA = BetNode::NA;
	const unsigned char &AI = BetNode::AI;
	const unsigned char &FD = BetNode::FD;
	const unsigned char &GO = BetNode::GO;


	// define no limit tree...

	// the following array (MYTREE) is defined here so that I can use the syntax of array
	// initialization to define a single tree from the parameters I am passed.
	// Once it's initialized, I copy it to a dynamically located heap memory location.

	const unsigned char &B1 = myparams.bets[0];
	const unsigned char &B2 = myparams.bets[1];
	const unsigned char &B3 = myparams.bets[2];
	const unsigned char &B4 = myparams.bets[3];
	const unsigned char &B5 = myparams.bets[4];
	const unsigned char &B6 = myparams.bets[5];

	const unsigned char &R11 = myparams.raises[0][0];
	const unsigned char &R12 = myparams.raises[0][1];
	const unsigned char &R13 = myparams.raises[0][2];
	const unsigned char &R14 = myparams.raises[0][3];
	const unsigned char &R15 = myparams.raises[0][4];
	const unsigned char &R16 = myparams.raises[0][5];

	const unsigned char &R21 = myparams.raises[1][0];
	const unsigned char &R22 = myparams.raises[1][1];
	const unsigned char &R23 = myparams.raises[1][2];
	const unsigned char &R24 = myparams.raises[1][3];
	const unsigned char &R25 = myparams.raises[1][4];
	const unsigned char &R26 = myparams.raises[1][5];

	const unsigned char &R31 = myparams.raises[2][0];
	const unsigned char &R32 = myparams.raises[2][1];
	const unsigned char &R33 = myparams.raises[2][2];
	const unsigned char &R34 = myparams.raises[2][3];
	const unsigned char &R35 = myparams.raises[2][4];
	const unsigned char &R36 = myparams.raises[2][5];

	const unsigned char &R41 = myparams.raises[3][0];
	const unsigned char &R42 = myparams.raises[3][1];
	const unsigned char &R43 = myparams.raises[3][2];
	const unsigned char &R44 = myparams.raises[3][3];
	const unsigned char &R45 = myparams.raises[3][4];
	const unsigned char &R46 = myparams.raises[3][5];

	const unsigned char &R51 = myparams.raises[4][0];
	const unsigned char &R52 = myparams.raises[4][1];
	const unsigned char &R53 = myparams.raises[4][2];
	const unsigned char &R54 = myparams.raises[4][3];
	const unsigned char &R55 = myparams.raises[4][4];
	const unsigned char &R56 = myparams.raises[4][5];

	const unsigned char &R61 = myparams.raises[5][0];
	const unsigned char &R62 = myparams.raises[5][1];
	const unsigned char &R63 = myparams.raises[5][2];
	const unsigned char &R64 = myparams.raises[5][3];
	const unsigned char &R65 = myparams.raises[5][4];
	const unsigned char &R66 = myparams.raises[5][5];

	const int N_NODES_NOLIMIT = 172;

	//Allowed result values: 
	// FD if the player to act folds.
	// GO if the play goes on in the next game round (including showdowns)
	// AI if an all-in bet is called
	// the beti of a child node if the betting continues in the present round
	// or NA if this action is not used.
	//   beti's are indexes into these such arrays
	const BetNode MYTREE[N_NODES_NOLIMIT] =
	{
		// {P#, #A, {result}, {potcontrib}}

		//0-1, 8 available actions, these are intitial node, and response to check.
		{P0,8, { 1, 2, 4, 6, 8, 10, 12, 98, NA}, {0, B1, B2, B3, B4, B5, B6, 0, NA}}, //0
		{P1,8, {GO, 3, 5, 7, 9, 11, 13, 99, NA}, {0, B1, B2, B3, B4, B5, B6, 0, NA}}, //1

		//2-13, 9 available actions, responding to a first bet
		{P1,9, {FD, GO, 14, 15, 16, 17, 18, 19, 86}, {0, B1, R11, R12, R13, R14, R15, R16, 0}}, //2
		{P0,9, {FD, GO, 20, 21, 22, 23, 24, 25, 92}, {0, B1, R11, R12, R13, R14, R15, R16, 0}}, //3
		{P1,9, {FD, GO, 26, 27, 28, 29, 30, 31, 87}, {0, B2, R21, R22, R23, R24, R25, R26, 0}}, //4
		{P0,9, {FD, GO, 32, 33, 34, 35, 36, 37, 93}, {0, B2, R21, R22, R23, R24, R25, R26, 0}}, //5

		{P1,9, {FD, GO, 38, 39, 40, 41, 42, 43, 88}, {0, B3, R31, R32, R33, R34, R35, R36, 0}}, //6
		{P0,9, {FD, GO, 44, 45, 46, 47, 48, 49, 94}, {0, B3, R31, R32, R33, R34, R35, R36, 0}}, //7
		{P1,9, {FD, GO, 50, 51, 52, 53, 54, 55, 89}, {0, B4, R41, R42, R43, R44, R45, R46, 0}}, //8
		{P0,9, {FD, GO, 56, 57, 58, 59, 60, 61, 95}, {0, B4, R41, R42, R43, R44, R45, R46, 0}}, //9

		{P1,9, {FD, GO, 62, 63, 64, 65, 66, 67, 90}, {0, B5, R51, R52, R53, R54, R55, R56, 0}}, //10
		{P0,9, {FD, GO, 68, 69, 70, 71, 72, 73, 96}, {0, B5, R51, R52, R53, R54, R55, R56, 0}}, //11
		{P1,9, {FD, GO, 74, 75, 76, 77, 78, 79, 91}, {0, B6, R61, R62, R63, R64, R65, R66, 0}}, //12
		{P0,9, {FD, GO, 80, 81, 82, 83, 84, 85, 97}, {0, B6, R61, R62, R63, R64, R65, R66, 0}}, //13

		//14-19 & 20-25, 3 available actions, responding to raises to a bet B1
		{P0,3, {FD, GO, 100, NA6}, {B1, R11, 0, NA6}}, //14
		{P0,3, {FD, GO, 101, NA6}, {B1, R12, 0, NA6}}, //15
		{P0,3, {FD, GO, 102, NA6}, {B1, R13, 0, NA6}}, //16
		{P0,3, {FD, GO, 103, NA6}, {B1, R14, 0, NA6}}, //17
		{P0,3, {FD, GO, 104, NA6}, {B1, R15, 0, NA6}}, //18
		{P0,3, {FD, GO, 105, NA6}, {B1, R16, 0, NA6}}, //19

		{P1,3, {FD, GO, 106, NA6}, {B1, R11, 0, NA6}}, //20
		{P1,3, {FD, GO, 107, NA6}, {B1, R12, 0, NA6}}, //21
		{P1,3, {FD, GO, 108, NA6}, {B1, R13, 0, NA6}}, //22
		{P1,3, {FD, GO, 109, NA6}, {B1, R14, 0, NA6}}, //23
		{P1,3, {FD, GO, 110, NA6}, {B1, R15, 0, NA6}}, //24
		{P1,3, {FD, GO, 111, NA6}, {B1, R16, 0, NA6}}, //25

		//26-31 & 32-37, 3 available actions, responding to raises to a bet B2
		{P0,3, {FD, GO, 112, NA6}, {B2, R21, 0, NA6}}, //26
		{P0,3, {FD, GO, 113, NA6}, {B2, R22, 0, NA6}}, //27
		{P0,3, {FD, GO, 114, NA6}, {B2, R23, 0, NA6}}, //28
		{P0,3, {FD, GO, 115, NA6}, {B2, R24, 0, NA6}}, //29
		{P0,3, {FD, GO, 116, NA6}, {B2, R25, 0, NA6}}, //30
		{P0,3, {FD, GO, 117, NA6}, {B2, R26, 0, NA6}}, //31

		{P1,3, {FD, GO, 118, NA6}, {B2, R21, 0, NA6}}, //32
		{P1,3, {FD, GO, 119, NA6}, {B2, R22, 0, NA6}}, //33
		{P1,3, {FD, GO, 120, NA6}, {B2, R23, 0, NA6}}, //34
		{P1,3, {FD, GO, 121, NA6}, {B2, R24, 0, NA6}}, //35
		{P1,3, {FD, GO, 122, NA6}, {B2, R25, 0, NA6}}, //36
		{P1,3, {FD, GO, 123, NA6}, {B2, R26, 0, NA6}}, //37

		//38-43 & 44-49, 3 available actions, responding to raises to a bet B3
		{P0,3, {FD, GO, 124, NA6}, {B3, R31, 0, NA6}}, //38
		{P0,3, {FD, GO, 125, NA6}, {B3, R32, 0, NA6}}, //39
		{P0,3, {FD, GO, 126, NA6}, {B3, R33, 0, NA6}}, //40
		{P0,3, {FD, GO, 127, NA6}, {B3, R34, 0, NA6}}, //41
		{P0,3, {FD, GO, 128, NA6}, {B3, R35, 0, NA6}}, //42
		{P0,3, {FD, GO, 129, NA6}, {B3, R36, 0, NA6}}, //43

		{P1,3, {FD, GO, 130, NA6}, {B3, R31, 0, NA6}}, //44
		{P1,3, {FD, GO, 131, NA6}, {B3, R32, 0, NA6}}, //45
		{P1,3, {FD, GO, 132, NA6}, {B3, R33, 0, NA6}}, //46
		{P1,3, {FD, GO, 133, NA6}, {B3, R34, 0, NA6}}, //47
		{P1,3, {FD, GO, 134, NA6}, {B3, R35, 0, NA6}}, //48
		{P1,3, {FD, GO, 135, NA6}, {B3, R36, 0, NA6}}, //49

		//50-55 & 56-61, 3 available actions, responding to raises to a bet B4
		{P0,3, {FD, GO, 136, NA6}, {B4, R41, 0, NA6}}, //50
		{P0,3, {FD, GO, 137, NA6}, {B4, R42, 0, NA6}}, //51
		{P0,3, {FD, GO, 138, NA6}, {B4, R43, 0, NA6}}, //52
		{P0,3, {FD, GO, 139, NA6}, {B4, R44, 0, NA6}}, //53
		{P0,3, {FD, GO, 140, NA6}, {B4, R45, 0, NA6}}, //54
		{P0,3, {FD, GO, 141, NA6}, {B4, R46, 0, NA6}}, //55

		{P1,3, {FD, GO, 142, NA6}, {B4, R41, 0, NA6}}, //56
		{P1,3, {FD, GO, 143, NA6}, {B4, R42, 0, NA6}}, //57
		{P1,3, {FD, GO, 144, NA6}, {B4, R43, 0, NA6}}, //58
		{P1,3, {FD, GO, 145, NA6}, {B4, R44, 0, NA6}}, //59
		{P1,3, {FD, GO, 146, NA6}, {B4, R45, 0, NA6}}, //60
		{P1,3, {FD, GO, 147, NA6}, {B4, R46, 0, NA6}}, //61

		//62-67 & 68-73, 3 available actions, responding to raises to a bet B5
		{P0,3, {FD, GO, 148, NA6}, {B5, R51, 0, NA6}}, //62
		{P0,3, {FD, GO, 149, NA6}, {B5, R52, 0, NA6}}, //63
		{P0,3, {FD, GO, 150, NA6}, {B5, R53, 0, NA6}}, //64
		{P0,3, {FD, GO, 151, NA6}, {B5, R54, 0, NA6}}, //65
		{P0,3, {FD, GO, 152, NA6}, {B5, R55, 0, NA6}}, //66
		{P0,3, {FD, GO, 153, NA6}, {B5, R56, 0, NA6}}, //67

		{P1,3, {FD, GO, 154, NA6}, {B5, R51, 0, NA6}}, //68
		{P1,3, {FD, GO, 155, NA6}, {B5, R52, 0, NA6}}, //69
		{P1,3, {FD, GO, 156, NA6}, {B5, R53, 0, NA6}}, //70
		{P1,3, {FD, GO, 157, NA6}, {B5, R54, 0, NA6}}, //71
		{P1,3, {FD, GO, 158, NA6}, {B5, R55, 0, NA6}}, //72
		{P1,3, {FD, GO, 159, NA6}, {B5, R56, 0, NA6}}, //73

		//74-79 & 80-85, 3 available actions, responding to raises to a bet B6
		{P0,3, {FD, GO, 160, NA6}, {B6, R61, 0, NA6}}, //74
		{P0,3, {FD, GO, 161, NA6}, {B6, R62, 0, NA6}}, //75
		{P0,3, {FD, GO, 162, NA6}, {B6, R63, 0, NA6}}, //76
		{P0,3, {FD, GO, 163, NA6}, {B6, R64, 0, NA6}}, //77
		{P0,3, {FD, GO, 164, NA6}, {B6, R65, 0, NA6}}, //78
		{P0,3, {FD, GO, 165, NA6}, {B6, R66, 0, NA6}}, //79

		{P1,3, {FD, GO, 166, NA6}, {B6, R61, 0, NA6}}, //80
		{P1,3, {FD, GO, 167, NA6}, {B6, R62, 0, NA6}}, //81
		{P1,3, {FD, GO, 168, NA6}, {B6, R63, 0, NA6}}, //82
		{P1,3, {FD, GO, 169, NA6}, {B6, R64, 0, NA6}}, //83
		{P1,3, {FD, GO, 170, NA6}, {B6, R65, 0, NA6}}, //84
		{P1,3, {FD, GO, 171, NA6}, {B6, R66, 0, NA6}}, //85

		//86-91, 2 available actions, P0 responding to P1's all-in from P0's BN
		{P0,2, {FD, AI, NA7}, {B1, 0, NA7}}, //86
		{P0,2, {FD, AI, NA7}, {B2, 0, NA7}}, //87
		{P0,2, {FD, AI, NA7}, {B3, 0, NA7}}, //88
		{P0,2, {FD, AI, NA7}, {B4, 0, NA7}}, //89
		{P0,2, {FD, AI, NA7}, {B5, 0, NA7}}, //90
		{P0,2, {FD, AI, NA7}, {B6, 0, NA7}}, //91

		//92-97, 2 available actions, P1 responding to P0's all-in from P1's BN
		{P1,2, {FD, AI, NA7}, {B1, 0, NA7}}, //92
		{P1,2, {FD, AI, NA7}, {B2, 0, NA7}}, //93
		{P1,2, {FD, AI, NA7}, {B3, 0, NA7}}, //94
		{P1,2, {FD, AI, NA7}, {B4, 0, NA7}}, //95
		{P1,2, {FD, AI, NA7}, {B5, 0, NA7}}, //96
		{P1,2, {FD, AI, NA7}, {B6, 0, NA7}}, //97

		//98-99, 2 available actions, responding to P0 all-in off the bat, or P0 check - P1 allin
		{P1,2, {FD, AI, NA7}, {0, 0, NA7}}, //98
		{P0,2, {FD, AI, NA7}, {0, 0, NA7}}, //99

		//100-105 & 106-111, 2 available actions, responding to all-in off of R1N
		{P1,2, {FD, AI, NA7}, {R11, 0, NA7}}, //100
		{P1,2, {FD, AI, NA7}, {R12, 0, NA7}}, //101
		{P1,2, {FD, AI, NA7}, {R13, 0, NA7}}, //102
		{P1,2, {FD, AI, NA7}, {R14, 0, NA7}}, //103
		{P1,2, {FD, AI, NA7}, {R15, 0, NA7}}, //104
		{P1,2, {FD, AI, NA7}, {R16, 0, NA7}}, //105

		{P0,2, {FD, AI, NA7}, {R11, 0, NA7}}, //106
		{P0,2, {FD, AI, NA7}, {R12, 0, NA7}}, //107
		{P0,2, {FD, AI, NA7}, {R13, 0, NA7}}, //108
		{P0,2, {FD, AI, NA7}, {R14, 0, NA7}}, //109
		{P0,2, {FD, AI, NA7}, {R15, 0, NA7}}, //110
		{P0,2, {FD, AI, NA7}, {R16, 0, NA7}}, //111

		//112-117 & 118-123, 2 available actions, responding to all-in off of R2N
		{P1,2, {FD, AI, NA7}, {R21, 0, NA7}}, //112
		{P1,2, {FD, AI, NA7}, {R22, 0, NA7}}, //113
		{P1,2, {FD, AI, NA7}, {R23, 0, NA7}}, //114
		{P1,2, {FD, AI, NA7}, {R24, 0, NA7}}, //115
		{P1,2, {FD, AI, NA7}, {R25, 0, NA7}}, //116
		{P1,2, {FD, AI, NA7}, {R26, 0, NA7}}, //117

		{P0,2, {FD, AI, NA7}, {R21, 0, NA7}}, //118
		{P0,2, {FD, AI, NA7}, {R22, 0, NA7}}, //119
		{P0,2, {FD, AI, NA7}, {R23, 0, NA7}}, //120
		{P0,2, {FD, AI, NA7}, {R24, 0, NA7}}, //121
		{P0,2, {FD, AI, NA7}, {R25, 0, NA7}}, //122
		{P0,2, {FD, AI, NA7}, {R26, 0, NA7}}, //123

		//124-129 & 130-135, 2 available actions, responding to all-in off of R3N
		{P1,2, {FD, AI, NA7}, {R31, 0, NA7}}, //124
		{P1,2, {FD, AI, NA7}, {R32, 0, NA7}}, //125
		{P1,2, {FD, AI, NA7}, {R33, 0, NA7}}, //126
		{P1,2, {FD, AI, NA7}, {R34, 0, NA7}}, //127
		{P1,2, {FD, AI, NA7}, {R35, 0, NA7}}, //128
		{P1,2, {FD, AI, NA7}, {R36, 0, NA7}}, //129

		{P0,2, {FD, AI, NA7}, {R31, 0, NA7}}, //130
		{P0,2, {FD, AI, NA7}, {R32, 0, NA7}}, //131
		{P0,2, {FD, AI, NA7}, {R33, 0, NA7}}, //132
		{P0,2, {FD, AI, NA7}, {R34, 0, NA7}}, //133
		{P0,2, {FD, AI, NA7}, {R35, 0, NA7}}, //134
		{P0,2, {FD, AI, NA7}, {R36, 0, NA7}}, //135

		//136-141 & 142-147, 2 available actions, responding to all-in off of R4N
		{P1,2, {FD, AI, NA7}, {R41, 0, NA7}}, //136
		{P1,2, {FD, AI, NA7}, {R42, 0, NA7}}, //137
		{P1,2, {FD, AI, NA7}, {R43, 0, NA7}}, //138
		{P1,2, {FD, AI, NA7}, {R44, 0, NA7}}, //139
		{P1,2, {FD, AI, NA7}, {R45, 0, NA7}}, //140
		{P1,2, {FD, AI, NA7}, {R46, 0, NA7}}, //141

		{P0,2, {FD, AI, NA7}, {R41, 0, NA7}}, //142
		{P0,2, {FD, AI, NA7}, {R42, 0, NA7}}, //143
		{P0,2, {FD, AI, NA7}, {R43, 0, NA7}}, //144
		{P0,2, {FD, AI, NA7}, {R44, 0, NA7}}, //145
		{P0,2, {FD, AI, NA7}, {R45, 0, NA7}}, //146
		{P0,2, {FD, AI, NA7}, {R46, 0, NA7}}, //147

		//148-153 & 154-159, 2 available actions, responding to all-in off of R5N
		{P1,2, {FD, AI, NA7}, {R51, 0, NA7}}, //148
		{P1,2, {FD, AI, NA7}, {R52, 0, NA7}}, //149
		{P1,2, {FD, AI, NA7}, {R53, 0, NA7}}, //150
		{P1,2, {FD, AI, NA7}, {R54, 0, NA7}}, //151
		{P1,2, {FD, AI, NA7}, {R55, 0, NA7}}, //152
		{P1,2, {FD, AI, NA7}, {R56, 0, NA7}}, //153

		{P0,2, {FD, AI, NA7}, {R51, 0, NA7}}, //154
		{P0,2, {FD, AI, NA7}, {R52, 0, NA7}}, //155
		{P0,2, {FD, AI, NA7}, {R53, 0, NA7}}, //156
		{P0,2, {FD, AI, NA7}, {R54, 0, NA7}}, //157
		{P0,2, {FD, AI, NA7}, {R55, 0, NA7}}, //158
		{P0,2, {FD, AI, NA7}, {R56, 0, NA7}}, //159

		//160-165 & 166-171, 2 available actions, responding to all-in off of R6N
		{P1,2, {FD, AI, NA7}, {R61, 0, NA7}}, //160
		{P1,2, {FD, AI, NA7}, {R62, 0, NA7}}, //161
		{P1,2, {FD, AI, NA7}, {R63, 0, NA7}}, //162
		{P1,2, {FD, AI, NA7}, {R64, 0, NA7}}, //163
		{P1,2, {FD, AI, NA7}, {R65, 0, NA7}}, //164
		{P1,2, {FD, AI, NA7}, {R66, 0, NA7}}, //165

		{P0,2, {FD, AI, NA7}, {R61, 0, NA7}}, //166
		{P0,2, {FD, AI, NA7}, {R62, 0, NA7}}, //167
		{P0,2, {FD, AI, NA7}, {R63, 0, NA7}}, //168
		{P0,2, {FD, AI, NA7}, {R64, 0, NA7}}, //169
		{P0,2, {FD, AI, NA7}, {R65, 0, NA7}}, //170
		{P0,2, {FD, AI, NA7}, {R66, 0, NA7}}  //171
	}; 


	if(myparams.limit)
	{

		//preflop - has own tree hard coded above

		tree[PREFLOP] = new BetNode[N_NODES_LIMIT_PFLOP];
		for(int i=0; i<N_NODES_LIMIT_PFLOP; i++)
			tree[PREFLOP][i] = LIMITTREEPFLOP[i];

		//flop - has own tree hard coded above

		tree[FLOP] = new BetNode[N_NODES_LIMIT];
		for(int i=0; i<N_NODES_LIMIT; i++)
			tree[FLOP][i] = LIMITTREE[i];

		//turn and river - has "big bet" must multiply flop tree potcontribs by 2

		tree[TURN] = tree[RIVER] = new BetNode[N_NODES_LIMIT];
		for(int i=0; i<N_NODES_LIMIT; i++)
		{
			tree[TURN][i] = LIMITTREE[i];
			for(int j=0; j<tree[TURN][i].numacts; j++)
				tree[TURN][i].potcontrib[j] *= 2;
		}
	}
	else
	{

		//copy this tree into heap memory for flop, turn, and river (no changes needed)

		tree[FLOP] = tree[TURN] = tree[RIVER] = new BetNode[N_NODES_NOLIMIT];
		for(int i=0; i<N_NODES_NOLIMIT; i++)
			tree[FLOP][i] = MYTREE[i];

		//copy this tree into heap memory for preflop (changes needed)

		tree[PREFLOP] = new BetNode[N_NODES_NOLIMIT];
		for(int i=0; i<N_NODES_NOLIMIT; i++)
		{
			tree[PREFLOP][i] = MYTREE[i];
			//change player to act to reflect reversed order preflop.
			tree[PREFLOP][i].playertoact = 1-tree[FLOP][i].playertoact;
		}

		//more changes to preflop tree

		//Step 1, bump up all non-NA or 0 (All in, or check/fold nothing) valiues to +bblind, 
		//to account for the betting floor
		for(int i=0; i<N_NODES_NOLIMIT; i++)
		{
			for(int j=0; j<9; j++)
			{
				if(tree[PREFLOP][i].potcontrib[j] != NA && tree[PREFLOP][i].potcontrib[j] != 0)
					tree[PREFLOP][i].potcontrib[j] += myparams.bblind;
			}
		}

		//FIX NODE ZERO

		//shift the 8 actions of the first node from 0-7 to 1-8 ...
		for(int i=8; i>0; i--)
		{
			tree[PREFLOP][0].potcontrib[i] = tree[PREFLOP][0].potcontrib[i-1];
			tree[PREFLOP][0].result[i] = tree[PREFLOP][0].result[i-1];
		}

		//... so that we can accomodate the new small blind folding action
		tree[PREFLOP][0].result[0] = FD;
		tree[PREFLOP][0].potcontrib[0] = myparams.sblind;

		//change number of actions to 9.
		tree[PREFLOP][0].numacts = 9;

		if(myparams.pushfold)
			//if pushfold, we need to make "calling" big blind amount unavailable.
			tree[PREFLOP][0].potcontrib[1] = 99;
		else
			//otherwise, change 'checking' value to 'calling' big blind amount
			tree[PREFLOP][0].potcontrib[1] = myparams.bblind;

		//FIX NODE ONE
		//change 'checking' value to 'calling' big blind amount
		tree[PREFLOP][1].potcontrib[0] = myparams.bblind;

		//FIX NODES WITH FOLD ZERO (2-13, 98, 99)

		//Now, must change the fold valuations from 0 to big blind.
		//responses to the six initial bets over bblind, for each player
		for(int i=2; i<=13; i++)
			tree[PREFLOP][i].potcontrib[0] = myparams.bblind;

		//and response to all-in over bblind, for each player.
		tree[PREFLOP][98].potcontrib[0] = myparams.bblind;
		tree[PREFLOP][99].potcontrib[0] = myparams.bblind;
	}
	*/

