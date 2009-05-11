#ifndef _memorymgr_h_
#define _memorymgr_h_

extern inline void getpointers(int sceni, int beti, int maxa, int walkeri,
				 float * &stratt, float * &stratn, float * &stratd, float * &regret);

void initmem();
void closemem();

#endif