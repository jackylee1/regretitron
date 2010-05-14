#include "../HandIndexingV1/getindex2N.h"
#include "../HandIndexingV1/constants.h"
#include "../PokerLibrary/binstorage.h"
#include "../MersenneTwister.h"
#include "../utility.h"
#include <poker_defs.h>
#include <inlines/eval.h>
#include <fstream>
#include <string> //c++
#include <string.h> //C
#include <iomanip>
#include <cmath>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/multi_array.hpp>
//#include <boost/graph/adj_list_serialize.hpp>
//#include <stdio.h> //atol
//#include <stdlib.h> //atol
//#include <string.h> //strcmp
using namespace std;
using namespace boost;


/*************************
  this code is designed for up to 4 game rounds and up to 9 actions per node.
  this code is designed to use "history bins" -- perfect recall.
                                                 **********************************/



//global data members and/or settings

typedef multi_array<int64, 2> OrderType;
OrderType maxorderindex(extents[4][8]);
bool havesetmaxorderindex = false;
//int max_bin[4];
const bool print = false;

//definitions for the Graph datatype 
//
//   Nodes hold their type only,
//   edges hold the regret and strategy values. This is unused for edges from Bucket nodes.
//   edges also hold a "cost", rather than terminal nodes holding a "utility". <--- This, and
//   the fact that I have only one actual tree in memory, are the only changes from how
//   the CPRG did it. I am trying to match their philosophy as much as possible to avoid errors 
//   here, at the expense of performance (on all fronts).
//

struct EdgeProp
{
	EdgeProp(double c) : regret(0), strat(0), cost(c) {}
	double regret;
	double strat;
	double cost;
};

enum NodeType {BucketR0, BucketR1, BucketR2, BucketR3, P0Plays, P1Plays, TerminalP0Wins, TerminalP1Wins, TerminalShowDown, ERROR};

typedef adjacency_list<vecS, vecS, directedS, NodeType, EdgeProp> Graph;
typedef graph_traits<Graph>::vertex_descriptor Vertex;
typedef graph_traits<Graph>::edge_descriptor Edge;
typedef graph_traits<Graph>::out_edge_iterator EIter;
typedef graph_traits<Graph>::degree_size_type Size;


//
//  functions to create a bitchin limit tree.
//

inline Vertex addpush(NodeType type, Graph &tree, vector<Vertex> & leaflist)
{
	Vertex b = add_vertex(type, tree);
	leaflist.push_back(b);
	return b;
}

void attachPreflopBettingTree(const Vertex &top, const Vertex &T0, const Vertex &T1, Graph &tree, vector<Vertex> &ll)
{
	Vertex n0 = add_vertex(P1Plays,tree);
	Vertex n1 = add_vertex(P0Plays,tree);
	Vertex n2 = add_vertex(P0Plays,tree);
	Vertex n3 = add_vertex(P1Plays,tree);
	Vertex n4 = add_vertex(P0Plays,tree);
	Vertex n5 = add_vertex(P1Plays,tree);
	Vertex n6 = add_vertex(P0Plays,tree);
	Vertex n7 = add_vertex(P1Plays,tree);

	add_edge(top,n0,EdgeProp(0.5),tree);

	add_edge(n0,T0,EdgeProp(0),tree);
	add_edge(n0,n1,EdgeProp(0.5),tree);
	add_edge(n0,n2,EdgeProp(0.5),tree);

	add_edge(n1, addpush(BucketR1,tree,ll), EdgeProp(0),tree);
	add_edge(n1,n5,EdgeProp(1),tree);

	add_edge(n5,T0,EdgeProp(0),tree);
	add_edge(n5, addpush(BucketR1,tree,ll), EdgeProp(1),tree);
	add_edge(n5,n6,EdgeProp(1),tree);

	add_edge(n6,T1,EdgeProp(0),tree);
	add_edge(n6, addpush(BucketR1,tree,ll), EdgeProp(1),tree);
	add_edge(n6,n7,EdgeProp(1),tree);
	
	add_edge(n7,T0,EdgeProp(0),tree);
	add_edge(n7, addpush(BucketR1,tree,ll), EdgeProp(1),tree);

	add_edge(n2,T1,EdgeProp(0),tree);
	add_edge(n2, addpush(BucketR1,tree,ll), EdgeProp(1),tree);
	add_edge(n2,n3,EdgeProp(1),tree);

	add_edge(n3,T0,EdgeProp(0),tree);
	add_edge(n3, addpush(BucketR1,tree,ll), EdgeProp(1),tree);
	add_edge(n3,n4,EdgeProp(1),tree);

	add_edge(n4,T1,EdgeProp(0),tree);
	add_edge(n4, addpush(BucketR1,tree,ll), EdgeProp(1),tree);
}

