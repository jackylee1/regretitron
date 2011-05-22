#include "reconciler.h"
#include <iostream>

class MergeParser
{
public:
	MergeParser( Reconciler & reconciler )
		: m_reconciler( reconciler )
	{ }

	~MergeParser( )
	{ }

	void Parse( const boost::filesystem::path & );

private:
	Reconciler & m_reconciler;

};

