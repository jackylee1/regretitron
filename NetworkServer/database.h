
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define OTL_ODBC

#else
#define OTL_ODBC_UNIX
#endif

#define OTL_EXCEPTION_IS_DERIVED_FROM_STD_EXCEPTION
#define OTL_STL

#include "otlv4.h"

#define MYSQL_CONNECT_STRING "DSN=pokerdb"

#define SESSION_TABLE "pokersessions"

