//the following are MAX INDEX VALUES
//Ni - describing a bucket of i ranks
#define N1 ((13)/(1)) //13 choose 1
#define N2 ((13*12)/(2*1)) //13 choose 2
#define N3 ((13*12*11)/(3*2*1)) //13 choose 3
#define N4 ((13*12*11*10)/(4*3*2*1)) //13 choose 4
#define N5 ((13*12*11*10*9)/(5*4*3*2*1)) //13 choose 5
#define N6 ((13*12*11*10*9*8)/(6*5*4*3*2*1)) //13 choose 6
#define N7 ((13*12*11*10*9*8*7)/(7*6*5*4*3*2*1)) //13 choose 7
//these are max mcolexi indexes, used below
//Mij - describing i indices that each describe j cards
#define M21 (13 + ((13*12)/(2*1))) // = mcolexi(12,12)+1 - combining 2 indices that each describe 1 card
#define M22 (N2 + (((N2)*(N2-1))/(2*1))) // = mcolexi(N2-1,N2-1)+1 - combining 2 indices that each describe 2 cards
#define M23 (N3 + (((N3)*(N3-1))/(2*1))) //combining 2 indices that each describe 3 cards
#define M31 (M21 + ((14*13*12)/(3*2*1))) // = mcolexi(12,12,12)+1 - combining 3 indices that each describe 1 card
#define M32 (M22 + (((N2+1)*(N2)*(N2-1))/(3*2*1))) // combining 3 indices that each describe 2 cards

#define INDEX7_MAX (N4*M31 + N3*N2*M21 + M32*N1 + N5*M21 + N4*N2*N1 + M23*N1 + N3*M22 + N6*N1 + N5*N2 + N4*N3 + N7)
#define INDEX6_MAX (N3*M31 + M22*M21 + N4*M21 + N3*N2*N1 + M32 + N5*N1 + N4*N2 + M23 + N6)
#define INDEX5_MAX (N2*M31 + N3*M21 + M22*N1 + N4*N1 + N3*N2 + N5)

const int INDEX2_MAX = M21 + N2; 
const int INDEX25_MAX = INDEX7_MAX*((7*6)/(2*1)); //7 choose 2 additional combos to specify which are mine
const int INDEX24_MAX = INDEX6_MAX*((6*5)/(2*1)); //6 choose 2 additional combos to specify which are mine
const int INDEX23_MAX = INDEX5_MAX*((5*4)/(2*1)); //5 choose 2 additional combos to specify which are mine
