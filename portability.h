#ifndef __PORTABILITY_H__
#define __PORTABILITY_H__


#ifdef _MSC_VER

#define ASMBRK _asm {int 3}
#define INT64(num) num##i64
#define UINT64(num) num##ui64
#define PAUSE() system("PAUSE")

#else //for gcc

#define ASMBRK __asm__("int $3")
#define INT64(num) num##LL
#define UINT64(num) num##LLU
#define PAUSE() do{ \
	std::cout << "Press [enter] to continue . . ."; \
	getchar(); \
}while(0)

#endif


#endif
