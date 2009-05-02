#include "stdafx.h"
#include "memorymgr.h"

//http://msdn.microsoft.com/en-us/library/bb613473(VS.85).aspx
//4-Gigabyte Tuning (4GT)
//  On 32-bit windows, allows you to allocate 3GB virtual address space to each process
//  instead of the default 2GB. System wide setting set at boot time.

//http://msdn.microsoft.com/en-us/library/wz223b1z.aspx
//IMAGE_FILE_LARGE_ADDRESS_AWARE value of the LOADED_IMAGE structure
//  Set by passing an option to the linker when compiling a program. It allows 
//  the program to access addresses larger than 2GB. Naturally, you must then 
//  be sure not to use that most significant bit in a weird way. I think it is the
//  default when compiling for 64-bit.

//http://msdn.microsoft.com/en-us/library/aa366778(VS.85).aspx
//Memory Limits for Windows Releases
//  Virtual address space (assuming large address aware^^): 
//       3GB for 32-bit windows with 4GTuning turned on
//       8TB for 64-bit windows on x64.

//http://msdn.microsoft.com/en-us/library/aa366796(VS.85).aspx
//Physical Adress Extension (PAE)
//  Allows >4GB physical memory in 32-bit editions of windows.

//http://msdn.microsoft.com/en-us/library/aa366527(VS.85).aspx
//Address Windowning Extensions (AWE)
//  Do not understand how, but it allows a process to have direct control over mapping
//  physical pages. This allows it access to memory > 4GB, and other perks?


/*

11 bits 2's complement = M

5 bits exponent: 0-31, shift to.. -10 - 21 = E

if(E>=0)
float = signextend(M) << (E)
else
float = (float)signextent(M) / 1<<(|E|)

*/














//Current strategy, Average strategy, Accumulated total regret:
//separated based on max actions to save memory, allocated in main.
float * strategy9, * strategy3;
float * strategysumnum9, * strategysumnum3; 
float * strategysumden9, * strategysumden3;
float * regret9, * regret3;


//the arrays of strategy and regret are indexed by:
//gameround and:
//preflop: hand index
//postflop: betting history, pot size, flopalyzer score, and HSS bin.


	//obtain the correct segments of the global data arrays
	if (beti<BETI_9CUTOFF)
	{
		//and these are now pointers to arrays of 8 (for strat) or 9 (for regret) elements
		assert(maxa > 3 && maxa <= 9); //this would mean problem with betting tree
		//Essentially, I am implementing multidimensional arrays here. The first 3 have 8
		//actions in the last dimension, the last has 9.
		strat       = strategy9       + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX9 + beti)*8;
		stratsumnum = strategysumnum9 + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX9 + beti)*8;
		stratsumden = strategysumden9 + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX9 + beti)*8;
		regret      = regret9         + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX9 + beti)*9;
	}
	else
	{
		//and these are now pointers to arrays of 2 (for strat) or 3 (for regret) elements
		assert(maxa <= 3); //this would mean problem with betting tree
		//The first 3 have 2 actions in the last dimension, the last has 3.
		strat       = strategy3       + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX3 + beti)*2;
		stratsumnum = strategysumnum3 + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX3 + beti)*2;
		stratsumden = strategysumden3 + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX3 + beti)*2;
		regret      = regret3         + (((myhandid)*SCENI_MAX + sceni)*BETI_MAX3 + beti)*3;
	}



		//allocate memory!
	strategy9       = new float[169*SCENI_MAX*BETI_MAX9*8];
	strategysumnum9 = new float[169*SCENI_MAX*BETI_MAX9*8];
	strategysumden9 = new float[169*SCENI_MAX*BETI_MAX9*8];
	regret9         = new float[169*SCENI_MAX*BETI_MAX9*9];

	strategy3       = new float[169*SCENI_MAX*BETI_MAX3*2];
	strategysumnum3 = new float[169*SCENI_MAX*BETI_MAX3*2];
	strategysumden3 = new float[169*SCENI_MAX*BETI_MAX3*2];
	regret3         = new float[169*SCENI_MAX*BETI_MAX3*3];

	//and initialize to 0.
	memset(strategy9,       0, 169*SCENI_MAX*BETI_MAX9*8*sizeof(float));
	memset(strategysumnum9, 0, 169*SCENI_MAX*BETI_MAX9*8*sizeof(float));
	memset(strategysumden9, 0, 169*SCENI_MAX*BETI_MAX9*8*sizeof(float));
	memset(regret9,         0, 169*SCENI_MAX*BETI_MAX9*9*sizeof(float));

	memset(strategy3,       0, 169*SCENI_MAX*BETI_MAX3*2*sizeof(float));
	memset(strategysumnum3, 0, 169*SCENI_MAX*BETI_MAX3*2*sizeof(float));
	memset(strategysumden3, 0, 169*SCENI_MAX*BETI_MAX3*2*sizeof(float));
	memset(regret3,         0, 169*SCENI_MAX*BETI_MAX3*3*sizeof(float));
	
