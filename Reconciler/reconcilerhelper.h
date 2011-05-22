#pragma once

#include "../utility.h"
class BotAPI;

class ReconcilerHelper
{
public:
	enum FormatEnum
	{
		FORMAT_NONE,
		FORMAT_FULLTILT,
		FORMAT_MERGE
	};

	ReconcilerHelper( ) { }
	~ReconcilerHelper( ) { }

	static FormatEnum GetFileFormat( const boost::filesystem::path & filepath );
	void DoReconcile( const boost::filesystem::path & filepath, BotAPI & bot, const bool forceactions );

	//for errors that occur while reconciling hand histories
	virtual void OnError( const std::string & errormsg ) { }
};


