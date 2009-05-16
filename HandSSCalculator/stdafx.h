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
#include <assert.h>
#include <windows.h> //needed for binstruct includes

#include "poker_defs.h" // PokerEval
#include "inlines/eval.h"

#include "../HandIndexingV1/getindex2N.h" //contains my fancy getindex2N function

//needs to be simple cout as I concatenating strings with preprocessor values
//and send them to REPORT. No need for messagebox
#define REPORT(chars) { \
	std::cout << chars << std::endl << std::flush; \
	_asm {int 3}; \
	exit(1); \
}
