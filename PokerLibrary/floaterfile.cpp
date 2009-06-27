#include "stdafx.h"
#include "floaterfile.h"
using namespace std;

const string FloaterFile::TYPESTR = TOSTRING(FLOATER);
const string FloaterFile::EXT = "."+TYPESTR;
bool FloaterFile::broken = false;

//define the location of the handstrength data on different machines
#ifdef __GNUC__ //my linux machine
const string FloaterFile::FOLDER = "/home/scott/pokerbreedinggrounds/bin/handstrengthdata/";
#elif defined _WIN32 //my windows machine
const string FloaterFile::FOLDER = "Z:/pokerbreedinggrounds/bin/handstrengthdata/";
#else
#error where is floater?
#endif

FloaterFile::FloaterFile(string filename, const int64 n_floaters, const bool preload)
	: mydata(NULL),
	  filehandle(NULL)
{
	if(broken) return;

	filename = FOLDER + filename + EXT;
	filehandle = new ifstream(filename.c_str(),ifstream::binary);
	if(!filehandle->is_open() || !filehandle->good())
	{
		broken = true;
		REPORT(filename + " doesn't exist or could not be opened", WARN);
		delete filehandle;
		filehandle = NULL;
		return;
	}
	filehandle->seekg(0, ios::end);
	if(!filehandle->good() || (uint64)filehandle->tellg()!=n_floaters*sizeof(floater))
		REPORT(filename + " found but not correct size");

	if(preload)
	{
		mydata = new vector<floater>(n_floaters, -1);
		filehandle->seekg(0, ios::beg);
		for(int64 i=0; i<n_floaters; i++)
		{
			floater temp;
			filehandle->read((char *)&temp, sizeof(floater));
			if(temp<0 || temp>1)
				REPORT(filename + " contained bad data!!");
			(*mydata)[i] = temp;
		}
		if(!filehandle->good() || filehandle->eof() || EOF!=filehandle->peek() || !filehandle->eof())
			REPORT(filename + " failed us at the last minute, don't know what happened.");
		filehandle->close();
		delete filehandle;
		filehandle = NULL;
	}
}

FloaterFile::~FloaterFile()
{
	if(mydata != NULL) 
		delete mydata;
	if(filehandle != NULL) 
		delete filehandle;
}

floater FloaterFile::get(int64 index)
{
	if(broken) return 0;
	if(filehandle == NULL) REPORT("use [] instead, but prolly something worse happened");
	filehandle->seekg(index*sizeof(floater));
	floater temp;
	filehandle->read((char*)&temp, sizeof(floater));
	if(!filehandle->good()) REPORT("the file failed on us");
	if(temp < 0 || temp > 1) REPORT("found bad data in floater file");
	return temp;
}

void FloaterFile::store(int64 index, floater value)
{
	if(mydata == NULL) REPORT("this thing does not support store()");
	if(index < 0 || index > (int64)mydata->size() || value < 0 || value > 1)
		REPORT("bad data trying to be put into a FloaterFile!");
	(*mydata)[index] = value;
}

void FloaterFile::savefile(string filename)
{
	if(mydata == NULL) REPORT("this thing does not support save()");
	filename = FOLDER + filename + EXT;
	ofstream f(filename.c_str(), ios::binary);
	if(!f.good() || !f.is_open())
		REPORT("failed to open "+filename+" for writing!");
	for(uint64 i=0; i<mydata->size(); i++)
	{
		floater temp = (*mydata)[i];
		if(temp < 0 || temp > 1)
			REPORT("you wanted to savefile on "+filename+" but the array contains bad data!");
		f.write((char*)&temp, sizeof(floater));
	}
	if(!f.good())
		REPORT(filename + " failed us at the last minute while saving, don't know what happened.");
	f.close();
}

