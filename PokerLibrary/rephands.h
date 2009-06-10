#ifndef __rephands_h_
#define __rephands_h_

//forward declaration
struct betnode;

//these find random hands matching the given sub-indices (like handi and boardi)
//used by the diagnostic window (in PokerPlayer) and printxxxxhand (in this file).
CardMask findpreflophand(int index);
void findflophand(int bin, int flopscore, CardMask &priv, CardMask &flop);
void findturnhand(int bin, int turnscore, CardMask &priv, CardMask &flop, CardMask &turn);
void findriverhand(int bin, int riverscore, CardMask &priv, CardMask &flop, CardMask &turn, CardMask &river);
//used by printsceniinfo and by savestrategy.cpp (in PokerNoLimit)
std::string preflophandstring(int index);

//used by diagnostics window (in PokerPlayer) and local functions (in this file)
void getindices(int sceni, int &gr, int &poti, int &boardi, int &handi, int bethist[3]);
void resolvepoti(int gr, int poti, int &lower, int &upper);
std::string bethiststr(int bethist);

//used only by printfirstnodestrat in savestrategy.cpp (in PokerNoLimit)
void printsceniinfo(std::ostream &out, int sceni, int n_hands);

//these are used by PokerPlayer and SimpleGame
int roundtobb(double pot);
void decomposecm(CardMask in, CardMask out[]);
std::string cardfilename(CardMask m);

#endif
