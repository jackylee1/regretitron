#ifndef _determinebins_h_
#define _determinebins_h_

// The all essential indexing function.
#include "../HandIndexingV1/getindex2N.h"

void initbins();

extern inline void determinebins(const CardMask &priv, 
						  const CardMask &flop, const CardMask &turn, const CardMask &river,
						  int &preflopbin, int &flopbin, int &turnbin, int &riverbin);

#endif