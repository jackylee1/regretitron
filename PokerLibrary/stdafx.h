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

// my REPORT function, and other things
#include "../report.h"

// PokerEval
#include "poker_defs.h"
#include "inlines/eval.h"

// TODO: reference additional headers your program requires here
