#if 0
#include "treelimit.h"

// For extern vs static and const,
// see: http://www.gamedev.net/community/forums/topic.asp?topic_id=318620

//THESE ARE THE GLOBAL VARIABLES THAT MAKE UP THE BETTING TREE.
//ONE IS FOR POST FLOP, THE OTHER IS FOR PREFLOP.

const betnode n[10] = //10 is how many betindex values.
{//	{ pl#,Nacts,{children},		{winners},		{utilities}	 }
	{	0,	2,	{1,	 9,  NA},	{NA, NA, NA},	{NA, NA, NA} }, //0
	{	1,	2,	{NA, 8,  NA},	{-1, NA, NA},	{0,  NA, NA} }, //1
	{	0,	3,	{NA, NA, 3 },	{1,  -1, NA},	{2,  4,  NA} }, //2
	{	1,	3,	{NA, NA, 4 },	{0,  -1, NA},	{4,  6,  NA} }, //3
	{	0,	2,	{NA, NA, NA},	{1,  -1, NA},	{6,  8,  NA} }, //4
	{	1,	3,	{NA, NA, 6 },	{0,  -1, NA},	{2,  4,  NA} }, //5
	{	0,	3,	{NA, NA, 7 },	{1,	 -1, NA},	{4,  6,  NA} }, //6
	{	1,	2,	{NA, NA, NA},	{0,  -1, NA},	{6,  8,  NA} }, //7
	{	0,	3,	{NA, NA, 5 },	{1,  -1, NA},	{0,  2,  NA} }, //8
	{	1,	3,  {NA, NA, 2 },	{0,  -1, NA},	{0,  2,  NA} }  //9
};

const betnode pfn[8] = //preflop is different.
{//	{ pl#,Nacts,{children},		{winners},		{utilities}	 }
	{	1,	3,	{NA, 1,  2 },	{0,  NA, NA},	{1,  NA, NA} }, //0
	{	0,	2,	{NA, 5,  NA},	{-1, NA, NA},	{2,  NA, NA} }, //1
	n[2], n[3], n[4], n[5], n[6], n[7] // 2 - 7
};

#endif