void attachBettingTree(const Vertex &top, NodeType ntype, const Vertex &T0, const Vertex &T1, Graph &tree, vector<Vertex> &ll, const Vertex &TSD)
{
	Vertex n0 = add_vertex(P0Plays,tree);
	Vertex n1 = add_vertex(P1Plays,tree);
	Vertex n2 = add_vertex(P0Plays,tree);
	Vertex n3 = add_vertex(P1Plays,tree);
	Vertex n4 = add_vertex(P0Plays,tree);
	Vertex n5 = add_vertex(P1Plays,tree);
	Vertex n6 = add_vertex(P0Plays,tree);
	Vertex n7 = add_vertex(P1Plays,tree);
	Vertex n8 = add_vertex(P0Plays,tree);
	Vertex n9 = add_vertex(P1Plays,tree);

	const double bet = (ntype == BucketR2 ? 1.0 : 2.0); //if we are terminating these in turn-buckets, then we're at the flop, use small bet
	const bool showdown = (ntype == TerminalShowDown);

	add_edge(top,n0,EdgeProp(0),tree);

	add_edge(n0,n5,EdgeProp(0),tree);
	add_edge(n0,n1,EdgeProp(0),tree);

	add_edge(n1,T0,EdgeProp(0),tree);
	add_edge(n1, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(bet), tree);
	add_edge(n1,n2,EdgeProp(bet),tree);

	add_edge(n2,T1,EdgeProp(0),tree);
	add_edge(n2, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(bet), tree);
	add_edge(n2,n3,EdgeProp(bet),tree);

	add_edge(n3,T0,EdgeProp(0),tree);
	add_edge(n3, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(bet), tree);
	add_edge(n3,n4,EdgeProp(bet),tree);

	add_edge(n4,T1,EdgeProp(0),tree);
	add_edge(n4, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(bet), tree);

	add_edge(n5, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(0), tree);
	add_edge(n5,n6,EdgeProp(0),tree);

	add_edge(n6,T1,EdgeProp(0),tree);
	add_edge(n6, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(bet), tree);
	add_edge(n6,n7,EdgeProp(bet),tree);

	add_edge(n7,T0,EdgeProp(0),tree);
	add_edge(n7, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(bet), tree);
	add_edge(n7,n8,EdgeProp(bet),tree);

	add_edge(n8,T1,EdgeProp(0),tree);
	add_edge(n8, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(bet), tree);
	add_edge(n8,n9,EdgeProp(bet),tree);

	add_edge(n9,T0,EdgeProp(0),tree);
	add_edge(n9, (showdown?TSD:addpush(ntype,tree,ll)), EdgeProp(bet), tree);
}

Vertex createLimitTree(Graph &tree, const vector<int> &max_bin)
{
	tree.clear();

	Vertex root = add_vertex(BucketR0, tree);
	Vertex T0 = add_vertex(TerminalP0Wins,tree);
	Vertex T1 = add_vertex(TerminalP1Wins,tree);
	Vertex TSD = add_vertex(TerminalShowDown,tree);

	vector<Vertex> leaflistA; //these will be lists of Bucket nodes, 
	vector<Vertex> leaflistB; // that need yet more trees attached.

	leaflistA.clear();
	for(int b=0; b<max_bin[0]; b++)  //for each bin, attach a tree
		attachPreflopBettingTree(root, T0,T1, tree, leaflistA); //uses BucketR1

	leaflistB.clear();
	for(int b=0; b<max_bin[1]; b++) for(size_t i=0; i<leaflistA.size(); i++)
		attachBettingTree(leaflistA[i], BucketR2, T0,T1, tree, leaflistB, TSD/*notused*/);

	leaflistA.clear();
	for(int b=0; b<max_bin[2]; b++) for(size_t i=0; i<leaflistB.size(); i++)
		attachBettingTree(leaflistB[i], BucketR3, T0,T1, tree, leaflistA, TSD/*notused*/);

	leaflistB.clear();
	for(int b=0; b<max_bin[3]; b++) for(size_t i=0; i<leaflistA.size(); i++)
		attachBettingTree(leaflistA[i], TerminalShowDown, T0,T1, tree, leaflistB/*notused*/, TSD);

	//a little error checking

	if(num_vertices(tree) != (Size)4+max_bin[0]*(15+7*max_bin[1]*(19+9*max_bin[2]*(19+90*max_bin[3]))) ||
		  num_edges(tree) != (Size)max_bin[0]*(22+189*max_bin[1]*(1+9*max_bin[2]*(1+9*max_bin[3]))))
		REPORT("Tree is not verified. Repeat. Tree is not good.");

	//return the root of a finished Limit tree.

	return root;
}


//
//  functions to create a little friendly Kuhn tree.
//

Vertex makeKuhnBettingTree(const Vertex &T0, const Vertex &T1, const Vertex &TSD, Graph &tree)
{

	//create vertices for each player/opponent node

	Vertex n0 = add_vertex(P0Plays,tree);
	Vertex n1 = add_vertex(P1Plays,tree);
	Vertex n2 = add_vertex(P1Plays,tree);
	Vertex n4 = add_vertex(P0Plays,tree);

	//connect them all together, including the given three permanent terminal nodes

	add_edge(n0,n1,EdgeProp(0),tree);
	add_edge(n0,n2,EdgeProp(0),tree);
	add_edge(n1,TSD,EdgeProp(0),tree);
	add_edge(n1,n4,EdgeProp(0),tree);
	add_edge(n2,T0,EdgeProp(0),tree);
	add_edge(n2,TSD,EdgeProp(1),tree);
	add_edge(n4,T1,EdgeProp(0),tree);
	add_edge(n4,TSD,EdgeProp(1),tree);

	//return the root of the betting portion of a Kuhn tree

	return n0;
}

Vertex createKuhnTree(Graph &tree)
{

	//clear the tree, create the root bucket node and the permanent terminal nodes

	tree.clear();

	Vertex root = add_vertex(BucketR0, tree);
	Vertex T0 = add_vertex(TerminalP0Wins,tree);
	Vertex T1 = add_vertex(TerminalP1Wins,tree);
	Vertex TSD = add_vertex(TerminalShowDown,tree);

	//take on the betting portions, one per bucket

	add_edge(root, makeKuhnBettingTree(T0,T1,TSD,tree), EdgeProp(1), tree);
	add_edge(root, makeKuhnBettingTree(T0,T1,TSD,tree), EdgeProp(1), tree);
	add_edge(root, makeKuhnBettingTree(T0,T1,TSD,tree), EdgeProp(1), tree);

	//return the root of a finished Kuhn tree.

	return root;
}


//
//  structure to represent the joint bucket sequence
//

class BucketSequence
{
	public:
		BucketSequence() { } //initializes MTRand randomly using time
		BucketSequence(int seed) : myrand(seed) { }
		virtual void set(int player, int b0, int b1, int b2, int b3) = 0;
		virtual void dealrandomly() = 0;
		virtual int access(int player, int gameround) const = 0;
		virtual int index(int player, int gameround) const = 0; // determines order of data in binary strat file
		virtual bool p0wins() const = 0;
		virtual bool p1wins() const = 0;
		virtual bool istie() const = 0;

