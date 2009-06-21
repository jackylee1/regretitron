#ifndef __get_index2N__
#define __get_index2N__

#include "../utility.h" //for int64

//Controls whether the testcode is seen by the compiler
#define COMPILE_TESTCODE 1

//getindex2N
// mine are the PokerEval CardMask containing my hole cards. Must be 2 cards.
// board are the PokerEval CardMask containing the board cards. Must contain 3, 4, or 5 cards.
// boardcards is just the total number of cards in the board. Must be 3, 4, or 5.
//This function will return an integer in the range 0..INDEX2N_MAX
//Two hands with the same integer should have the same strength/EV/etc against an opponent.
extern int getindex2N(const CardMask& mine, const CardMask& board, const int boardcards, int * suits = NULL);
extern int getindex2(const CardMask& mine);

inline int getindex23(CardMask pflop, CardMask flop) { return getindex2N(pflop, flop, 3); }
extern int getindex231(CardMask pflop, CardMask flop, CardMask turn);
extern int64 getindex2311(CardMask pflop, CardMask flop, CardMask turn, CardMask river);

#if COMPILE_TESTCODE
extern void testindex2N();
extern void testindex231();
#endif

#endif
