CPP := g++# for C++ code
CC := gcc# for C code
CFLAGS := -Wall

ifeq ($(DEBUG),y)
   CFLAGS += -g
   CFGFLAG := d
   CFGDIR := debug
else
   CFLAGS += -O3
   CFGFLAG :=
   CFGDIR := release
endif

# define library paths in addition to /usr/lib
LFLAGS := -Llib
# define any libraries to link into executable:
LFLAGS += -lticpp$(CFGFLAG) -lpthread

# define the output executable file
# OUT is used by clean. MYOUT is used for compiling to.
OUT := bin/PokerNoLimit
MYOUT := $(OUT)$(CFGFLAG)

# so that I do not have to change the source of pokereval
INCLUDES = -Ipokereval/include

# file that includes dependencies from gcc
DEPFILE = .depends

# define the object and depends files
SRC1 := $(wildcard PokerLibrary/*.cpp HandIndexingV1/*.cpp PokerNoLimit/*.cpp)
SRC1 += mymersenne.cpp
SRC2 := $(wildcard pokereval/lib/*.c)
OBJ := $(SRC1:%.cpp=$(CFGDIR)/%.o)
OBJ += $(SRC2:%.c=$(CFGDIR)/%.o) 

.PHONY:	clean all init

all:
	@mkdir -p $(CFGDIR)/PokerLibrary
	@mkdir -p $(CFGDIR)/PokerNoLimit
	@mkdir -p $(CFGDIR)/HandIndexingV1
	@mkdir -p $(CFGDIR)/pokereval/lib
	@rm -f stdafx.h $(DEPFILE)
	@touch $(DEPFILE)
	@makedepend -Y -p$(CFGDIR)/ -f$(DEPFILE) -- $(CFLAGS) -- $(INCLUDES) $(SRC1) $(SRC2) 2>/dev/null
	@touch stdafx.h
	@$(MAKE) $(MYOUT) -r --no-print-directory
	@rm -f stdafx.h $(DEPFILE) $(DEPFILE).bak

clean:
	@echo "deleting crap..."
	@find . -name "*.o" -exec rm {} \;
	@rm -f $(OUT) $(OUT)d stdafx.h $(DEPFILE) $(DEPFILE).bak

$(MYOUT): $(OBJ)
	@echo "$(CPP) (object files) $(LFLAGS) -o $@"
	@$(CPP) $(OBJ) $(LFLAGS) -o $@

$(CFGDIR)/%.o : %.cpp
	$(CPP) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(CFGDIR)/%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

sinclude $(DEPFILE)

