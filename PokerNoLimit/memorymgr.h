#ifndef _memorymgr_h_
#define _memorymgr_h_

#include "../PokerLibrary/constants.h"

inline float safedivide(float n, float d)
{
	if (d==0 && n!=0) //if we just didn't get there, then the numerator would also be zero
		REPORT("zero denominator!");
	else if(d==0 && n==0) //could have just never gotten there, or could be invalid action
		return 0; //zero probability either way.
	else //d!=0
		return n/d;
}

//algorithm strategy data for 9-membered nodes
//used once per cardsi per gameround-betting-tree-9-membered node.
template <int N> struct treedataN_t
{
#if VECTORIZE
	treedataN_t() : stratn(N-1,0), stratd(N-1,0), regret(N,0) {}
	std::vector<float> stratn;
	std::vector<float> stratd;
	std::vector<float> regret;
#else
	float stratn[N-1];
	float stratd[N-1];
	float regret[N];
#endif
};
template <int N> struct treestratN_t
{
	treestratN_t() : strat(N-1,0) {}
	std::vector<float> strat;
};


//entire algorithm data for a single cardsi for a single gr-betting-tree.
//used once per cardsi per gameround-betting-tree
struct treedata_t
{
	treedataN_t<9> data9[BETI9_MAX];
	treedataN_t<3> data3[BETI3_MAX];
	treedataN_t<2> data2[BETI2_MAX];
};

struct treestrat_t
{
	treestrat_t(const treedata_t &d) 
	{
		for(int b=0; b<BETI9_MAX; b++)
			for(int a=0; a<8; a++)
				strat9[b].strat[a] = safedivide(d.data9[b].stratn[a], d.data9[b].stratd[a]);
		for(int b=0; b<BETI3_MAX; b++)
			for(int a=0; a<2; a++)
				strat3[b].strat[a] = safedivide(d.data3[b].stratn[a], d.data3[b].stratd[a]);
		for(int b=0; b<BETI2_MAX; b++)
			for(int a=0; a<1; a++)
				strat2[b].strat[a] = safedivide(d.data2[b].stratn[a], d.data2[b].stratd[a]);
	}

	treestratN_t<9> strat9[BETI9_MAX];
	treestratN_t<3> strat3[BETI3_MAX];
	treestratN_t<2> strat2[BETI2_MAX];
};

//a single gameround-betting-tree. contains algorithm data for each
//possible cardsi and a single gameround-betting-tree
//used once per gameround-betting-tree.
struct pftree_t
{
	treedata_t treedata[CARDSI_PFLOP_MAX];
};
struct ftree_t
{
	treedata_t treedata[CARDSI_FLOP_MAX];
};
struct ttree_t
{
	treedata_t treedata[CARDSI_TURN_MAX];
};
struct rtree_t
{
	treedata_t treedata[CARDSI_RIVER_MAX];
};


extern pftree_t * pftreebase;
extern ftree_t * ftreebase;
extern ttree_t * ttreebase;
extern rtree_t * rtreebase;
struct twople
{
	twople(int _x, int _y) : x(_x), y(_y) {}
	unsigned char x;
	unsigned char y;
};
struct threeple
{
	threeple(twople a, int _z) : x(a.x), y(a.y), z(_z) {}
	unsigned char x;
	unsigned char y;
	unsigned char z;
};
extern std::vector<unsigned char> pfexitnodes;
extern std::vector<twople> fexitnodes;
extern std::vector<threeple> texitnodes;



#if VECTORIZE
#define GET(exp) exp.begin()
#else
#define GET(exp) exp
#endif
//returns the needed data and beti-offset for a single treedata_t
inline void betindexing(int beti, treedata_t * baseptr,
#if VECTORIZE
			std::vector<float>::iterator &stratn, 
			std::vector<float>::iterator &stratd, 
			std::vector<float>::iterator &regret)
#else
			float * &stratn, float * &stratd, float * &regret)
#endif
{
	if (beti<BETI9_CUTOFF)
	{
		int betioffset = beti;
		stratn = GET(baseptr->data9[betioffset].stratn);
		stratd = GET(baseptr->data9[betioffset].stratd);
		regret = GET(baseptr->data9[betioffset].regret);
	}
	else if (beti<BETI3_CUTOFF)
	{
		int betioffset = beti-BETI9_CUTOFF;
		stratn = GET(baseptr->data3[betioffset].stratn);
		stratd = GET(baseptr->data3[betioffset].stratd);
		regret = GET(baseptr->data3[betioffset].regret);
	}
	else if (beti<BETI2_CUTOFF)
	{
		int betioffset = beti-BETI3_CUTOFF;
		stratn = GET(baseptr->data2[betioffset].stratn);
		stratd = GET(baseptr->data2[betioffset].stratd);
		regret = GET(baseptr->data2[betioffset].regret);
	}
}


void initmem();
void closemem();
void savestratresult(const char * const filename);

#endif