	protected:
		int randInt(int n) { return myrand.randInt(n); }

	private:
		MTRand myrand;
};

class KuhnBuckets : public BucketSequence
{
	public:
		KuhnBuckets() : c(2,-1) { }
		KuhnBuckets(int seed) : BucketSequence(seed), c(2,-1) { }
		void set(int player, int b0, int b1, int b2, int b3) { c[player] = b0; }
		void dealrandomly() { c[0] = randInt(2); c[1] = (c[0]+1+randInt(1))%3; }
		int access(int player, int gameround) const { if(gameround != 0) REPORT("failed access of kuhn buckets"); return c[player]; }
		int index(int player, int gameround) const { if(gameround != 0) REPORT("failed access of kuhn buckets"); return c[player]; }
		bool p0wins() const { return c[0] > c[1]; }
		bool p1wins() const { return c[1] > c[0]; }
		bool istie() const { if(c[0] == c[1]) REPORT("Kuhn cards don't equal."); return false; }

	private:
		vector<int> c;
};

class LimitBuckets : public BucketSequence
{
	public:
		LimitBuckets(const vector<int> &my_max_bin) : c(extents[2][4]), max_bin(my_max_bin),
			b0("bins/preflop"+tostring(max_bin[0]),-1,max_bin[0],INDEX2_MAX,true),
			b1("bins/flop"+tostring(max_bin[0])+"-"+tostring(max_bin[1]),-1,max_bin[1],INDEX23_MAX,true),
			b2("bins/turn"+tostring(max_bin[0])+"-"+tostring(max_bin[1])+"-"+tostring(max_bin[2]),-1,max_bin[2],INDEX231_MAX,true),
			b3("bins/river"+tostring(max_bin[0])+"-"+tostring(max_bin[1])+"-"+tostring(max_bin[2])+"-"+tostring(max_bin[3]),-1,max_bin[3],INDEX2311_MAX,true) { }

		LimitBuckets(const vector<int> &my_max_bin, int seed) : BucketSequence(seed), c(extents[2][4]), max_bin(my_max_bin),
			b0("bins/preflop"+tostring(max_bin[0]),-1,max_bin[0],INDEX2_MAX,true),
			b1("bins/flop"+tostring(max_bin[0])+"-"+tostring(max_bin[1]),-1,max_bin[1],INDEX23_MAX,true),
			b2("bins/turn"+tostring(max_bin[0])+"-"+tostring(max_bin[1])+"-"+tostring(max_bin[2]),-1,max_bin[2],INDEX231_MAX,true),
			b3("bins/river"+tostring(max_bin[0])+"-"+tostring(max_bin[1])+"-"+tostring(max_bin[2])+"-"+tostring(max_bin[3]),-1,max_bin[3],INDEX2311_MAX,true) { }

		void set(int player, int b0, int b1, int b2, int b3) { c[player][0]=b0; c[player][1]=b1; c[player][2]=b2; c[player][3]=b3; winner=ERROR; }
		void dealrandomly();
		int access(int player, int gameround) const { return c[player][gameround]; }

		int index(int player, int gameround) const 
		{
			switch(gameround)
			{
				// determines order of data in binary strat file
				case 0: return c[player][0];
				case 1: return c[player][0]*max_bin[1] + c[player][1];
				case 2: return c[player][0]*max_bin[1]*max_bin[2] + c[player][1]*max_bin[2] + c[player][2];
				case 3: return c[player][0]*max_bin[1]*max_bin[2]*max_bin[3] + c[player][1]*max_bin[2]*max_bin[3] + c[player][2]*max_bin[3] + c[player][3];
			}
			REPORT("failure to yeild."); exit(-1);
		}
		bool p0wins() const { if(winner==ERROR) REPORT("not in that state"); return winner == P0WINS; }
		bool p1wins() const { if(winner==ERROR) REPORT("not in that state"); return winner == P1WINS; }
		bool istie() const { if(winner==ERROR) REPORT("not in that state"); return winner == ISTIE; }
		
	private:
		multi_array<int, 2> c;
		const vector<int> max_bin;
		const PackedBinFile b0, b1, b2, b3;
		enum {P0WINS, P1WINS, ISTIE, ERROR} winner;
};

void LimitBuckets::dealrandomly()
{
	CardMask m,h,f,t,r;
	m.cards_n = h.cards_n = f.cards_n = t.cards_n = r.cards_n = 0;
	vector<int> random_vector(52);
	for(int i=0; i<52; i++) random_vector[i] = i;
	for(int i=0; i<9; i++) swap(random_vector[i], random_vector[i+randInt(51-i)]);
	CardMask_SET(m,random_vector[0]);
	CardMask_SET(m,random_vector[1]);
	CardMask_SET(h,random_vector[2]);
	CardMask_SET(h,random_vector[3]);
	CardMask_SET(f,random_vector[4]);
	CardMask_SET(f,random_vector[5]);
	CardMask_SET(f,random_vector[6]);
	CardMask_SET(t,random_vector[7]);
	CardMask_SET(r,random_vector[8]);

	c[0][0] = b0.retrieve(getindex2(m));
	c[1][0] = b0.retrieve(getindex2(h));
	c[0][1] = b1.retrieve(getindex23(m,f));
	c[1][1] = b1.retrieve(getindex23(h,f));
	c[0][2] = b2.retrieve(getindex231(m,f,t));
	c[1][2] = b2.retrieve(getindex231(h,f,t));
	c[0][3] = b3.retrieve(getindex2311(m,f,t,r));
	c[1][3] = b3.retrieve(getindex2311(h,f,t,r));

	HandVal mv, hv;
	CardMask_OR(m,m,f);
	CardMask_OR(m,m,t);
	CardMask_OR(m,m,r);
	CardMask_OR(h,h,f);
	CardMask_OR(h,h,t);
	CardMask_OR(h,h,r);
	mv = StdDeck_StdRules_EVAL_N(m,7);
	hv = StdDeck_StdRules_EVAL_N(h,7);

	if(mv > hv) winner = P0WINS;
	else if(mv < hv) winner = P1WINS;
	else winner = ISTIE;
}



