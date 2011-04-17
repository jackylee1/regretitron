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
using namespace std;

//reads the offset table from a strategy file into a vector
vector<vector<int64> > getosets(Strategy &s)
{
	InFile &f(s.getstratfile());
	f.Seek(0); //reset to beginning, where offsets table is
	vector<vector<int64> > offsets(4, vector<int64>(8,0));
	for(int r=0; r<4; r++) for(int n=7; n>=0; n--)
	{
		offsets[r][n] = f.Read<int64>();
	}
	return offsets;
}

//does a double pass on the header to get the location and size of each strategy region for this file
void handleosets(Strategy &s, void(*func)(int, int, int64, int64, Strategy&))
{
	//should not touch file after this as func will receive same file pointer and move the Seek on me
	InFile &f(s.getstratfile());
	int64 fileend = f.Size();

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
	o << "0x" << hex << setfill('0') << setw(10) << loc << ": ";
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
/*
		groupingvector[r][ groupid(
					i % s.getactmax(r,n+2), //actioni
					i / s.getactmax(r,n+2), //cardsi
					n+2, //nacts
					r, s)] = false; //round & strategy
					*/
		

//given parameters, assigns a groupid which is also an index into groupingvector
int groupid(int actioni, int cardsi, int nacts, int r, Strategy &s)
{
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
	InFile &f(s.getstratfile());
	if(size%(n+3)!=0) throw string("broken size!");
	f.Seek(loc);
	int64 zeros=0, ones=0;

	for(int i=0; i<size/(n+3); i++)
	{
		bool couldbezerouse=true;
		bool couldbeoneuse=true;
		unsigned char data;
		unsigned char checksum=0;
		for(int j=0; j<n+2; j++)
		{
			data = f.Read<unsigned char>();
			if(data!=0x01)
				couldbezerouse=false;
			if(data!=0xFF)
				couldbeoneuse=false;
			checksum+=data;
		}
		data = f.Read<unsigned char>();
		if(s.getcardmach().getcardsimax(r) * s.getactmax(r,n+2) * (n+3) != size)
			throw string("size of this strategy region is not consistent with actionmax and cardsimax");
		if(data!=checksum) 
			throw string("bad checksum during count at "+tostring(f.Tell()));
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
	o << "0x" << hex << setfill('0') << setw(10) << loc << ": ";
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

// ------------ what at info

int64 offset; //global so callback can use
void searchforoffset(int r, int n, int64 loc, int64 size, Strategy &s)
{
	const int c = 20;
	if(offset >= loc && offset < loc+size)
	{
		int64 index = (offset-loc)/(n+3);
		offset = loc + (n+3) * index;
		cout << s.getfilename() << ": Offset " << hex << offset << " is in round " << r << " and nacts " << n+2 << endl;
		InFile &f(s.getstratfile());
		f.Seek(offset);
		cout << setw(c) << right << "Bytes: ";
		unsigned char bytes[10];
		uint32 sum = 0;
		for(int i=0; i<n+2; i++)
		{
			sum += bytes[i] = f.Read<unsigned char>();
			cout << hex << setw(2) << setfill('0') << (int)bytes[i] << setfill(' ') << dec << ' ';
		}
		cout << '(' << hex << setw(2) << setfill('0') << (int)(sum&0xff) << setfill(' ') << dec << ") ";
		if((sum&0xff) != f.Read<unsigned char>())
			throw string("Checksum in strat file failed.");
		cout << "= ";
		for(int i=0; i<n+2; i++)
			cout << setprecision(4) << 100.0*bytes[i]/sum << "% ";
		cout << endl
			<< setw(c) << right << "Actioni: " << index%s.getactmax(r,n+2) << " (of max " << s.getactmax(r,n+2) << ')' << endl
			<< setw(c) << right << "Cardsi: " << index/s.getactmax(r,n+2) << " = <"
			<< (index/s.getactmax(r,n+2))%s.getcardmach().getparams().bin_max[0] << ' '
			<< (r<1?".":
				tostring((index/s.getactmax(r,n+2))/s.getcardmach().getparams().bin_max[0]%s.getcardmach().getparams().bin_max[1])) << ' '
			<< (r<2?".":
				tostring((index/s.getactmax(r,n+2))/s.getcardmach().getparams().bin_max[0]/s.getcardmach().getparams().bin_max[1]%s.getcardmach().getparams().bin_max[2])) << ' '
			<< (r<3?".":
				tostring((index/s.getactmax(r,n+2))/s.getcardmach().getparams().bin_max[0]/s.getcardmach().getparams().bin_max[1]/s.getcardmach().getparams().bin_max[2]))
			<< "> (of max " << s.getcardmach().getcardsimax(r) << " = <"
			<< s.getcardmach().getparams().bin_max[0] << ' '
			<< s.getcardmach().getparams().bin_max[1] << ' '
			<< s.getcardmach().getparams().bin_max[2] << ' '
			<< s.getcardmach().getparams().bin_max[3]
		  	<< ">)" << endl 
			<< setw(c) << right << "Player(0/1): " << 'P' << playerbyactioni(r,n+2,index%s.getactmax(r,n+2)) << endl << endl;
	}
}

void printwhatat(Strategy & s, const char * arg)
{
	istringstream i(arg);
	i >> hex >> offset;
	if(i.fail()) throw string("Bad offset, see usage.");
	handleosets(s, searchforoffset);
}

// ------------------ diffing

//code to get the error of two vectors of probabilities
// error = percentage of probability space that returns different answers
struct SortType { SortType(int pl, int act, double pos) : player(pl), action(act), position(pos) {}  int player; int action; double position; };
bool sorttype(const SortType & a, const SortType & b) { return a.position < b.position; }
double geterror(const vector<double> &p1, const vector<double> &p2)
{
	if(p1.size() != p2.size()) throw string("two vectors hav edifferentt sizes");
	if(!fpequal( accumulate(p1.begin(), p1.end(), 0.0), 1.0, p1.size()/*ulps*/)) throw string("p1 does not add to one ( = "+tostring(accumulate(p1.begin(), p1.end(), 0.0))+")");
	if(!fpequal( accumulate(p2.begin(), p2.end(), 0.0), 1.0, p2.size()/*ulps*/)) throw string("p2 does not add to one ( = "+tostring(accumulate(p2.begin(), p2.end(), 0.0))+")");

	/*** This algorithm compares two discrete probability distributions and 
	  has the following properties :

	  1) symmetric. Proof:
	   0 = sum(p1) - sum(p2)
		 = sum(p1 - p2)
		 = sum(p1 - p2)_{p1-p2 positive}  +  sum(p1 - p2)_{p1-p2 negative}
	   ===> sum(p1 - p2)_{p1>p2} = sum(p2 - p1)_{p2>p1}
	  2) Bounded above by 1.0. Proof:
	     sum(p1 - p2)_{p1>p2} < sum(p1)_{p1>p2} < sum(p1) = 1.0
	  3) Max value is 1.0. Proof:
	     #2 above and the example p1 = { 1.0, 0.0 }; p2 = { 0.0, 1.0 };
	  4) Bounded below by 0.0. Proof: It is the sum of positive numbers
	  5) Min value is 0.0. Proof: #4 above and the case p1 = p2.

	  So it is a symmetric inner product of two probability distributions 
	  with values between 0 and 1 inclusive, which is 0 for identical distributions
	  and which doesn't suffer from the ordering issues because it is constructed to 
	  compare each probability individually.  
	 **********************************************************************/

	double totalerror = 0;
	for(unsigned m = 0; m < p1.size(); m++)
	{
		if(p1[m] > p2[m]) totalerror += p1[m] - p2[m];
	}

	return totalerror;
}

Strategy * otherstrat = NULL; //global used by diffcallback
const int thresholds[] = { 25, 50, 75, 100 };
const int nthresh = sizeof(thresholds) / sizeof(int);
void diffcallback(int r, int n, int64 loc, int64 size, Strategy &s)
{
	Strategy &s1 = s;
	Strategy &s2 = *otherstrat;
	if(size%(n+1)!=0 || s1.getactmax(r,n+2) * s1.getcardmach().getcardsimax(r) != size / (n+3))
		throw string("bad strategy file...");
	if(size%(n+1)!=0 || s2.getactmax(r,n+2) * s2.getcardmach().getcardsimax(r) != size / (n+3))
		throw string("bad other strategy file...");

	vector<int> tally(nthresh, 0);

	for(int actioni = 0; actioni < s1.getactmax(r,n+2); actioni++)
	{
		for(int cardsi = 0; cardsi < s1.getcardmach().getcardsimax(r); cardsi++)
		{

			vector<double> p1;
			vector<double> p2;
			s1.getprobs(r, actioni, n+2, cardsi, p1);
			s2.getprobs(r, actioni, n+2, cardsi, p2);
			double totalerror = geterror(p1, p2);

			for(int i=0; i<nthresh; i++)
			{
				if(fpgreatereq(thresholds[i], 100.0*totalerror))
				{
					tally[i]++;
					break;
				}
			}
		}
	}

	int64 checktotal = 0;
	for(int i=0; i<nthresh; i++)
		checktotal += tally[i];
	if(checktotal != size/(n+3))
		throw string("failed checktotal");

	vector<string> linefmt(1, "Rnd "+tostring(r)+" numa "+tostring(n+2)+": ");
	for(int i=0; i<nthresh; i++)
		linefmt.push_back(tostring(100.0*tally[i]/checktotal)+"%");
	printformat(linefmt);
}


void dodiffanalysis(Strategy & s1, Strategy & s2)
{
	if(s1.getstratfile().Size() != s2.getstratfile().Size())
		throw string("Two strategies have different sized strategy files.");
	otherstrat = &s2;
	vector<string> header(1,"");
	int lastpct = 0;
	for(int i=0;i<nthresh;i++)
	{
		header.push_back(tostring(lastpct)+"-"+tostring(thresholds[i])+"%");
		lastpct = thresholds[i];
	}
	printformat(header);
	handleosets(s1, diffcallback);
}

struct NodeData
{
	NodeData(int r, int n, int a) : gr(r), numa(n), actioni(a) { }
	int gr;
	int numa;
	int actioni;
};

void historywalk(int r, Vertex node, const BettingTree& tree, vector<NodeData>& results, int gr, int numa, int actioni)
{
	switch(tree[node].type)
	{
		case TerminalP0Wins: case TerminalP1Wins: case TerminalShowDown: return;
		default: break;
	}

	try
	{
		if(r == gr && tree[node].actioni == actioni && out_degree(node, tree) == (unsigned)numa)
			throw 5; //throw value irrelevant


		EIter e, elast;
		for(tie(e, elast) = out_edges(node, tree); e!=elast; e++)
			if(tree[*e].type == Call)
				historywalk(r+1, target(*e, tree), tree, results, gr, numa, actioni);
			else
				historywalk(r, target(*e, tree), tree, results, gr, numa, actioni);
	}
	catch ( int )
	{
		results.push_back(NodeData(r, out_degree(node, tree), tree[node].actioni));
		throw;
	}
}

void dohistoryof(Strategy & s, int gr, int numa, int actioni)
{
	vector<NodeData> results;
	try
	{
		historywalk(0, s.gettreeroot(), s.gettree(), results, gr, numa, actioni);
		throw string("Node not found.");
	}
	catch ( int )
	{
		cout << "Found " << results.size() << " history nodes." << endl;
		vector<string> fmt(1,"");
		fmt.push_back("Round");
		fmt.push_back("Numacts");
		fmt.push_back("Actioni");
		printformat(fmt);
		for(unsigned i=0; i<results.size(); i++)
		{
			fmt.assign(1,"");
			fmt.push_back(tostring(results[i].gr));
			fmt.push_back(tostring(results[i].numa));
			fmt.push_back(tostring(results[i].actioni));
			printformat(fmt);
		}
	}
}


const int col1 = 1, col2 = 3;
int nmax = 0;
int currentn = 0;
double tolerance = 0;
string probstring(const vector<double> &p)
{
	ostringstream o;
	for(vector<double>::const_iterator q = p.begin(); q!=p.end(); q++)
		o << setprecision(3) << *q << "% ";
	return o.str();
}
void diffprintcallback(int r, int n, int64 loc, int64 size, Strategy &s)
{
	Strategy &s1 = s;
	Strategy &s2 = *otherstrat;
	if(size%(n+1)!=0 || s1.getactmax(r,n+2) * s1.getcardmach().getcardsimax(r) != size / (n+3))
		throw string("bad strategy file...");
	if(size%(n+1)!=0 || s2.getactmax(r,n+2) * s2.getcardmach().getcardsimax(r) != size / (n+3))
		throw string("bad other strategy file...");

	vector<int> tally(nthresh, 0);

	for(int actioni = 0; actioni < s1.getactmax(r,n+2); actioni++)
	{
		for(int cardsi = 0; cardsi < s1.getcardmach().getcardsimax(r); cardsi++)
		{

			vector<double> p1;
			vector<double> p2;
			s1.getprobs(r, actioni, n+2, cardsi, p1);
			s2.getprobs(r, actioni, n+2, cardsi, p2);
			if(geterror(p1, p2) > tolerance)
			{
				cout << setw(33) << "Round "+tostring(r)+" nacts "+tostring(n+2)+" acti "+tostring(actioni)+" cardsi "+tostring(cardsi) 
					<< setw(33) << probstring(p1) << setw(33) << probstring(p2) << endl;
				if(++currentn == nmax)
					throw "Stopped at "+tostring(nmax)+" differences";
			}
		}
	}
}

	

void printdiff(Strategy & s1, Strategy & s2, int n, double tol)
{
	if(s1.getstratfile().Size() != s2.getstratfile().Size())
		throw string("Two strategies have different sized strategy files.");
	tolerance = tol;
	otherstrat = &s2;
	cout << setw(33) << ' ' << setw(33) << s1.getfilename() << setw(33) << s2.getfilename() << endl;
	if((nmax = n) <= 0) throw string("bad nmax");
	handleosets(s1, diffprintcallback);
}

bool randomopponent;

void walk(int r, Vertex node, Strategy & s, vector<int>& cardsi, double prob0, double prob1, 
		void (*func)(int, int, int, int, double, double, const vector<double>&),
		void (*leaf)(double, double));
void walkactions(int r, Vertex node, Strategy & s, vector<int>& cardsi, double prob0, double prob1, 
		void (*func)(int, int, int, int, double, double, const vector<double>&),
		void (*leaf)(double, double))
{
	switch(s.gettree()[node].type) 
	{ 
		default: break; 
		case TerminalP0Wins: case TerminalP1Wins: case TerminalShowDown: 
			if(leaf != NULL)
				(*leaf)(prob0, prob1);
			return; 
	}

	const int nacts = out_degree(node, s.gettree());
	vector<double> p;
	s.getprobs(r, s.gettree()[node].actioni, nacts, cardsi[r], p);
	vector<double> opp;
	if(randomopponent)
		opp.assign(nacts, 1.0/nacts);
	else
		opp = p;

	if(func != NULL)
		(*func)(r, nacts, s.gettree()[node].actioni, cardsi[r], prob0, prob1, p);//call the callback

	EIter e, elast;
	int i=0;
	for(tie(e, elast) = out_edges(node, s.gettree()); e!=elast; e++, i++)
		if(s.gettree()[*e].type == Call && r != 3)
			if(playerindex(s.gettree()[node].type) == 0)
				walk(r+1, target(*e, s.gettree()), s, cardsi, prob0*p[i], prob1*opp[i], func, leaf);
			else
				walk(r+1, target(*e, s.gettree()), s, cardsi, prob0*opp[i], prob1*p[i], func, leaf);
		else
			if(playerindex(s.gettree()[node].type) == 0)
				walkactions(r, target(*e, s.gettree()), s, cardsi, prob0*p[i], prob1*opp[i], func, leaf);
			else
				walkactions(r, target(*e, s.gettree()), s, cardsi, prob0*opp[i], prob1*p[i], func, leaf);
}

void walk(int r, Vertex node, Strategy & s, vector<int>& cardsi, double prob0, double prob1, 
		void (*func)(int, int, int, int, double, double, const vector<double>&),
		void (*leaf)(double, double))
{
	const int bmax = s.getcardmach().getparams().bin_max[r];
	if(cardsi[r] == -1) //unset: iterate through all.
	{
		for(int bin = 0; bin < bmax; bin++)
		{
			cardsi[r] = (r==0?0:cardsi[r-1]) + (r==0?1:s.getcardmach().getcardsimax(r-1)) * bin;
			walkactions(r, node, s, cardsi, prob0/bmax, prob1/bmax, func, leaf);
		}
		cardsi[r] = -1;
	}
	else //set: use as-is.
	{
		walkactions(r, node, s, cardsi, prob0, prob1, func, leaf);
	}
}


vector<double> tom0, tom1, mary0, mary1;
void leaftom(double p0, double p1)
{
	tom0.push_back(p0);
	tom1.push_back(p1);
}

void leafmary(double p0, double p1)
{
	mary0.push_back(p0);
	mary1.push_back(p1);
}

void docompare(Strategy & tom, Strategy & mary)
{
	vector<int> cardsi(4,-1);
	randomopponent = false;
	walk(0, tom.gettreeroot(), tom, cardsi, 1.0, 1.0, NULL, leaftom);
	walk(0, mary.gettreeroot(), mary, cardsi, 1.0, 1.0, NULL, leafmary);
	double err = geterror(tom0, mary0);
	//if(!fpequal( err, geterror(tom1, mary1) ))
		//throw string("errors are not equal");
	cout << setprecision(4) << 100.0*(1-err) << " ";

	tom0.clear(); tom1.clear(); mary0.clear(); mary1.clear();

	randomopponent = true;
	walk(0, tom.gettreeroot(), tom, cardsi, 1.0, 1.0, NULL, leaftom);
	walk(0, mary.gettreeroot(), mary, cardsi, 1.0, 1.0, NULL, leafmary);
	double err0 = geterror(tom0, mary0);
	double err1 = geterror(tom1, mary1);
	cout << setprecision(4) << 100.0*(1-err0) << " " << 100.0*(1-err1) << endl;
}

//print usage info
void printanalysisusage(string me)
{
	cout << "Analysis Usage: " << endl;
    cout << "  print header:        " << me << " header filename" << endl;
    cout << "  print count:         " << me << " count filename" << endl;
	cout << "  print indices:       " << me << " whatat [hex offset] filename" << endl;
	cout << "  diff two strategies: " << me << " diff filename filename" << endl;
	cout << "  print n differences: " << me << " diff n tolerance filename filename" << endl;
	cout << "  node history:        " << me << " historyof gr numa actioni filename" << endl;
	cout << "  comp two strategies: " << me << " compare filename filename" << endl;
}

//our main -- called from actual main, we decide here whether we handle it or not
//returns true if we can handle the parameters as given, otherwise returns false
bool doanalysis(int argc, char* argv[])
{
	try
	{
		ConsoleLogger analysislogger;

		if(argc == 1)
			return false;
		else if(argc == 3 && string(argv[1]) == "header")
		{
			Strategy mystrategy(argv[2], false, analysislogger);
			printheader(mystrategy);
			return true;
		}
		else if(argc == 3 && string(argv[1]) == "count")
		{
			Strategy mystrategy(argv[2], false, analysislogger);
			printcount(mystrategy);
			return true;
		}
		else if (argc >= 4 && string(argv[1]) == "whatat")
		{
			for(int i=3; i<argc; i++)
			{
				Strategy mystrategy(argv[i], false, analysislogger);
				printwhatat(mystrategy, argv[2]);
			}
			return true;
		}
		else if (argc == 4 && string(argv[1]) == "diff")
		{
			Strategy strat1(argv[2], false, analysislogger);
			Strategy strat2(argv[3], false, analysislogger);
			dodiffanalysis(strat1, strat2);
			return true;
		}
		else if (argc == 6 && string(argv[1]) == "diff")
		{
			Strategy strat1(argv[4], false, analysislogger);
			Strategy strat2(argv[5], false, analysislogger);
			printdiff(strat1, strat2, atoi(argv[2]), atof(argv[3]));
			return true;
		}
		else if (argc == 6 && string(argv[1]) == "historyof")
		{
			Strategy mystrategy(argv[5], false, analysislogger);
			dohistoryof(mystrategy, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
			return true;
		}
		else if (argc == 4 && string(argv[1]) == "compare")
		{
			Strategy s1(argv[2], true, analysislogger);
			Strategy s2(argv[3], true, analysislogger);
			docompare(s1, s2);
			return true;
		}
		else if (string(argv[1]) == "test")
		{
			vector<double> p1, p2;
			for(int i=2; i<argc; i+=2)
			{
				p1.push_back(atof(argv[i]));
				p2.push_back(atof(argv[i+1]));
			}
			cout << geterror(p1,p2) << endl;
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

