CPP := g++# for C++ code
CC := gcc# for C code
# compiler flags:
CFLAGS := -Wall -DTIXML_USE_TICPP -march=native -Wno-deprecated
# linker flags:
# define library paths in addition to /usr/lib with -L
# define any libraries to link into executable with -l
LFLAGS := -lpthread -rdynamic -ggdb -march=native

ifeq ($(PEDANTIC),y)
   CPP += -ansi -pedantic -Wextra -Wno-long-long
endif

# define the output executable file
# OUT is used by clean. MYOUT is used for compiling to.
OUT := bin/playoff
OUTD := bin/playoffd
OUTP := bin/playoffp

ifeq ($(DEBUG),y)
   CFLAGS += -ggdb -D_GLIBCXX_DEBUG
   MYOUT := $(OUTD)
   CFGDIR := debug
else 
ifeq ($(PROFILE),y)
   CFLAGS += -O3 -pg -ggdb
   LFLAGS += -pg
   MYOUT := $(OUTP)
   CFGDIR := profile
else
   CFLAGS += -O3 -ggdb
   MYOUT := $(OUT)
   CFGDIR := release
endif
endif


# so that I do not have to change the source of pokereval
INCLUDES = -Ipokereval/include

# file that includes dependencies from makedepend
DEPFILE = .depends

# define the object files and sources
SRCPE := $(wildcard pokereval/lib/*.c)

SRC := $(wildcard PokerPlayerMFC/*.cpp PokerLibrary/*.cpp TinyXML++/*.cpp Playoff/*.cpp HandIndexingV1/*.cpp)
SRC += utility.cpp
OBJ := $(SRC:%.cpp=$(CFGDIR)/%.o)
OBJ += $(SRCPE:%.c=$(CFGDIR)/%.o) 

.PHONY:	clean all init solver binmaker

init:
	@mkdir -p $(CFGDIR)/Playoff
	@mkdir -p $(CFGDIR)/PokerPlayerMFC
	@mkdir -p $(CFGDIR)/HandSSCalculator
	@mkdir -p $(CFGDIR)/PokerLibrary
	@mkdir -p $(CFGDIR)/PokerNoLimit
	@mkdir -p $(CFGDIR)/HandIndexingV1
	@mkdir -p $(CFGDIR)/pokereval/lib
	@mkdir -p $(CFGDIR)/TinyXML++
	@find $(CFGDIR)/TinyXML++ -name "*.o" -exec rm {} \;
	@rm -f $(DEPFILE)
	@touch $(DEPFILE)
	@makedepend -Y -p$(CFGDIR)/ -f$(DEPFILE) -- $(CFLAGS) -- $(INCLUDES) $(SRC) $(SRCPE) 2>/dev/null
	@touch stdafx.h
	@$(MAKE) -f playoff.mk all -r --no-print-directory
	@find $(CFGDIR)/TinyXML++ -name "*.o" -exec rm {} \;
	@rm -f $(DEPFILE) $(DEPFILE).bak stdafx.h

all: playoff

playoff: $(MYOUT)

clean:
	@echo "deleting crap..."
	@find . -name "*.o" -exec rm {} \;
	@rm -f $(OUT) $(OUTD) $(OUTP) $(DEPFILE) $(DEPFILE).bak

$(MYOUT): $(OBJ)
	@echo "$(CPP) (object files) $(LFLAGS) -o $@"
	@$(CPP) $(OBJ) $(LFLAGS) -o $@

$(CFGDIR)/%.o : %.cpp
	@echo "$(CPP) $(CFLAGS) $(INCLUDES) ($<)"
	@$(CPP) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(CFGDIR)/%.o : %.c
	@echo "$(CC) $(CFLAGS) $(INCLUDES) ($<)"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

sinclude $(DEPFILE)

