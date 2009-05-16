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

// Needed for file mapping.
#include <Windows.h>

// PokerEval
#include "poker_defs.h"
#include "inlines/eval.h"

// So all files get nice random numbers
#include "../mymersenne.h"

// custom global macros
#define REPORT(chars) { \
    MessageBox(NULL, TEXT(chars), TEXT("failure"), MB_OK);\
	_asm {int 3}; \
	exit(1); \
}

#define PRINTMASK(c) GenericDeck_maskString(&StdDeck, &c)

#define BENCH(c,n) do{ clock_t __clocker = clock()-c; \
	std::cout << n << " iterations completed in " \
		<< float(__clocker)/CLOCKS_PER_SEC << " seconds. (" \
		<< (float) n * CLOCKS_PER_SEC / (float) __clocker << " iterations per second)" << endl; \
	c = clock(); \
}while(0);