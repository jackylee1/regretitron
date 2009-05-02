#ifndef _flopalyzer_h_
#define _flopalyzer_h_

#define COMPILE_FLOPALYZER_TESTCODE 0

const int FLOP_MAX=6;
const int TURN_MAX=17;
const int RIVER_MAX=37;

int analyzeflop(const CardMask flop);
int analyzeturn(const CardMask flop, const CardMask turn);
int analyzeriver(const CardMask flop, const CardMask turn, const CardMask river);

#if COMPILE_FLOPALYZER_TESTCODE
void testflopalyzer();
#endif

#endif