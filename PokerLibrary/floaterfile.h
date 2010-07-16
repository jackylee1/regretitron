#ifndef __floater_file_h__
#define __floater_file_h__

#include "../utility.h"
#include <string>
#include <vector>
using std::string;
using std::vector;

#define FLOATER double
typedef FLOATER floater;

class FloaterFile //represents a file of floater type between 0 and 1 inclusive
{
public:
	FloaterFile(string filename, const int64 n_floaters, const bool preload = true); //loads floaters from file
	FloaterFile(const int64 n_floaters) : mydata(new vector<floater>(n_floaters, -1)), filehandle(NULL) {}
	~FloaterFile();
	inline floater operator[](uint64 index) const { return (*mydata)[index]; }
	floater get(int64 index);
	void store(int64 index, floater value);
	void savefile(string filename);
	static string gettypename() { return TYPESTR; }
private:
	FloaterFile(const FloaterFile& rhs);
	FloaterFile& operator=(const FloaterFile& rhs);

	static bool broken; //tracks wether the floater files can be found anywhere
	vector<floater> * mydata;
	InFile * filehandle;
	static const string EXT;
	static const string FOLDER;
	static const string TYPESTR;
};

#endif