//
//  Main Recursive Algorithm
//

double WalkTrees(Vertex player0, Vertex player1, const BucketSequence &jointbucketsequence, double prob0, double prob1, double runningcost, Graph &tree)
{
	if(tree[player0] != tree[player1])
		REPORT("trees got mismatched somewhere....");

	switch(tree[player0])
	{
		case P0Plays:
		{

			//get the number of edges, and get access to the edges

			const Size n = out_degree(player0, tree);
			pair<EIter,EIter> edges = out_edges(player0, tree);
			pair<EIter,EIter> edgesother = out_edges(player1, tree);

			//add up all the regret

			if(print) cout << "found p0 node with regrets ";
			double totalregret = 0;
			for(EIter e=edges.first; e!=edges.second; e++)
			{
				if(print) cout << setw(10) << tree[*e].regret;
				totalregret += (tree[*e].regret > 0 ? tree[*e].regret : 0);
			}
			if(print) cout << endl;

			//set the current strategy from the regret

			vector<double> strategy(n, 1.0/n);
			if(totalregret > 0)
			{
				Size j=0;
				for(EIter e=edges.first; e!=edges.second; e++)
					strategy[j++] = (tree[*e].regret > 0 ? tree[*e].regret/totalregret : 0);
			}

			//set the utility from the children and get average utility

			vector<double> utility(n);
			double totalutility = 0;
			{
				EIter e=edges.first, eo=edgesother.first;
				for(Size j=0; j<n; j++)
				{
					utility[j] = (prob1==0 && strategy[j]==0 ? 0 : //speed hack
						WalkTrees(target(*e, tree), target(*eo, tree), jointbucketsequence, strategy[j] * prob0, prob1, runningcost + tree[*e].cost, tree));
					e++; eo++;
					totalutility += utility[j] * strategy[j];
				}
			}

			//set the regret and the average strategy, and return total utility

			{
				EIter e=edges.first;
				for(Size j=0; j<n; j++)
				{
					tree[*e].strat += prob0 * strategy[j];
					tree[*e++].regret += prob1 * (utility[j] - totalutility);
				}
			}

			return totalutility;

		}
		case P1Plays:
		{

			//get the number of edges, and get access to the edges

			const graph_traits<Graph>::degree_size_type n = out_degree(player1, tree);
			pair<EIter, EIter> edges = out_edges(player1, tree);
			pair<EIter, EIter> edgesother = out_edges(player0, tree);

			//add up all the regret

			if(print) cout << "found p1 node with regrets ";
			double totalregret = 0;
			for(EIter e=edges.first; e!=edges.second; e++)
			{
				if(print) cout << setw(10) << tree[*e].regret;
				totalregret += (tree[*e].regret > 0 ? tree[*e].regret : 0);
			}
			if(print) cout << endl;

			//set the current strategy from the regret

			vector<double> strategy(n, 1.0/n);
			if(totalregret > 0)
			{
				Size j=0;
				for(EIter e=edges.first; e!=edges.second; e++)
					strategy[j++] = (tree[*e].regret > 0 ? tree[*e].regret/totalregret : 0);
			}

			//set the utility from the children and get average utility

			vector<double> utility(n);
			double totalutility = 0;
			{
				EIter e=edges.first, eo=edgesother.first;
				for(Size j=0; j<n; j++)
				{
					utility[j] = (prob0 == 0 && strategy[j] == 0 ? 0 : //speed hack
							-WalkTrees(target(*eo, tree), target(*e, tree), jointbucketsequence, prob0, strategy[j] * prob1, runningcost + tree[*e].cost, tree));
					e++; eo++;
					totalutility += utility[j] * strategy[j];
				}
			}

			//set the regret, and return total utility

			{
				EIter e=edges.first;
				for(Size j=0; j<n; j++)
				{
					tree[*e].strat += prob1 * strategy[j];
					tree[*e++].regret += prob0 * (utility[j] - totalutility);
				}
			}

			return -totalutility;
		}
		case BucketR0:
		case BucketR1:
		case BucketR2:
		case BucketR3:
		{
			int r = tree[player0] - BucketR0;
			Edge e0 = *(out_edges(player0,tree).first + jointbucketsequence.access(0,r));
			Edge e1 = *(out_edges(player1,tree).first + jointbucketsequence.access(1,r));
			return WalkTrees(target(e0,tree), target(e1,tree), jointbucketsequence, prob0, prob1, runningcost + tree[e0].cost, tree);
		}

		case TerminalP0Wins:
			return runningcost;
		case TerminalP1Wins:
			return -runningcost;
		case TerminalShowDown:
			if(jointbucketsequence.p0wins()) return runningcost;
			else if(jointbucketsequence.istie()) return 0;
			else if(jointbucketsequence.p1wins()) return -runningcost;
		default:
			REPORT("tree fucked up"); exit(-1);
	}

}


//
//  code to read out the tree and save it in my highly proprietary binary format, with
//  the lookup table as the first 256 bytes, and then bytes of data, with checksum
//  for the remaining part, organized by the order of each node that within those nodes
//  that have the same number of available actions
//

//first put the data in one of these before writing to disk
typedef multi_array<vector<unsigned char>, 2> DataVessel;

