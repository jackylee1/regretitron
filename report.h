#define REPORT(chars) { \
	std::cout << chars << std::endl; \
	_asm {int 3}; \
	system("PAUSE"); \
	exit(1); \
}


#define printmask(c) GenericDeck_maskString(&StdDeck, &c)
