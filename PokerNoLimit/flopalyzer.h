#ifndef _flopalyzer_h_
#define _flopalyzer_h_

#define COMPILE_FLOPALYZER_TESTCODE 0

int analyzeflop(const CardMask flop);
int analyzeturn(const CardMask flop, const CardMask turn);
int analyzeriver(const CardMask flop, const CardMask turn, const CardMask river);

#if COMPILE_FLOPALYZER_TESTCODE
void testflopalyzer();
#endif

#endif