void treerunner(Vertex v, int gameround, const BucketSequence &buckets, OrderType &orderindex, DataVessel & vessel, const Graph &tree, bool maxrun = false)
{

	EIter efirst, elast;
	tie(efirst, elast) = out_edges(v,tree);
	switch(tree[v])
	{
		case BucketR0:
		case BucketR1:
		case BucketR2:
		case BucketR3:
			treerunner(target(*(efirst+buckets.access(0,tree[v]-BucketR0)),tree), tree[v]-BucketR0, buckets, orderindex, vessel, tree, maxrun);
			return;

		case P0Plays:
		case P1Plays:

			if(!maxrun) 
			{
				int j=0, n = out_degree(v,tree);
				double maxstrategy=0, totalstrategy=0;
				const bool printkuhnstrat = false;
				vector<unsigned char> mychar(n+1,1); mychar[n]=0;

				for(EIter e = efirst; e != elast; e++)
				{
					maxstrategy = max(maxstrategy, tree[*e].strat);
					totalstrategy += tree[*e].strat;
				}

				if(printkuhnstrat) cout << "P" << tree[v]-P0Plays << " Card" << buckets.access(0,0) << ": ";

				if(maxstrategy == 0)
					mychar[n]=n;
				else for(EIter e = efirst; e != elast; e++)
				{
					if(printkuhnstrat) cout << setw(12) << setprecision(6) << tree[*e].strat;
					mychar[n] += mychar[j++] = (unsigned char)(255.0 * tree[*e].strat / maxstrategy);
				}
				if(printkuhnstrat) cout << "   (" << setprecision(3) << 100.0*tree[*(elast-1)].strat / totalstrategy << "%)" << endl;
				
				copy(mychar.begin(), mychar.end(), vessel[gameround][n-2].begin() 
						+ (n+1) * (buckets.index(0,gameround)*maxorderindex[gameround][n-2] + orderindex[gameround][n-2])); //only place "BucketSequence.index" is used
			}

			orderindex[gameround][out_degree(v,tree)-2]++;
			for(EIter e = efirst; e != elast; e++)
				treerunner(target(*e,tree), gameround, buckets, orderindex, vessel, tree, maxrun);
			return;

		case TerminalP0Wins:
		case TerminalP1Wins:
		case TerminalShowDown:
			return;
		default:
			REPORT("fucked up yo");
			exit(-1);
	}
}

int64 save(const string &filename, const Vertex & root, const Graph &tree, const vector<int> &max_bin)
{

	DataVessel unwrittendata(extents[4][8]);
	OrderType orderindex(extents[4][8]);
	LimitBuckets buckets(max_bin);

	if(!havesetmaxorderindex)
	{
		havesetmaxorderindex = true;
		buckets.set(0,0,0,0,0);
		treerunner(root, 0, buckets, maxorderindex, unwrittendata, tree, true/*maxrun*/);
		//for(int gr=0; gr<4; gr++) for(int numi=0; numi<8; numi++)
			//cout << maxorderindex[gr][numi] << endl;
	}

	for(int numi=0; numi<8; numi++)
	{
		unwrittendata[0][numi].resize((numi+3)*max_bin[0]*maxorderindex[0][numi]);
		unwrittendata[1][numi].resize((numi+3)*max_bin[0]*max_bin[1]*maxorderindex[1][numi]);
		unwrittendata[2][numi].resize((numi+3)*max_bin[0]*max_bin[1]*max_bin[2]*maxorderindex[2][numi]);
		unwrittendata[3][numi].resize((numi+3)*max_bin[0]*max_bin[1]*max_bin[2]*max_bin[3]*maxorderindex[3][numi]);
	}

	for(int r0b=0; r0b<max_bin[0]; r0b++) for(int r1b=0; r1b<max_bin[1]; r1b++) for(int r2b=0; r2b<max_bin[2]; r2b++) for(int r3b=0; r3b<max_bin[3]; r3b++)
	{
		buckets.set(0,r0b,r1b,r2b,r3b);
		for(int gr=0; gr<4; gr++) for(int numi=0; numi<8; numi++)
			orderindex[gr][numi] = 0;
		treerunner(root, 0, buckets, orderindex, unwrittendata, tree);
	}

	ofstream f((filename + ".strat").c_str(), ofstream::binary);
	int64 tableoffset=0;
	int64 dataoffset=4*8*8;
	for(int gr=0; gr<4; gr++) for(int numi=7; numi>=0; numi--)
	{
		// seek to beginning and write the offset
		f.seekp(8*tableoffset++);
		f.write((char*)&dataoffset, 8);
		if(unwrittendata[gr][numi].size() > 0)
		{
			f.seekp(dataoffset);
			f.write((char*)&unwrittendata[gr][numi][0], unwrittendata[gr][numi].size());
			dataoffset = f.tellp();
		}
	}
	f.close();

	return dataoffset;
}

//
// code to read a strategy file into a tree and then PLAY IT!!
//

//run through the tree setting the strategy from the given vector
void loadintotree(const Vertex &node, int dataround, const vector<int> &buckets, int numactions, Graph &tree, vector<unsigned char>::iterator &data, int currentround=-1)
{
	EIter efirst, elast;
	tie(efirst, elast) = out_edges(node,tree);
	switch(tree[node])
	{
		case BucketR0:
		case BucketR1:
		case BucketR2:
		case BucketR3:
			if(tree[node]-BucketR0 <= dataround)
				loadintotree(target(*(efirst+buckets[tree[node]-BucketR0]),tree), dataround, buckets, numactions, tree, data, tree[node]-BucketR0);
			return;

		case P0Plays:
		case P1Plays:
			if(currentround == dataround && out_degree(node, tree)==(Size)numactions)
			{
				vector<int> probs(numactions);
				int total = 0;
				for(int i=0; i<numactions; i++)
					total += probs[i] = *data++;
				if((unsigned char)total != *data++) REPORT("strat xsum err");
				double running = 0;
				int i=0;
				for(EIter e = efirst; e != elast; e++)
					tree[*e].strat = running += (double)probs[i++]/total; //strat is the CDF

				if(running != tree[*(elast-1)].strat || running < 0.999999 || running > 1.000001)
					REPORT("broken numbers: "+tostring(running));
				else
					tree[*(elast-1)].strat = 1;
			}

			for(EIter e = efirst; e != elast; e++)
				loadintotree(target(*e,tree), dataround, buckets, numactions, tree, data, currentround);
			return;

		case TerminalP0Wins:
		case TerminalP1Wins:
		case TerminalShowDown:
			return;
		default:
			REPORT("fucked up yo");
			exit(-1);
	}
}

