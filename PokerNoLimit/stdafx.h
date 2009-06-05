// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

//http://www.eventhelix.com/realtimemantra/HeaderFileIncludePatterns.htm

#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#include <iostream> //cout
#include <fstream> //needed for saving the strategy file.
#include <stdio.h>
#include <bitset> //needed for bitsets!
#include <sstream> //for formatting strings more easily
#include <string> //needed for some of the rephands shenanigans
#include <iomanip> //needed for formating the way floats are printed in cout's and the like
#include <time.h> //needed benchmark timing
#include <vector> //for recording exit node table needed for player
#include <float.h> //for floating point epsilon
#include <numeric> //for accumulate
#ifndef _WIN32
#include <stdlib.h> //for LINUX exit function exit(int)
#endif

//things specific to Windows and Linux
#include "../portability.h" //needed for ASMBRK below, PAUSE(), etc..

// TinyXML
#define TIXML_USE_TICPP
#include "../TinyXML++/ticpp.h"

// PokerEval
#include <poker_defs.h>
#include <inlines/eval.h>

// custom global macros
#define REPORT(chars) do{ \
	std::cout << chars << std::endl; \
	ASMBRK; \
	exit(1); \
}while(0)

#define PRINTMASK(c) GenericDeck_maskString(&StdDeck, &c)
