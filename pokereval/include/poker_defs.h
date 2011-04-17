/*
 * Copyright (C) 1999-2006 
 *           Brian Goetz <brian@quiotix.com>
 *           Loic Dachary <loic@dachary.org>
 *           Igor Kravtchenko <igor@obraz.net>
 *
 *  This package is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 dated June, 1991.
 *
 *  This package is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this package; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */
#ifndef __POKER_DEFS_H__
#define __POKER_DEFS_H__


/* Compiler-specific junk */

#if defined(_MSC_VER)
#  define UINT64_TYPE unsigned __int64
#  define inline __inline
#  define POKEREVAL_DEFINE_THREAD __declspec( thread )
#else
#  define POKEREVAL_DEFINE_THREAD 
#  include "poker_config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#else
#  ifdef HAVE_STDINT_H
#    include <stdint.h>
#  endif
#endif

/* 64-bit integer junk */

#ifdef _MSC_VER
typedef __int64 int64;
typedef unsigned __int64 uint64;
typedef __int32 int32;
typedef unsigned __int32 uint32;
typedef __int16 int16;
typedef unsigned __int16 uint16;
typedef __int8 int8;
typedef unsigned __int8 uint8;
#elif __GNUC__ 
#include <stdint.h>
typedef int64_t int64;
typedef uint64_t uint64;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int8_t int8;
typedef uint8_t uint8;
#endif

#define USE_INT64 1


#ifdef USE_INT64
#define LongLong_OP(result, op1, op2, operation) \
  do { result = (op1) operation (op2); } while (0)
#define LongLong_OR(result, op1, op2)  LongLong_OP(result, op1, op2, |)
#define LongLong_AND(result, op1, op2) LongLong_OP(result, op1, op2, &)
#define LongLong_XOR(result, op1, op2) LongLong_OP(result, op1, op2, ^)
#endif


#include "deck.h"
#include "handval.h"
//#include "handval_low.h"
#include "enumerate.h"

#include "game_std.h"

#endif

