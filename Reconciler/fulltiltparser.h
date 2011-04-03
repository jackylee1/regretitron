#include "reconciler.h"
#include <iostream>

class FullTiltParser
{
public:
	FullTiltParser( Reconciler & reconciler )
		: m_reconciler( reconciler )
	{ }

	~FullTiltParser( )
	{ }

	void Parse( istream & input );

private:
	Reconciler & m_reconciler;

};

