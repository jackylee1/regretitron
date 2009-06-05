//this file contains declarations that are needed for other
//projects to use this library's functionality

#ifndef __get_index2N__
#define __get_index2N__

//Controls whether the testcode is seen by the compiler
#define COMPILE_TESTCODE 0

//getindex2N
// mine are the PokerEval CardMask containing my hole cards. Must be 2 cards.
// board are the PokerEval CardMask containing the board cards. Must contain 3, 4, or 5 cards.
// boardcards is just the total number of cards in the board. Must be 3, 4, or 5.
//This function will return an integer in the range 0..INDEX2N_MAX
//Two hands with the same integer should have the same strength/EV/etc against an opponent.
extern int getindex2N(const CardMask& mine, const CardMask& board, const int boardcards);
extern int getindex2(const CardMask& mine);

#if COMPILE_TESTCODE
extern void testindex2N();
#endif

#endif
