#define REPORT(chars) { \
	std::cout << chars << std::endl; \
	_asm {int 3}; \
	system("PAUSE"); \
	exit(1); \
}

#define PRINTMASK(c) GenericDeck_maskString(&StdDeck, &c)

#define BENCH(c,n) do{ clock_t __clocker = clock()-c; \
	std::cout << n << "iterations completed in " \
		<< float(__clocker)/CLOCKS_PER_SEC << " seconds. (" \
		<< (float) n * CLOCKS_PER_SEC / (float) __clocker << " iterations per second)" << endl; \
	c = clock(); \
	}while(0);
