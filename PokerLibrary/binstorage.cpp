#include "stdafx.h"
#include "binstorage.h"
#include "../utility.h"
#include <fstream>
using namespace std;

const string PackedBinFile::EXT = ".bins";

PackedBinFile::PackedBinFile(string filename, int64 filesize, int bin_max, int64 index_max, bool preload) :
	_bindata(NULL),
	_haveseen(NULL),
	_filehandle(NULL),
	_index_max(index_max),
	_bin_max(bin_max),
	_preloaded(preload)
{
	filename+=EXT; //tack on extension

	if(filesize == -1)
		filesize = numwordsneeded(bin_max, index_max)*8;
	else if(filesize != numwordsneeded(bin_max, index_max)*8)
		REPORT("wrong file size for "+filename+": you said "+tostring(filesize)
				+", but should be "+tostring(numwordsneeded(bin_max, index_max)*8));

	_filehandle = new InFile(filename, filesize);

	if(preload)
	{
		_bindata = new vector<uint64>();
		_bindata->reserve(filesize/8);
		for(int i=0; i<filesize/8; i++)
			_bindata->push_back(_filehandle->Read<uint64>());
		if((int64)_bindata->size()!=filesize/8)
			REPORT("error reading bin file");
		delete _filehandle;
		_filehandle = NULL;
	}
}


//essentially the only difference is that this is always preloaded, and it starts
//with garbage data. that's it!
PackedBinFile::PackedBinFile(int bin_max, int64 index_max) :
	_bindata(new vector<uint64>(numwordsneeded(bin_max, index_max), -1)),
	_haveseen(new vector<unsigned char>(index_max, 0)), //tracks how many we've stored to each location
	_filehandle(NULL),
	_index_max(index_max),
	_bin_max(bin_max),
	_preloaded(true)
{ }

PackedBinFile::~PackedBinFile()
{
	if((_preloaded && !(_bindata!=NULL && _filehandle==NULL))
			|| (!_preloaded && !(_bindata==NULL && _filehandle!=NULL)))
		REPORT("state of packed bin file is no good in destructor check");

	if(_haveseen!=NULL)
		delete _haveseen;
	if(_bindata!=NULL)
		delete _bindata;
	if(_filehandle!=NULL)
		delete _filehandle;
}

int PackedBinFile::retrieve(int64 index) const
{	
	if(index < 0 || index >= _index_max)
		REPORT("invalid parameters to retrieve()");

	const int nbits = bitsperbin(_bin_max);
	const int binsperword = 64 / nbits;
	uint64 word;

	if(_preloaded)
		word = (*_bindata)[index / binsperword];
	else
	{
		_filehandle->Seek((index / binsperword) * 8);
		word = _filehandle->Read<uint64>();
	}

	word>>=nbits*(index%binsperword);
	word&=~(0xffffffffffffffffULL<<nbits);
	if(word >= (unsigned int) _bin_max) 
		REPORT("invalid bin file.");
	return (int)word;
}


//takes a pointer to the base of an array of bin data, and an offset i. it 
//stores bin in that position, packed as tightly as it can given the max value
//of all the bin values is bin_max-1. companion to the above function.
//used by HandSSCalculator routines only to store data.
void PackedBinFile::store(int64 index, int bin_num)
{
	if(!_preloaded)
		REPORT("You cannot store() to a non-preloaded BinFile");
	if(index<0 || index>=_index_max ||  bin_num<0 || bin_num>=_bin_max)
		REPORT("invalid parameters to store( " + tostr( index ) + ", " + tostr( bin_num ) + " )");

	// For a while, I only incremented the count _haveseen[ index ] when the count was 0 or when the number 
	// being stored was different than the one already stored. But the nesting algorithm in my binning
	// code (in HandSSCalculator) relied on it to increment no matter what, so I preserve that here now.
	if( _haveseen )//&& ( ( * _haveseen )[ index ] == 0 || bin_num != retrieve( index ) ) )
		if( ( * _haveseen )[ index ]++ == 0xFF )
			REPORT("you have stored to index "+tostring(index)+" over 255 times! _haveseen counter overflowed.");

	const int nbits = bitsperbin(_bin_max);

	// position is 0-offset position of the value we're looking for
	// in units of nbits

	const int binsperword = 64/nbits;
	const int position = index%binsperword;

	//worked on windows, not so much on linux. reference:
	//http://coding.derkeiler.com/Archive/C_CPP/comp.lang.c/2008-03/msg05323.html
	uint64 zeromask = (nbits*(position+1)>=64) ? 0 : 0xffffffffffffffffULL<<(nbits*(position+1));
	zeromask |= ~(0xffffffffffffffffULL<<(nbits*position));
	(*_bindata)[index/binsperword] &= zeromask; //zeroes out the old value

	uint64 bin_bits = bin_num;
	(*_bindata)[index/binsperword] |= bin_bits<<(nbits*position); //or's in the new value
}

int PackedBinFile::isstored(int64 index) const
{
	if(_haveseen == NULL)
		REPORT("this functionality is only meaningful when the PackedBinFile is NOT opened from a file");
	return (int)(*_haveseen)[index];
}

string PackedBinFile::save(string filename) const
{
	filename+=EXT; //tack on extension

	if(!_preloaded)
		REPORT("can't save unless you're preloaded!");

	vector<int64> multiplicity_count(256, 0);
	vector<int64> first_index(256, -1);
	for(int64 i=0; i<_index_max; i++)
		if(multiplicity_count[(*_haveseen)[i]]++ == 0)
			first_index[(*_haveseen)[i]] = i;

	string ret = "Saving "+tostring(filename)+". Counts of multiplicities of times each index was stored:\n";
	for(int i=0; i<256; i++)
		if(multiplicity_count[i] != 0)
			ret += "   "+tostring(i)+": "+tostring(multiplicity_count[i])+" times ("
					+(multiplicity_count[i] == _index_max ? "ALL)" : tostring((double)100.0*multiplicity_count[i]/_index_max)+"%)")
					+(i!=1 ? " (first index: "+tostring(first_index[i])+")\n" : "\n");
	ret += '\n';

	ofstream myfile(filename.c_str(), ios::binary);
	for(uint64 i=0; i<_bindata->size(); i++)
	{
		uint64 temp;
		temp = (*_bindata)[i];
		myfile.write((char*)&temp, 8);
	}

	if(!myfile.good() || (int64)myfile.tellp() != numwordsneeded(_bin_max, _index_max)*8)
		REPORT("error saving file... and return string was:\n\n"+ret);

	myfile.close();

	return ret;
}

//takes the total number of bins, bin_max, and returns the minimum number
//of bits it takes to represent numbers in the range 0..bin_max-1. 
//we shift out bin_max-1 until no bits are left, counting how many times it takes.
//used by all the below functions.
int PackedBinFile::bitsperbin(int bin_max)
{
	//easiest way to allow the special case that requires in fact no bin files by creating bin files  for it
	if(bin_max == 1)
		return 1;

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
//size, returns the size, in 8-byte words, of a file needed to store that many bin 
//numbers. it calculates the file size by calculating how many bin numbers
//store and retrieve will pack into each uint64.
int64 PackedBinFile::numwordsneeded(int bin_max, int64 index_max)
{
	//compute the number of bins we can fit in a 64-bit word

	int binsperword = 64/bitsperbin(bin_max);

	//compute the number of bytes that needs.

	if(index_max%binsperword==0)
		return (index_max/binsperword);
	else
		return (index_max/binsperword + 1);
}
