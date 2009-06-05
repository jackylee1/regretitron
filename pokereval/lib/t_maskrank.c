#include "poker_defs.h"
#include "../../portability.h"

/* 
 * Table StdDeck_maskRankTable
 */

/*
StdDeck_masksRanksTable[].  Maps card ranks (2..A) to a cardmask which
has all the bits set except the bits corresponding to the cards whose
have the input rank.  This is a quick way to mask off all the cards of
a specific rank.
 */

StdDeck_CardMask StdDeck_maskRankTable[13] = { 
      { INT64(0xfffefffefffefffe) }  ,
      { INT64(0xfffdfffdfffdfffd) }  ,
      { INT64(0xfffbfffbfffbfffb) }  ,
      { INT64(0xfff7fff7fff7fff7) }  ,
      { INT64(0xffefffefffefffef) }  ,
      { INT64(0xffdfffdfffdfffdf) }  ,
      { INT64(0xffbfffbfffbfffbf) }  ,
      { INT64(0xff7fff7fff7fff7f) }  ,
      { INT64(0xfefffefffefffeff) }  ,
      { INT64(0xfdfffdfffdfffdff) }  ,
      { INT64(0xfbfffbfffbfffbff) }  ,
      { INT64(0xf7fff7fff7fff7ff) }  ,
      { INT64(0xefffefffefffefff) }  
};
