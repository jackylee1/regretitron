#ifdef _MSC_VER

#define ASMBRK _asm {int 3}
#define INT64(num) num##i64
#define UINT64(num) num##ui64

#else //for gcc

#define ASMBRK __asm__("int $3")
#define INT64(num) num##LL
#define UINT64(num) num##LLU

#endif