void readstrategy(const string &filename, const Vertex & root, Graph &tree, int nbins)
{
	ifstream f(filename.c_str(), ifstream::binary);
	DataVessel data(extents[4][DataVessel::extent_range(2,10)]);
	vector<int64> dataoffset(4*8+1);
	f.read((char*)&dataoffset[0],4*8*8);
	f.seekg(0,ios::end);
	dataoffset[4*8] = f.tellg();
	f.seekg(4*8*8,ios::beg);

	for(int r=0; r<4; r++) for(int n=9; n>=2; n--) //order of file
	{
		int i=8*r+(9-n);
		if(f.tellg() != dataoffset[i]) REPORT("strat bad");
		const int64 size = dataoffset[i+1]-dataoffset[i];
		if(size > 0)
		{
			f.seekg(dataoffset[i]);
			data[r][n].resize(size);
			f.read((char*)&data[r][n][0], size);
		}
	}
	f.close();

	for(int n=2; n<=9; n++)
	{
		vector<int> buckets(4,0);
		vector<unsigned char>::iterator iter0 = data[0][n].begin();
		vector<unsigned char>::iterator iter1 = data[1][n].begin();
		vector<unsigned char>::iterator iter2 = data[2][n].begin();
		vector<unsigned char>::iterator iter3 = data[3][n].begin();
		for(buckets[0]=0; buckets[0]<nbins; buckets[0]++) 
		{
			loadintotree(root, 0, buckets, n, tree, iter0);
			for(buckets[1]=0; buckets[1]<nbins; buckets[1]++) 
			{
				loadintotree(root, 1, buckets, n, tree, iter1);
				for(buckets[2]=0; buckets[2]<nbins; buckets[2]++) 
				{
					loadintotree(root, 2, buckets, n, tree, iter2);
					for(buckets[3]=0; buckets[3]<nbins; buckets[3]++) 
						loadintotree(root, 3, buckets, n, tree, iter3);
				}
			}
		}
		if(iter0 != data[0][n].end() || iter1 != data[1][n].end() || iter2 != data[2][n].end() || iter3 != data[3][n].end()) 
			REPORT("did not use all data! "+tostring(n));
	}
}

//walk the trees recursively, randomly determining actions, returning the result
pair<double,NodeType> playgame(const Vertex &player0, const Vertex &player1, double runningcost, const vector<int> &buckets0, const vector<int> &buckets1, const Graph &tree0, const Graph &tree1, MTRand & random)
{
	EIter efirst0, elast0, efirst1, elast1;
	tie(efirst0, elast0) = out_edges(player0,tree0);
	tie(efirst1, elast1) = out_edges(player1,tree1);
	if(tree0[player0] != tree1[player1]) REPORT("mismatched trees yo");
	switch(tree0[player0])
	{
		case BucketR0:
		case BucketR1:
		case BucketR2:
		case BucketR3:
		{
			int r = tree0[player0] - BucketR0;
			if(tree0[*(efirst0+buckets0[r])].cost != tree1[*(efirst1+buckets1[r])].cost) REPORT("broke");
			return playgame( target(*(efirst0+buckets0[r]),tree0), target(*(efirst1+buckets1[r]),tree1), 
					runningcost + tree0[*(efirst0+buckets0[r])].cost, buckets0, buckets1, tree0, tree1, random);
		}

		case P0Plays:
		{
			double answer = random.rand();
			EIter e0 = efirst0, e1 = efirst1;
			while(e0 != elast0 && e1 != elast1)
			{
				if(tree0[*e0].cost != tree1[*e1].cost) REPORT("broke");
				if(tree0[*e0].strat >= answer)
					return playgame(target(*e0,tree0), target(*e1,tree1), runningcost+tree0[*e0].cost, buckets0, buckets1, tree0, tree1, random);
				e0++;
				e1++;
			}
		}
		REPORT("non 1 strat0");

		case P1Plays:
		{
			double answer = random.rand();
			EIter e0 = efirst0, e1 = efirst1;
			while(e0 != elast0 && e1 != elast1)
			{
				if(tree0[*e0].cost != tree1[*e1].cost) REPORT("broke");
				if(tree1[*e1].strat >= answer)
					return playgame(target(*e0,tree0), target(*e1,tree1), runningcost+tree0[*e0].cost, buckets0, buckets1, tree0, tree1, random);
				e0++;
				e1++;
			}
			REPORT("non 1 strat1 "+tostring(answer)+" "+tostring(tree1[*efirst1].strat)+" "+tostring(tree1[*(efirst1+1)].strat));
		}

		case TerminalP0Wins:
		case TerminalP1Wins:
		case TerminalShowDown:
		return pair<double,NodeType> (runningcost,tree0[player0]);

		default:
		REPORT("fucked up yo");
		exit(-1);
	}
}

