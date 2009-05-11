#ifndef _flopalyzer_h_
#define _flopalyzer_h_

#define COMPILE_FLOPALYZER_TESTCODE 0

int flopalyzer(const CardMask flop);
int turnalyzer(const CardMask flop, const CardMask turn);
int rivalyzer(const CardMask flop, const CardMask turn, const CardMask river);

#if COMPILE_FLOPALYZER_TESTCODE
void testflopalyzer();
#endif

#endif