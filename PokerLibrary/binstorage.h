#ifndef __binstorage_h__
#define __binstorage_h__

#include <string>
using std::string;
#include <fstream>
using std::ifstream;
#include <vector>
using std::vector;
#include "../utility.h"

class PackedBinFile
{
public:
	//opens an existing bin file for reading (hasfile = true)
	//filesize = -1 disables error checking
	PackedBinFile(string filename, int64 filesize, int bin_max, int64 index_max, bool preload);
	//creates a bin file in memory for writing, and then saving (hasfile = false, preload = true)
	PackedBinFile(int bin_max, int64 index_max);
	~PackedBinFile();

	int retrieve(int64 index) const;
	void store(int64 index, int bin_num);
	int isstored(int64 index) const; //returns number of times that index has been stored
	string save(string filename) const; //returns pretty string reporting how much you over-stored.
	int64 getindexmax() { return _index_max; }

	//utility function
	static int64 numwordsneeded(int bin_max, int64 index_max);

private:
	PackedBinFile(const PackedBinFile& rhs);
	PackedBinFile& operator=(const PackedBinFile& rhs);

	static int bitsperbin(int bin_max);

	vector<uint64> * _bindata;
	vector<unsigned char> * _haveseen;
	ifstream * _filehandle;
	const int64 _index_max;
	const int _bin_max;
	const bool _preloaded;
	static const string EXT;
};

#endif
