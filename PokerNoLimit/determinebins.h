#ifndef _determinebins_h_
#define _determinebins_h_

void initbins();

void determinebins(const CardMask &priv, const CardMask &flop, const CardMask &turn, const CardMask &river,
				   int &flopbin, int &turnbin, int &riverbin);

#endif