void playoffs(const string &tom_file, int tom_bins, const string &mary_file, int mary_bins, int64 numgames = 0)
{
	Graph tom_tree, mary_tree;
	Vertex tom_root = createLimitTree(tom_tree, vector<int>(4,tom_bins)), mary_root = createLimitTree(mary_tree, vector<int>(4,mary_bins));
	readstrategy(tom_file, tom_root, tom_tree, tom_bins);
	readstrategy(mary_file, mary_root, mary_tree, mary_bins);

	vector<PackedBinFile *> tom_bfile(4,NULL), mary_bfile(4,NULL);
	tom_bfile[0] = new PackedBinFile("bins/preflop"+tostring(tom_bins),-1,tom_bins,INDEX2_MAX,true);
	tom_bfile[1] = new PackedBinFile("bins/flop"+tostring(tom_bins)+"-"+tostring(tom_bins),-1,tom_bins,INDEX23_MAX,true);
	tom_bfile[2] = new PackedBinFile("bins/turn"+tostring(tom_bins)+"-"+tostring(tom_bins)+"-"+tostring(tom_bins),-1,tom_bins,INDEX231_MAX,true);
	tom_bfile[3] = new PackedBinFile("bins/river"+tostring(tom_bins)+"-"+tostring(tom_bins)+"-"+tostring(tom_bins)+"-"+tostring(tom_bins),-1,tom_bins,INDEX2311_MAX,true);

	if(tom_bins != mary_bins)
	{
		mary_bfile[0] = new PackedBinFile("bins/preflop"+tostring(mary_bins),-1,mary_bins,INDEX2_MAX,true);
		mary_bfile[1] = new PackedBinFile("bins/flop"+tostring(mary_bins)+"-"+tostring(mary_bins),-1,mary_bins,INDEX23_MAX,true);
		mary_bfile[2] = new PackedBinFile("bins/turn"+tostring(mary_bins)+"-"+tostring(mary_bins)+"-"+tostring(mary_bins),-1,mary_bins,INDEX231_MAX,true);
		mary_bfile[3] = new PackedBinFile("bins/river"+tostring(mary_bins)+"-"+tostring(mary_bins)+"-"+tostring(mary_bins)+"-"+tostring(mary_bins),-1,mary_bins,INDEX2311_MAX,true);
	}
	else for(int i=0; i<4; i++)
		mary_bfile[i] = tom_bfile[i];

	bool infiniterepeat = (numgames == 0);
	if(numgames == 0) numgames = 10000;
	vector<int> tom_buckets(4), mary_buckets(4);
	double tom_utilitysum=0;
	int64 iterations=0;
	vector<int> random_vector(52);
	MTRand random;

	do
	{
		for(int x=0; x<numgames; x++)
		{
			iterations++;

			CardMask tom,mary,f,t,r;
			tom.cards_n = mary.cards_n = f.cards_n = t.cards_n = r.cards_n = 0;
			for(int i=0; i<52; i++) random_vector[i] = i;
			for(int i=0; i<9; i++) swap(random_vector[i], random_vector[i+random.randInt(51-i)]);
			CardMask_SET(tom,random_vector[0]);
			CardMask_SET(tom,random_vector[1]);
			CardMask_SET(mary,random_vector[2]);
			CardMask_SET(mary,random_vector[3]);
			CardMask_SET(f,random_vector[4]);
			CardMask_SET(f,random_vector[5]);
			CardMask_SET(f,random_vector[6]);
			CardMask_SET(t,random_vector[7]);
			CardMask_SET(r,random_vector[8]);

			for(int swapper=0; swapper<2; swapper++)
			{
				tom_buckets[0] = tom_bfile[0]->retrieve(getindex2(tom));
				tom_buckets[1] = tom_bfile[1]->retrieve(getindex23(tom,f));
				tom_buckets[2] = tom_bfile[2]->retrieve(getindex231(tom,f,t));
				tom_buckets[3] = tom_bfile[3]->retrieve(getindex2311(tom,f,t,r));
				mary_buckets[0] = mary_bfile[0]->retrieve(getindex2(mary));
				mary_buckets[1] = mary_bfile[1]->retrieve(getindex23(mary,f));
				mary_buckets[2] = mary_bfile[2]->retrieve(getindex231(mary,f,t));
				mary_buckets[3] = mary_bfile[3]->retrieve(getindex2311(mary,f,t,r));

				CardMask tomfull, maryfull;
			   	tomfull.cards_n = tom.cards_n | f.cards_n | t.cards_n | r.cards_n; 
				maryfull.cards_n = mary.cards_n | f.cards_n | t.cards_n | r.cards_n;
				HandVal tom_val = StdDeck_StdRules_EVAL_N(tomfull,7);
				HandVal mary_val = StdDeck_StdRules_EVAL_N(maryfull,7);

				double utility;
				NodeType resulttype;

				if(x%2==0)
				{
					tie(utility, resulttype) = playgame(tom_root, mary_root, 0, tom_buckets, mary_buckets, tom_tree, mary_tree, random);
					if(resulttype == TerminalP0Wins)
						tom_utilitysum += utility;
					else if(resulttype == TerminalP1Wins)
						tom_utilitysum -= utility;
					else if(resulttype == TerminalShowDown)
					{
						if(tom_val > mary_val)
							tom_utilitysum +=utility;
						else if(mary_val > tom_val)
							tom_utilitysum -=utility;
					}
					else REPORT("err");
				}
				else
				{
					tie(utility, resulttype) = playgame(mary_root, tom_root, 0, mary_buckets, tom_buckets, mary_tree, tom_tree, random);
					if(resulttype == TerminalP0Wins)
						tom_utilitysum -= utility;
					else if(resulttype == TerminalP1Wins)
						tom_utilitysum += utility;
					else if(resulttype == TerminalShowDown)
					{
						if(tom_val > mary_val)
							tom_utilitysum +=utility;
						else if(mary_val > tom_val)
							tom_utilitysum -=utility;
					}
					else REPORT("err");
				}

				swap(tom,mary); //swap their cards.
			}
		}

		cout << setw(7) << (double)iterations/1000000 << "M game pairs: "
			 << (tom_utilitysum>0?" LEFT":"RIGHT") << " wins " << setw(8) 
			 << (tom_utilitysum>0?1:-1)*1000*tom_utilitysum/(2*iterations) << " +- " 
			 << setw(5) << setprecision(3) << 2*sqrt((double)5000000/iterations) 
			 << " milli-big-blinds per game." << endl;
		if(infiniterepeat) cout << "\033[1A"; //move cursor up one line
	}
	while(infiniterepeat);
}


