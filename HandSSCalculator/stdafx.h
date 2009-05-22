// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#include <stdio.h>
#include <tchar.h>

#include <iostream> //required for cout
#include <fstream> //required for files
#include <algorithm> //required for sort
#include <sstream> //to convert numbers to strings
#include <iomanip> //to pad things

//PokerEval
#include "poker_defs.h"
#include "inlines/eval.h"

#define REPORT(chars) do{ \
	std::cout << chars << std::endl << std::flush; \
	_asm {int 3}; \
	exit(1); \
}while(0)

#define PRINTMASK(c) GenericDeck_maskString(&StdDeck, &c)
