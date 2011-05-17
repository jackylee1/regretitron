#pragma once

#include "../Reconciler/reconcilerhelper.h"
#include "database.h"

class SessionReconciler : public ReconcilerHelper
{
public:
	SessionReconciler( uint64 sessionid ) 
		: m_database( MYSQL_CONNECT_STRING )
		, m_sessionid( sessionid )
	{ }
private:
	virtual void OnError( const std::string & errormsg ) 
	{ 
		string fullmessage = "During session #" + tostr( m_sessionid ) 
			+ " reconciliation: \"" + errormsg + "\"";
		otl_stream errinserter( 50, "insert into errorlog( id, message ) values ( NULL, :msg<char[32000]> )", m_database );
		errinserter << fullmessage;
	}

	otl_connect m_database;
	uint64 m_sessionid;
};