int main(int argc, char *argv[])
{
	checkdataformat();

	if(argc==6 && strcmp(argv[1],"playoff")==0)
		playoffs(argv[2], atoi(argv[3]), argv[4], atoi(argv[5]));
	else if(argc==7 && strcmp(argv[1],"playoff")==0)
		playoffs(argv[2], atoi(argv[3]), argv[4], atoi(argv[5]), atol(argv[6]));
	else if (argc==3 && strcmp(argv[1],"solve")==0)
	{
		Graph tree;

		vector<int> max_bin(4, atoi(argv[1]));
		int64 maxiterations = (int64)atoi(argv[2])*1000000;

		Vertex root = createLimitTree(tree,max_bin);
		string filename = "limit-"+tostring(max_bin[0])+"bin";

		LimitBuckets myseq(max_bin, 3/*seed*/);
		char counter = 'A';
		int64 iterations = 0;
		double starttime = getdoubletime();

		for(int x=0; x<5; x++)
		{
			for(int64 i=0; i<maxiterations/5; i++)
			{
				myseq.dealrandomly();
				WalkTrees(root, root, myseq, 1.0, 1.0, 0.0, tree);
				iterations++;
			}

			cout << setprecision(3) << (double)iterations/1000000 << "M iterations done in " << (getdoubletime() - starttime)/3600 << " hours" << endl;

			string fullname = filename + "-" + tostring(counter++);
			int64 savelength = save("strategy/"+fullname,root,tree,max_bin);

			ofstream savelog((fullname+".xml").c_str());

			savelog << "<?xml version=\"1.0\" ?>" << endl;
			savelog << "<strategy version=\"3\">" << endl;
			savelog << "    <solver>" << endl;
			savelog << "        <savefile filesize=\"" << savelength << "\">strategy/" << fullname << ".strat</savefile>" << endl;
			savelog << "        <run iterations=\"" << iterations << "\" totaltime=\"" << (getdoubletime()-starttime)/3600 
				<< " hours\" randseededwith=\"3\" system=\"***Alternate Solver!!!!\" />" << endl;
			savelog << "    </solver>" << endl;
			savelog << "    <cardbins>" << endl;
			savelog << "        <meta usehistory=\"1\" useflopalyzer=\"0\" />" << endl;
			savelog << "        <round0 nbins=\"" << max_bin[0] << "\" filesize=\"-1\">bins/preflop" 
				<< max_bin[0] << "</round0>" << endl;
			savelog << "        <round1 nbins=\"" << max_bin[1] << "\" filesize=\"-1\">bins/flop" 
				<< max_bin[0] << "-" << max_bin[1] << "</round1>" << endl;
			savelog << "        <round2 nbins=\"" << max_bin[2] << "\" filesize=\"-1\">bins/turn" 
				<< max_bin[0] << "-" << max_bin[1] << "-" << max_bin[2] << "</round2>" << endl;
			savelog << "        <round3 nbins=\"" << max_bin[3] << "\" filesize=\"-1\">bins/river" 
				<< max_bin[0] << "-" << max_bin[1] << "-" << max_bin[2] << "-" << max_bin[3] << "</round3>" << endl;
			savelog << "    </cardbins>" << endl;
			savelog << "    <tree>" << endl;
			savelog << "        <meta stacksize=\"200\" pushfold=\"0\" limit=\"1\" />" << endl;
			savelog << "        <blinds sblind=\"1\" bblind=\"2\" />" << endl;
			savelog << "        <bets B1=\"0\" B2=\"0\" B3=\"0\" B4=\"0\" B5=\"0\" B6=\"0\" />" << endl;
			savelog << "        <raise1 R11=\"0\" R12=\"0\" R13=\"0\" R14=\"0\" R15=\"0\" R16=\"0\" />" << endl;
			savelog << "        <raise2 R21=\"0\" R22=\"0\" R23=\"0\" R24=\"0\" R25=\"0\" R26=\"0\" />" << endl;
			savelog << "        <raise3 R31=\"0\" R32=\"0\" R33=\"0\" R34=\"0\" R35=\"0\" R36=\"0\" />" << endl;
			savelog << "        <raise4 R41=\"0\" R42=\"0\" R43=\"0\" R44=\"0\" R45=\"0\" R46=\"0\" />" << endl;
			savelog << "        <raise5 R51=\"0\" R52=\"0\" R53=\"0\" R54=\"0\" R55=\"0\" R56=\"0\" />" << endl;
			savelog << "        <raise6 R61=\"0\" R62=\"0\" R63=\"0\" R64=\"0\" R65=\"0\" R66=\"0\" />" << endl;
			savelog << "    </tree>" << endl;
			savelog << "</strategy>" << endl;

			savelog.close();
		}
	}
	else
	{ 
		cout << "Usage: " << argv[0] << " solve [number of bins] [number of total iterations in millions]" << endl; 
		cout << "              solves that many iterations of limit with histbin n-n-n-n" << endl;
		cout << "       " << argv[0] << " playoff [strat file 1] [number of bins 1] [strat file 2] [number of bins 2]" << endl;
		cout << "              plays games continually outputting the result to the screen." << endl;
		cout << "       " << argv[0] << " playoff [strat file 1] [number of bins 1] [strat file 2] [number of bins 2] [number of games to play]" << endl;
		cout << "              plays that many games and puts out the result and exits." << endl;
		exit(-1); 
	}

	cout << "viva le revulacion" << endl;

#ifdef _WIN32
	PAUSE();
#endif
	return 0;
}

