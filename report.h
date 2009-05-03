#define REPORT(chars) { \
	std::cout << chars << std::endl; \
	_asm {int 3}; \
	system("PAUSE"); \
	exit(1); \
}

#define PRINTMASK(c) GenericDeck_maskString(&StdDeck, &c)

#define BENCH(c) std::cout << float(clock()-c)/CLOCKS_PER_SEC << " seconds elapsed." << endl;
