CPP := g++# for C++ code
CC := gcc# for C code
# compiler flags:
CFLAGS := -Wall -DTIXML_USE_TICPP -march=native -Wno-deprecated
# linker flags:
# define library paths in addition to /usr/lib with -L
# define any libraries to link into executable with -l
LFLAGS := -lpthread -lboost_regex -lboost_system -lboost_thread -lboost_filesystem -lodbc -rdynamic -ggdb -march=native

ifeq ($(PEDANTIC),y)
   CPP += -ansi -pedantic -Wextra -Wno-long-long
endif

# define the output executable file
# OUT is used by clean. MYOUT is used for compiling to.
OUT1 := bin/playoff
OUT1D := bin/playoffd
OUT1P := bin/playoffp

OUT3 := bin/botserver
OUT3D := bin/botserverd
OUT3P := bin/botserverp

ifeq ($(DEBUG),y)
   CFLAGS += -ggdb -D_GLIBCXX_DEBUG
   MYOUT1 := $(OUT1D)
   MYOUT3 := $(OUT3D)
   CFGDIR := debug
else 
ifeq ($(PROFILE),y)
   CFLAGS += -O3 -pg -ggdb
   LFLAGS += -pg
   MYOUT1 := $(OUT1P)
   MYOUT3 := $(OUT3P)
   CFGDIR := profile
else
   CFLAGS += -O3 -ggdb
   MYOUT1 := $(OUT1)
   MYOUT3 := $(OUT3)
   CFGDIR := release
endif
endif


# so that I do not have to change the source of pokereval
INCLUDES = -Ipokereval/include

# file that includes dependencies from makedepend
DEPFILE = .depends

# define the object files and sources
SRCPE := $(wildcard pokereval/lib/*.c)

SRC1 := $(wildcard PokerPlayerMFC/*.cpp PokerLibrary/*.cpp TinyXML++/*.cpp Playoff/*.cpp HandIndexingV1/*.cpp) utility.cpp
OBJ1 := $(SRC1:%.cpp=$(CFGDIR)/%.o)
OBJ1 += $(SRCPE:%.c=$(CFGDIR)/%.o) 

SRC3 := $(wildcard PokerPlayerMFC/*.cpp PokerLibrary/*.cpp TinyXML++/*.cpp Reconciler/*.cpp NetworkServer/*.cpp HandIndexingV1/*.cpp) utility.cpp
OBJ3 := $(SRC3:%.cpp=$(CFGDIR)/%.o)
OBJ3 += $(SRCPE:%.c=$(CFGDIR)/%.o) 

.PHONY:	clean all init playoff reconciler botserver

init:
	@mkdir -p $(CFGDIR)/Playoff
	@mkdir -p $(CFGDIR)/Reconciler
	@mkdir -p $(CFGDIR)/PokerPlayerMFC
	@mkdir -p $(CFGDIR)/PokerLibrary
	@mkdir -p $(CFGDIR)/HandIndexingV1
	@mkdir -p $(CFGDIR)/NetworkServer
	@mkdir -p $(CFGDIR)/pokereval/lib
	@mkdir -p $(CFGDIR)/TinyXML++
	@find $(CFGDIR)/TinyXML++ -name "*.o" -delete
	@rm -f $(DEPFILE)
	@touch $(DEPFILE)
	@makedepend -Y -p$(CFGDIR)/ -f$(DEPFILE) -- $(CFLAGS) -- $(INCLUDES) $(SRC1) $(SRC3) $(SRCPE) 2>/dev/null
	@touch stdafx.h
	@$(MAKE) -f playoff.mk all -r --no-print-directory
	@find $(CFGDIR)/TinyXML++ -name "*.o" -delete
	@rm -f $(DEPFILE) $(DEPFILE).bak stdafx.h

all: playoff botserver

playoff: $(MYOUT1)

botserver: $(MYOUT3)

clean:
	@echo "deleting crap..."
	@find . -name "*.o" -delete
	@rm -f $(OUT1) $(OUT1D) $(OUT1P) $(OUT3) $(OUT3D) $(OUT3P) $(DEPFILE) $(DEPFILE).bak stdafx.h

$(MYOUT1): $(OBJ1)
	@echo "$(CPP) (object files) $(LFLAGS) -o $@"
	@$(CPP) $(OBJ1) $(LFLAGS) -o $@

$(MYOUT3): $(OBJ3)
	@echo "$(CPP) (object files) $(LFLAGS) -o $@"
	@$(CPP) $(OBJ3) $(LFLAGS) -o $@

$(CFGDIR)/%.o : %.cpp
	@echo "$(CPP) $(CFLAGS) $(INCLUDES) ($<)"
	@$(CPP) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(CFGDIR)/%.o : %.c
	@echo "$(CC) $(CFLAGS) $(INCLUDES) ($<)"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

sinclude $(DEPFILE)

