#ifndef __binstorage_h__
#define __binstorage_h__

#include "../portability.h"

//takes the total number of bins, bin_max, and returns the minimum number
//of bits it takes to represent numbers in the range 0..bin_max-1. 
//we shift out bin_max-1 until no bits are left, counting how many times it takes.
//used by all the below functions.
inline int bitsperbin(int bin_max)
{
	unsigned long bin_max_bits = bin_max-1;
	int nbits=0;
	while(bin_max_bits)
	{
		bin_max_bits>>=1;
		nbits++;
	}
	return nbits;
}

//takes the number of bins and the number of hands to be stored with that bin
//size, returns the size, in bytes, of a file needed to store that many bin 
//numbers. the number of hands is going to be one of the constnats INDEX2N_MAX
//usually. it calculates the file size by calculating how many bin numbers
//store and retrieve will pack into each unsigned long long.
//used by HandBinning to determine the length of arrays to allocate and number of
//bytes to write. used by determinebins.cpp (PokerNoLimit) to check the size of 
//files on disk as consistency check
inline int binfilesize(int bin_max, int n_hands)
{
	if (8!=sizeof(unsigned long long)) REPORT("64 bit failure.");

	//compute the number of bins we can fit in a 64-bit word

	int binsperword = 64/bitsperbin(bin_max);

	//compute the number of bytes that needs.

	if(n_hands%binsperword==0)
		return (n_hands/binsperword) * 8;
	else
		return (n_hands/binsperword + 1) * 8;
}


//takes a pointer to an array of bin data, and returns the ith element,
//as it was packed.
// n is the index into the array. this function is designed to save memory
// by taking into account the max value of each item is bin_max. n ranges
// from 0 to INDEX2N_MAX or so, bin_max is typically 8 or 16 or so.
//used by determinebins.cpp (PokerNoLimit, outer loop) and by PokerPlayer
inline int retrieve(unsigned long long * dataarr, int i, int bin_max)
{	
	if(i<0 || bin_max<=0)
		REPORT("invalid parameters to retrieve()");

	int nbits = bitsperbin(bin_max);

	// (64/nbits) is now the number of packed bin numbers per 64-bit word.

	int binsperword = 64/nbits;
	unsigned long long word = dataarr[i/binsperword];
	word>>=nbits*(i%binsperword);
	word&=~(0xffffffffffffffffULL<<nbits);
	return (int)word;
}

//takes a pointer to the base of an array of bin data, and an offset i. it 
//stores bin in that position, packed as tightly as it can given the max value
//of all the bin values is bin_max-1. companion to the above function.
//used by HandSSCalculator routines only to store data.
inline void store(unsigned long long * dataarr, int i, int bin, int bin_max)
{
	
	if(i<0 || bin<0 || bin>=bin_max || bin_max<=0)
		REPORT("invalid parameters to store()");

	int nbits = bitsperbin(bin_max);

	// position is 0-offset position of the value we're looking for
	// in units of nbits

	int binsperword = 64/nbits;
	int position = i%binsperword;
	
	unsigned long long zeromask = 0xffffffffffffffffULL<<(nbits*(position+1));
	zeromask |= ~(0xffffffffffffffffULL<<(nbits*position));
	dataarr[i/binsperword] &= zeromask; //zeroes out the old value
	
	unsigned long long bin_bits = bin;
	dataarr[i/binsperword] |= bin_bits<<(nbits*position); //or's in the new value
}

#endif
