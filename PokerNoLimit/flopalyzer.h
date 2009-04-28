
#define COMPILE_FLOPALYZER_TESTCODE

#define FLOP_MAX 6
#define TURN_MAX 17
#define RIVER_MAX 37

int analyzeflop(const CardMask flop);
int analyzeturn(const CardMask flop, const CardMask turn);
int analyzeriver(const CardMask flop, const CardMask turn, const CardMask river);

#ifdef COMPILE_FLOPALYZER_TESTCODE
void testflopalyzer();
#endif