#define REPORT(chars) { \
	std::cout << chars << std::endl; \
	system("PAUSE"); \
	exit(1); \
}


#define printmask(c) GenericDeck_maskString(&StdDeck, &c)
