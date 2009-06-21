CPP := g++# for C++ code
CC := gcc# for C code
# compiler flags:
CFLAGS := -Wall -DTIXML_USE_STL
# linker flags:
# define library paths in addition to /usr/lib with -L
# define any libraries to link into executable with -l
LFLAGS := -lpthread

ifeq ($(PEDANTIC),y)
   CPP += -ansi -pedantic -Wextra -Wno-long-long
endif

# define the output executable file
# OUT is used by clean. MYOUT is used for compiling to.
OUT1 := bin/solver
OUT1D := bin/solverd
OUT1P := bin/solverp

OUT2 := bin/binmaker
OUT2D := bin/binmakerd
OUT2P := bin/binmakerp

ifeq ($(DEBUG),y)
   CFLAGS += -ggdb -D_GLIBCXX_DEBUG
   MYOUT1 := $(OUT1D)
   MYOUT2 := $(OUT2D)
   CFGDIR := debug
else 
ifeq ($(PROFILE),y)
   CFLAGS += -O3 -pg
   LFLAGS += -pg
   MYOUT1 := $(OUT1P)
   MYOUT2 := $(OUT2P)
   CFGDIR := profile
else
   CFLAGS += -O3
   MYOUT1 := $(OUT1)
   MYOUT2 := $(OUT2)
   CFGDIR := release
endif
endif


# so that I do not have to change the source of pokereval
INCLUDES = -Ipokereval/include

# file that includes dependencies from makedepend
DEPFILE = .depends

# define the object files and sources
SRCPE := $(wildcard pokereval/lib/*.c)

SRC1 := $(wildcard PokerLibrary/*.cpp HandIndexingV1/*.cpp PokerNoLimit/*.cpp TinyXML++/*.cpp)
SRC1 += utility.cpp
OBJ1 := $(SRC1:%.cpp=$(CFGDIR)/%.o)
OBJ1 += $(SRCPE:%.c=$(CFGDIR)/%.o) 

SRC2 := $(wildcard PokerLibrary/*.cpp HandSSCalculator/*.cpp HandIndexingV1/*.cpp)
SRC2 += utility.cpp
OBJ2 := $(SRC2:%.cpp=$(CFGDIR)/%.o)
OBJ2 += $(SRCPE:%.c=$(CFGDIR)/%.o) 

.PHONY:	clean all init solver binmaker

init:
	@mkdir -p $(CFGDIR)/HandSSCalculator
	@mkdir -p $(CFGDIR)/PokerLibrary
	@mkdir -p $(CFGDIR)/PokerNoLimit
	@mkdir -p $(CFGDIR)/HandIndexingV1
	@mkdir -p $(CFGDIR)/pokereval/lib
	@mkdir -p $(CFGDIR)/TinyXML++
	@rm -f $(DEPFILE)
	@touch $(DEPFILE)
	@makedepend -Y -p$(CFGDIR)/ -f$(DEPFILE) -- $(CFLAGS) -- $(INCLUDES) $(SRC1) $(SRC2) $(SRCPE) 2>/dev/null
	@touch stdafx.h
	@$(MAKE) all -r --no-print-directory
	@rm -f $(DEPFILE) $(DEPFILE).bak stdafx.h

all: solver binmaker

solver: $(MYOUT1)

binmaker: $(MYOUT2)

clean:
	@echo "deleting crap..."
	@find . -name "*.o" -exec rm {} \;
	@rm -f $(OUT1) $(OUT2) $(OUT1D) $(OUT2D) $(OUT2P) $(DEPFILE) $(DEPFILE).bak

$(MYOUT1): $(OBJ1)
	@echo "$(CPP) (object files) $(LFLAGS) -o $@"
	@$(CPP) $(OBJ1) $(LFLAGS) -o $@

$(MYOUT2): $(OBJ2)
	@echo "$(CPP) (object files) $(LFLAGS) -o $@"
	@$(CPP) $(OBJ2) $(LFLAGS) -o $@

$(CFGDIR)/%.o : %.cpp
	@echo "$(CPP) $(CFLAGS) $(INCLUDES) ($<)"
	@$(CPP) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(CFGDIR)/%.o : %.c
	@echo "$(CC) $(CFLAGS) $(INCLUDES) ($<)"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

sinclude $(DEPFILE)

