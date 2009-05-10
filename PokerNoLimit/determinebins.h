#ifndef _determinebins_h_
#define _determinebins_h_

void initbins();
void closebins();

void determinebins(const CardMask &priv, 
						  const CardMask &flop, const CardMask &turn, const CardMask &river,
						  int &preflopbin, int &flopbin, int &turnbin, int &riverbin);

int getpreflopbin(const CardMask &priv);
int getflopbin(const CardMask &priv, const CardMask &flop);
int getturnbin(const CardMask &priv, const CardMask &flop, const CardMask &turn);
int getriverbin(const CardMask &priv, const CardMask &flop, const CardMask &turn, const CardMask &river);

#endif