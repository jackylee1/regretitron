#include "analysis.h"
#include "../PokerLibrary/treenolimit.h"
#include "../PokerLibrary/cardmachine.h"
#include "../utility.h"
#include "../PokerLibrary/constants.h"
#include "../PokerPlayerMFC/strategy.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
using namespace std;

//reads the offset table from a strategy file into a vector
vector<vector<int64> > getosets(Strategy &s)
{
	ifstream &f(s.getstratfile());
	if(!f.is_open()) throw string("bad file");
	f.seekg(0); //reset to beginning, where offsets table is
	vector<vector<int64> > offsets(4, vector<int64>(8,0));
	for(int r=0; r<4; r++) for(int n=7; n>=0; n--)
	{
		f.read((char*)&offsets[r][n], 8);
	}
	return offsets;
}

//does a double pass on the header to get the location and size of each strategy region for this file
void handleosets(Strategy &s, void(*func)(int, int, int64, int64, Strategy&))
{
	//should not touch file after this as func will receive same file pointer and move the seekg on me
	ifstream &f(s.getstratfile());
	if(!f.is_open()) throw string("bad file");
	f.seekg(0, ios::end);
	int64 fileend = f.tellg();

	vector<vector<int64> > offsets(getosets(s));
	int lastr = -1, lastn = -1;
	for(int r=0; r<4; r++) for(int n=7; n>=0; n--)
	{
		if(lastr != -1 && offsets[r][n] != offsets[lastr][lastn])
			(*func)(lastr, lastn, offsets[lastr][lastn], offsets[r][n]-offsets[lastr][lastn], s);
		lastr = r;
		lastn = n;
	}
	if(fileend != offsets[lastr][lastn])
		(*func)(lastr, lastn, offsets[lastr][lastn], fileend-offsets[lastr][lastn], s);
}

