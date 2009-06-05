// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <iostream> //cout
#include <fstream> //needed for saving the strategy file.
#include <string> //needed for some of the rephands shenanigans
#include <sstream> //for formatting strings more easily
#include <iomanip> //needed for formating the way floats are printed in cout's and the like


// PokerEval
#include <poker_defs.h>
#include <inlines/eval.h>

// So all files get nice random numbers
#include "../mymersenne.h"

// custom global macros
#include "../portability.h"
#define REPORT(chars) do{ \
	std::cout << chars << std::endl; \
	ASMBRK; \
	exit(1); \
}while(0)

#define PRINTMASK(c) GenericDeck_maskString(&StdDeck, &c)
