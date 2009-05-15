// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <iostream>
#include <fstream>
#include <windows.h>
#include <algorithm> //has min_element
#include <numeric> //has accumulate

// PokerEval
#include "poker_defs.h"
#include "inlines/eval.h"

// my REPORT function, and other thingsn
#include "../report.h"



// TODO: reference additional headers your program requires here