//prints a vector of strings in columns
void printformat(vector<string> a)
{
	const int w[] = {13, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
	for(unsigned i=0; i< a.size(); i++)
		cout << right << setw(w[i]) << a[i];
	cout << endl;
}

//callback for a strategy file region that just pretty prints the data
void printheaderline(int r, int n, int64 loc, int64 size, Strategy &s)
{
	vector<string> a;
	ostringstream o;
	o << "0x" << hex << setfill('0') << setw(8) << loc << ": ";
	a.push_back(o.str());
	a.push_back(tostring(r));
	a.push_back(tostring(n+2));
	a.push_back(tostring(size));
	if(size%(n+3)!=0) throw string("broken size!");
	a.push_back(tostring(size/(n+3)));
	printformat(a);
}

//pretty prints the offset table of a strategy
void printheader(Strategy &s)
{
	cout << "Header of " << s.getfilename() << ":" << endl << endl;
	vector<string> a(1," ");
	a.push_back("Round");
	a.push_back("N Acts");
	a.push_back("bytes");
	a.push_back("N Nodes");
	printformat(a);
	handleosets(s, printheaderline);
}

//-----------------------------
//code to count smartly follows
//-----------------------------

//returns the number of nodes of a certain type per full limit mini-tree
//specific to deep-stacked limit holdem. in short stacked these numbers are not constant
int nodespertreebyact(int r, int nact)
{
	switch(nact)
	{
		case 2: return r==0 ? 3 : 4;
		case 3: return r==0 ? 5 : 6;
		default: return 0;
	}
}

int playerbyactioni(int r, int nact, int actioni)
{
	if(r==0 && nact == 2) switch(actioni % nodespertreebyact(r, nact))
	{
		case 0: case 2: return 0;
		case 1: return 1;
	}
	else if (r==0 && nact == 3) switch(actioni % nodespertreebyact(r, nact))
	{
		case 2: case 3: return 0;
		case 0: case 1: case 4: return 1;
	}
	else if (r>0 && nact == 2) switch(actioni % nodespertreebyact(r, nact))
	{
		case 0: case 3: return 0;
		case 1: case 2: return 1;
	}
	else if (r>0 && nact == 3) switch(actioni % nodespertreebyact(r, nact))
	{
		case 0: case 4: case 2: return 0;
		case 1: case 3: case 5: return 1;
	}

	throw string("something fishy this way come");
}

void walk(int r, Vertex node, const BettingTree& tree)
{
	switch(tree[node].type)
	{
		case TerminalP0Wins:
		case TerminalP1Wins:
		case TerminalShowDown:
			return;
		default:
			break;
	}

	if(playerbyactioni(r, out_degree(node, tree), tree[node].actioni) != playerindex(tree[node].type))
		throw string("walk found an error!");

	EIter e, elast;
	for(tie(e, elast) = out_edges(node, tree); e!=elast; e++)
		if(tree[*e].type == Call)
			walk(r+1, target(*e, tree), tree);
		else
			walk(r, target(*e, tree), tree);
}



//given parameters, assigns a groupid which is also an index into groupingvector
int groupid(int actioni, int cardsi, int nacts, int r, Strategy &s)
{
	return
		combine(actioni/nodespertreebyact(r,nacts), cardsi, s.getcardmach().getcardsimax(r));
	return
		//s.getcardmach().getcardsimax(r)*(s.getactmax(r,2)/nodespertreebyact(r,2)) * playerbyactioni(r, nacts, actioni) +
		s.getcardmach().getcardsimax(r) * (actioni/nodespertreebyact(r,nacts)) +
		cardsi;
}

//returns the number of groups given the round number
int groupmax(int r, Strategy &s)
{
	if(s.getactmax(r,2) % nodespertreebyact(r,2) != 0) throw string("inalid 2-nodes per tree");
	if(s.getactmax(r,3) % nodespertreebyact(r,3) != 0) throw string("inalid 3-nodes per tree");
	if(s.getactmax(r,2) / nodespertreebyact(r,2) != s.getactmax(r,3) / nodespertreebyact(r,3)) throw string("round has inconsistent number of mini-trees");
	return
		//2 *
		(s.getactmax(r,2)/nodespertreebyact(r,2)) *
		s.getcardmach().getcardsimax(r);
}

//global groupingvector allows counting untouched groups of nodes for potential smart allocation
vector<vector<bool> > groupingvector(4);

//callback for a strategy file region that counts up the info I need
void printcountline(int r, int n, int64 loc, int64 size, Strategy &s)
{
	ifstream &f(s.getstratfile());
	if(!f.is_open()) throw string("bad file");
	if(size%(n+3)!=0) throw string("broken size!");
	f.seekg(loc);
	int64 zeros=0, ones=0;

	for(int i=0; i<size/(n+3); i++)
	{
		bool couldbezerouse=true;
		bool couldbeoneuse=true;
		unsigned char data;
		unsigned char checksum=0;
		for(int j=0; j<n+2; j++)
		{
			f.read((char*)&data,1);
			if(data!=0x01)
				couldbezerouse=false;
			if(data!=0xFF)
				couldbeoneuse=false;
			checksum+=data;
		}
		f.read((char*)&data,1);
		if(s.getcardmach().getcardsimax(r) * s.getactmax(r,n+2) * (n+3) != size)
			throw string("size of this strategy region is not consistent with actionmax and cardsimax");
		if(data!=checksum) 
			throw string("bad checksum during count at "+tostring(f.tellg()));
		if(couldbezerouse && couldbeoneuse)
			throw string("can't be both...");
		if(!couldbezerouse) //groups are assumed untouched (i.e. zero use) until we find a group member that is not
			//int groupid(int actioni, int cardsi, int nacts, int r, Strategy &s)
			groupingvector[r][ groupid(
					i % s.getactmax(r,n+2), //actioni
					i / s.getactmax(r,n+2), //cardsi
					n+2, //nacts
					r, s)] = false; //round & strategy
		if(couldbezerouse)
			zeros++;
		if(couldbeoneuse)
			ones++;
	}

	//print counting stats for this strategy region

	vector<string> a;
	ostringstream o;
	o << "0x" << hex << setfill('0') << setw(8) << loc << ": ";
	a.push_back(o.str());
	a.push_back(tostring(r));
	a.push_back(tostring(n+2));
	a.push_back(tostring(size/(n+3)));
	a.push_back(tostring(zeros));
	a.push_back(tostring(ones));
	a.push_back(tostring(100.0*zeros/(size/(n+3)))+"%");
	a.push_back(tostring(100.0*ones/(size/(n+3)))+"%");
	printformat(a);
}

//pretty prints the offset table of a strategy including all counts
void printcount(Strategy &s)
{
	walk(0, s.gettreeroot(), s.gettree());
	cout << "walk appears to have worked ... " << endl;

	cout << "Count of " << s.getfilename() << ":" << endl << endl;
	vector<string> a(1," ");
	a.push_back("Round");
	a.push_back("N Acts");
	a.push_back("N Nodes");
	a.push_back("UnTouch");
	a.push_back("OneTouch");
	a.push_back("%UnTouch");
	a.push_back("%OneTouch");
	printformat(a);

	//initialize the groupingvector. all groups are assumed untouched until touched
	for(int r=0; r<4; r++)
		groupingvector[r].assign(groupmax(r,s), true);

	//prints general stats about counting, fills in groupingvector
	handleosets(s, printcountline);

	//print stats from groupingvector
	for(int r=0; r<4; r++)
	{
		int count = 0;
		for(int i=0; i<groupmax(r,s); i++)
			if (groupingvector[r][i])
				count++;
		cout << "Round " << r << ": " << 100.0*count/groupmax(r,s) << "% untouched when grouped by groupid" << endl;
	}
	for(int r=0; r<4; r++)
		cout << "Size of group pointers for round " << r << ": " << groupmax(r,s)*8/1024.0/1024.0 << " megabytes." << endl;
	for(int r=0; r<4; r++)
		cout << "Size of node pointers for round " << r << ": " << s.getcardmach().getcardsimax(r)*(s.getactmax(r,2)+s.getactmax(r,3))*8/1024.0/1024.0 << " megabytes." << endl;
}

//print usage info
void printanalysisusage(string me)
{
	cout << "Analysis Usage: " << endl;
    cout << "  print header:      " << me << " header filename" << endl;
    cout << "  print count:       " << me << " count filename" << endl;
}

//our main -- called from actual main if parameters are given to solve (otherwise normal solve is done)
//returns true if we can handle the parameters as given, otherwise returns false
bool doanalysis(int argc, char* argv[])
{
	try
	{
		if(argc == 3 && string(argv[1]) == "header")
		{
			Strategy mystrategy(argv[2], false);
			printheader(mystrategy);
			return true;
		}
		else if(argc == 3 && string(argv[1]) == "count")
		{
			Strategy mystrategy(argv[2], false);
			printcount(mystrategy);
			return true;
		}
		else
			return false;
	}
	catch(string& s)
	{
		cout << s << endl;
		return true;
	}

	return false;
}

