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

#include "poker_defs.h" // PokerEval
#include "inlines/eval.h"

#include "../report.h" //my error reporting function
#include "../HandIndexingV1/getindex2N.h" //contains my fancy getindex2N function


// TODO: reference additional headers your program requires here
