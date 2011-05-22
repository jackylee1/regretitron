#ifndef __handindex_constants_h__
#define __handindex_constants_h__

#include "../utility.h" //for int64

//the following are MAX INDEX VALUES
//Ni - describing a bucket of i ranks
const int N1 = ((13)/(1)); //13 choose 1
const int N2 = ((13*12)/(2*1)); //13 choose 2
const int N3 = ((13*12*11)/(3*2*1)); //13 choose 3
const int N4 = ((13*12*11*10)/(4*3*2*1)); //13 choose 4
const int N5 = ((13*12*11*10*9)/(5*4*3*2*1)); //13 choose 5
const int N6 = ((13*12*11*10*9*8)/(6*5*4*3*2*1)); //13 choose 6
const int N7 = ((13*12*11*10*9*8*7)/(7*6*5*4*3*2*1)); //13 choose 7
//these are max mcolexi indexes, used below
//Mij - describing i indices that each describe j cards
const int M21 = (13 + ((13*12)/(2*1))); // = mcolexi(12,12)+1 - combining 2 indices that each describe 1 card
const int M22 = (N2 + (((N2)*(N2-1))/(2*1))); // = mcolexi(N2-1,N2-1)+1 - combining 2 indices that each describe 2 cards
const int M23 = (N3 + (((N3)*(N3-1))/(2*1))); //combining 2 indices that each describe 3 cards
const int M31 = (M21 + ((14*13*12)/(3*2*1))); // = mcolexi(12,12,12)+1 - combining 3 indices that each describe 1 card
const int M32 = (M22 + (((N2+1)*(N2)*(N2-1))/(3*2*1))); // combining 3 indices that each describe 2 cards
const int M41 = (M31 + ((15*14*13*12)/(4*3*2*1))); // = mcolexi(12,12,12,12)+1 - combining 4 indices that each describe 1 card

const int INDEX7_MAX = (N4*M31 + N3*N2*M21 + M32*N1 + N5*M21 + N4*N2*N1 + M23*N1 + N3*M22 + N6*N1 + N5*N2 + N4*N3 + N7);
const int INDEX6_MAX = (N3*M31 + M22*M21 + N4*M21 + N3*N2*N1 + M32 + N5*N1 + N4*N2 + M23 + N6);
const int INDEX5_MAX = (N2*M31 + N3*M21 + M22*N1 + N4*N1 + N3*N2 + N5);
const int INDEX4_MAX = (M41 + N2*M21 + N3*N1 + M22 + N4);



//below this line is what is actually used by other code. the above were just for inputting purposes
//--------------------------------------------------------------------------------------------------

const int INDEX2_MAX = M21 + N2; 
const int INDEX25_MAX = INDEX7_MAX*((7*6)/(2*1)); //7 choose 2 additional combos to specify which are mine
const int INDEX24_MAX = INDEX6_MAX*((6*5)/(2*1)); //6 choose 2 additional combos to specify which are mine
const int INDEX23_MAX = INDEX5_MAX*((5*4)/(2*1)); //5 choose 2 additional combos to specify which are mine

const int INDEX231_MAX = INDEX24_MAX*4; //4 for the 4 locations of the turn card among the 4 board cards
const int64 INDEX2311_MAX = INDEX25_MAX*4LL*5LL;  //5 for the turn card location among the 5 board cards, 
												  //4 for the river card location among the remaining 4
const int INDEX3_MAX = (M31 + N2*N1 + N3);
const int INDEX31_MAX = INDEX4_MAX*4;
const int INDEX311_MAX = INDEX5_MAX*4*5;


#endif
