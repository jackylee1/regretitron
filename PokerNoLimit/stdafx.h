// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

//http://www.eventhelix.com/realtimemantra/HeaderFileIncludePatterns.htm

#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#include <iostream>
#include <stdio.h>
#include <tchar.h>
#include <time.h> //needed for time() needed for srand.

// Needed for file mapping.
#include <Windows.h>

// my REPORT function, and other thingsn
#include "../report.h"

// my all-important constants!
#include "constants.h"

// PokerEval
#include "poker_defs.h"
#include "inlines/eval.h"

// TODO: reference additional headers your program requires here
