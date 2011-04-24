#ifndef _treenolimit_h_
#define _treenolimit_h_

#include "constants.h" //needed for MAX_ACTIONS and gamerounds
#include <string>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/multi_array.hpp>
using namespace std;

//define new names for the data my graph will hold. see http://www.boost.org/doc/libs/1_43_0/libs/graph/doc/using_adjacency_list.html#sec:custom-edge-properties
struct settings_tag { typedef boost::graph_property_tag kind; };
struct maxorderindex_tag { typedef boost::graph_property_tag kind; };

//used to set the betting values, and hence the entire tree shape
struct treesettings_t
{
	int sblind, bblind;
	int bets[6];
	int raises[6][6];
	int stacksize;
	bool pushfold;
	bool limit;
};

//used by the edges of the tree
enum EdgeType { Fold, Call, Bet, BetAllin, CallAllin };

struct EdgeProp
{
	EdgeProp(EdgeType t, int p) : type(t), potcontrib(p) {}
	EdgeType type;
	int potcontrib;
};

//used by the nodes of the tree
enum NodeType {P0Plays = 0, P1Plays = 1, TerminalP0Wins, TerminalP1Wins, TerminalShowDown, NodeError};

struct NodeProp
{
	NodeProp() : type(NodeError), actioni(-999999999) {}
	NodeProp(NodeType t) : type(t), actioni(-999999999) {}
	NodeType type;
	int actioni;
};

//define the tree types

/* ********* Boost Graph container selector issues **************
   Boost Graph allows you to chooese the edge list, and vertex list for your tree. 
   Compiled from Boost Graph documentation:

Out Edge Selector                                                  Invalidates          preserves edge
   Selector      (selects)        Space?          Time?             iterators        ordering (my own tests)
   vecS          vector           least           fastest             YES*                  yes
   listS         list             2nd most        3rd fastest          no                   yes
   slistS        slist            2nd least       2nd fastest          no                   NO!
   setS          set              most            2nd slowest          no                     
   multisetS     multiset                                              no                     
   hash_setS     hash_set                         slowest              no                     
     * invalidates adjacency iterators when an edge is added or removed
     * invalidates edge iterators when an edge is added or removed from a directedS graph

Vertex Selector                                                    Invalidates
   Selector      (selects)        Space?          Time?             iterators
   vecS          vector           good            all good            YES*
   listS         list             +3 ptrs           "                  no
   slistS        slist                              "                  no
   setS          set                                "                  no
   multisetS     multiset                           "                  no
   hash_setS     hash_set                         all good             no
     * invalidates edge and adjacenecy iterators when a vertex is added to a directedS graph
	 * invalidates all descriptors and iterators when a vertex is removed from any type of graph

===> CHOOSE listS for everything until space and time are a PROVEN ISSUE!
   */
typedef boost::adjacency_list<boost::listS, boost::listS, boost::directedS, NodeProp, EdgeProp, 
		boost::property<settings_tag, treesettings_t, 
		boost::property<maxorderindex_tag, boost::multi_array<int,2> > > > BettingTree;
typedef boost::graph_traits<BettingTree>::vertex_descriptor Vertex;
typedef boost::graph_traits<BettingTree>::edge_descriptor Edge;
typedef boost::graph_traits<BettingTree>::out_edge_iterator EIter;
typedef boost::graph_traits<BettingTree>::degree_size_type Size;


//functions provided here to manipulate the trees

//assumes tree object exists already, with settings inside. checks to make sure you don't have more actions than max actions
treesettings_t makelimittreesettings( int sblind, int bblind, int stacksize );
Vertex createtree(BettingTree &tree, const int max_actions, LoggerClass & treelogger); 
string actionstring(const BettingTree &tree, const Edge &edge, double multiplier);
inline int playerindex(NodeType nodetype)
{
	switch(nodetype)
	{
		case P0Plays: return 0;
		case P1Plays: return 1;
		default: REPORT("invalid node in playerindex"); exit(-1);
	}
}



#endif
